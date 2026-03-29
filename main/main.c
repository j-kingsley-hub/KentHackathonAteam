#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp_board.h"
#include "lv_port.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "usb_camera.h"
#include "gemini_client.h"
#include "bsp_btn.h"
#include <stdlib.h>
#include <string.h>

#include "indicator_model.h"
#include "indicator_view.h"
// #include "indicator_controller.h"
// Declare the external image structs
extern const lv_img_dsc_t dogprototype_img;
extern const lv_img_dsc_t dogprototype3_img;
extern const lv_img_dsc_t dogTalking_img;
extern const uint8_t dinotest1_jpg[];
extern const size_t dinotest1_jpg_size;
extern const uint8_t cavemantest_jpg[];
extern const size_t cavemantest_jpg_size;
extern const uint8_t mammothtest_jpg[];
extern const size_t mammothtest_jpg_size;

ESP_EVENT_DEFINE_BASE(VIEW_EVENT_BASE);
esp_event_loop_handle_t view_event_handle;
lv_obj_t *ui_dog_label = NULL;

static const char *TAG = "app_main";

#define VERSION "v1.1.0"

#define SENSECAP "\n\
   _____                      _________    ____         \n\
  / ___/___  ____  ________  / ____/   |  / __ \\       \n\
  \\__ \\/ _ \\/ __ \\/ ___/ _ \\/ /   / /| | / /_/ /   \n\
 ___/ /  __/ / / (__  )  __/ /___/ ___ |/ ____/         \n\
/____/\\___/_/ /_/____/\\___/\\____/_/  |_/_/           \n\
--------------------------------------------------------\n\
 Version: %s %s %s\n\
--------------------------------------------------------\n\
"
lv_obj_t *ui_image = NULL;
lv_obj_t *ui_label = NULL;
lv_obj_t *ui_selection_box = NULL;
lv_obj_t *ui_selection_label = NULL;
static lv_timer_t *image_toggle_timer = NULL;
static bool button_showing_talking = false;
static const lv_img_dsc_t *ui_images[] = {
    &dogprototype_img,
    &dogprototype3_img,
};
static uint8_t ui_image_index = 0;

typedef enum
{
    IMAGE_SELECTION_DINO = 0,
    IMAGE_SELECTION_CAVEMAN,
    IMAGE_SELECTION_MAMMOTH,
    IMAGE_SELECTION_COUNT,
} image_selection_t;

static image_selection_t current_selection = IMAGE_SELECTION_DINO;
static const char *selection_names[IMAGE_SELECTION_COUNT] = {
    "DinoTest1.jpg",
    "CaveManTest.jpg",
    "MammothTest.jpg",
};
static const uint8_t *selection_buffers[IMAGE_SELECTION_COUNT] = {NULL};
static size_t selection_sizes[IMAGE_SELECTION_COUNT] = {0};

static void update_selection_label(void)
{
    if (ui_selection_label)
    {
        char text[64];
        snprintf(text, sizeof(text), "Selected: %s", selection_names[current_selection]);
        lv_label_set_text(ui_selection_label, text);
    }
}

static void image_toggle_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (button_showing_talking)
    {
        button_showing_talking = false;
        ui_image_index = 0;
        lv_img_set_src(ui_image, ui_images[ui_image_index]);
        return;
    }

    ui_image_index = 1 - ui_image_index;
    lv_img_set_src(ui_image, ui_images[ui_image_index]);
}

static void button_single_click_cb(void *arg)
{
    (void)arg;
    const char *selected_name = selection_names[current_selection];
    ESP_LOGI(TAG, "Button pressed! Sending selected image: %s", selected_name);

    lv_port_sem_take();
    if (ui_image == NULL)
    {
        ESP_LOGW(TAG, "Button pressed before UI image was ready");
    }
    else
    {
        lv_img_set_src(ui_image, &dogTalking_img);
        button_showing_talking = true;
        if (image_toggle_timer != NULL)
        {
            lv_timer_reset(image_toggle_timer);
        }
    }
    if (ui_label)
    {
        char text[80];
        snprintf(text, sizeof(text), "Sending %s to Gemini...", selected_name);
        lv_label_set_text(ui_label, text);
    }
    lv_port_sem_give();

    uint8_t *img_buf = (uint8_t *)selection_buffers[current_selection];
    size_t img_size = selection_sizes[current_selection];
    if (img_buf == NULL || img_size == 0)
    {
        ESP_LOGE(TAG, "Selected image payload missing: %s", selected_name);
        update_ui("Selected image missing.", "#800000");
        return;
    }

    gemini_client_send_image(img_buf, img_size);
}

static void button_double_click_cb(void *arg)
{
    (void)arg;
    current_selection = (current_selection + 1) % IMAGE_SELECTION_COUNT;
    ESP_LOGI(TAG, "Image selection changed to %s", selection_names[current_selection]);
    if (ui_selection_label)
    {
        lv_port_sem_take();
        update_selection_label();
        lv_port_sem_give();
    }
}

void update_ui(const char *voice_line, const char *color_hex_str)
{
    lv_port_sem_take();
    lv_obj_t *scr = lv_scr_act();

    int color_val = 0;
    if (color_hex_str != NULL && strlen(color_hex_str) >= 6)
    {
        if (color_hex_str[0] == '#')
            color_hex_str++;
        color_val = (int)strtol(color_hex_str, NULL, 16);
    }

    lv_obj_set_style_bg_color(scr, lv_color_hex(color_val), 0);

    if (ui_label)
    {
        lv_label_set_text(ui_label, voice_line ? voice_line : "Unknown Dog noise");
    }
    lv_port_sem_give();
}

void app_main(void)
{
    ESP_LOGI("", SENSECAP, VERSION, __DATE__, __TIME__);

    ESP_ERROR_CHECK(bsp_board_init());

    // Camera initialization is not required for button-triggered static image sends
    // usb_camera_init();
    gemini_client_init();

    lv_port_init();

    esp_event_loop_args_t view_event_task_args = {
        .queue_size = 10,
        .task_name = "view_event_task",
        .task_priority = uxTaskPriorityGet(NULL),
        .task_stack_size = 10240,
        .task_core_id = tskNO_AFFINITY};

    ESP_ERROR_CHECK(esp_event_loop_create(&view_event_task_args, &view_event_handle));

    lv_port_sem_take();
    indicator_view_init();

    selection_buffers[IMAGE_SELECTION_DINO] = dinotest1_jpg;
    selection_sizes[IMAGE_SELECTION_DINO] = dinotest1_jpg_size;
    selection_buffers[IMAGE_SELECTION_CAVEMAN] = cavemantest_jpg;
    selection_sizes[IMAGE_SELECTION_CAVEMAN] = cavemantest_jpg_size;
    selection_buffers[IMAGE_SELECTION_MAMMOTH] = mammothtest_jpg;
    selection_sizes[IMAGE_SELECTION_MAMMOTH] = mammothtest_jpg_size;

    // The view should have allocated ui_image and ui_label on ui_screen_openai
    // We just need to start the timer!
    if (ui_image != NULL)
    {
        lv_img_set_src(ui_image, ui_images[ui_image_index]);
    }
    image_toggle_timer = lv_timer_create(image_toggle_timer_cb, 500, NULL);
    update_selection_label();

    lv_port_sem_give();

    indicator_model_init();
    indicator_controller_init();

    // Initialize Onboard User Button (GPIO 38, top of the SenseCAP Indicator)
    // We register this AFTER indicator_model_init() so it overrides the default screen toggle
    bsp_btn_register_callback(BOARD_BTN_ID_USER, BUTTON_SINGLE_CLICK, button_single_click_cb, NULL);
    bsp_btn_register_callback(BOARD_BTN_ID_USER, BUTTON_DOUBLE_CLICK, button_double_click_cb, NULL);

    static char buffer[128]; /* Make sure buffer is enough for `sprintf` */
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
