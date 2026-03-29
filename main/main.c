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
extern const lv_img_dsc_t dog_idle_1_img;
extern const lv_img_dsc_t dog_idle_2_img;
extern const lv_img_dsc_t dog_talking_1_img;
extern const lv_img_dsc_t dog_talking_2_img;
extern const lv_img_dsc_t dog_scared_1_img;
extern const lv_img_dsc_t dog_scared_2_img;
extern const uint8_t magicrabit_jpg[];
extern const size_t magicrabit_jpg_size;
extern const uint8_t dinotest3_jpg[];
extern const size_t dinotest3_jpg_size;
extern const uint8_t cavemantest_jpg[];
extern const size_t cavemantest_jpg_size;
extern const uint8_t mammothtest_jpg[];
extern const size_t mammothtest_jpg_size;
extern const uint8_t asteroid_death_jpg[];
extern const size_t asteroid_death_jpg_size;

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

#include <stdbool.h>

lv_obj_t *ui_blind_image = NULL;
lv_obj_t *ui_blind_label = NULL;
lv_obj_t *ui_blind_response_box = NULL;
lv_obj_t *ui_blind_selection_box = NULL;
lv_obj_t *ui_blind_selection_label = NULL;
lv_obj_t *ui_blind_selection_thumb = NULL;

static int current_blind_selection = 0;
extern const lv_img_dsc_t img_bridge;
extern const lv_img_dsc_t img_stairs;
const lv_img_dsc_t *blind_images[] = {&img_bridge, &img_stairs};

extern const lv_img_dsc_t img_bridge_thumb;
extern const lv_img_dsc_t img_stairs_thumb;
const lv_img_dsc_t *blind_thumbs[] = {&img_bridge_thumb, &img_stairs_thumb};

const char* blind_selection_names[] = {"Bridge", "Stairs"};

extern const uint8_t bridge_jpg[];
extern const unsigned int bridge_jpg_size;
extern const uint8_t stairs_jpg[];
extern const unsigned int stairs_jpg_size;
const uint8_t *blind_selection_buffers[] = { bridge_jpg, stairs_jpg };

// We define a function to get sizes dynamically, or we use a trick:
size_t get_blind_size(int index) {
    if (index == 0) return bridge_jpg_size;
    return stairs_jpg_size;
}


void update_blind_ui(const char *text, const char *color_hex_str)
{
    ESP_LOGI(TAG, "Updating blind UI: %s", text);
    lv_port_sem_take();
    if (ui_blind_label != NULL) {
        lv_label_set_text(ui_blind_label, text);
    }
    if (ui_blind_response_box != NULL && color_hex_str != NULL) {
        uint32_t color_val = strtol(color_hex_str + 1, NULL, 16);
        lv_obj_set_style_bg_color(ui_blind_response_box, lv_color_hex(color_val), 0);
    }
    lv_port_sem_give();
}

static void blind_button_single_click_cb(void *arg)
{
    const char *selected_name = blind_selection_names[current_blind_selection];
    ESP_LOGI(TAG, "Blind analysis: %s", selected_name);
    lv_port_sem_take();
    if (ui_blind_label) {
        char text[80];
        snprintf(text, sizeof(text), "Analyzing %s...", selected_name);
        lv_label_set_text(ui_blind_label, text);
    }
    lv_port_sem_give();

    const uint8_t *img_buf = blind_selection_buffers[current_blind_selection];
    size_t img_size = get_blind_size(current_blind_selection);
    gemini_client_send_image((uint8_t*)img_buf, img_size, true);
}

static void lvgl_send_blind_task(void *arg)
{
    blind_button_single_click_cb(NULL);
    vTaskDelete(NULL);
}

static void blind_image_clicked_cb(lv_event_t *e)
{
    xTaskCreate(lvgl_send_blind_task, "send_blind", 8192, NULL, 5, NULL);
}

static void blind_selection_box_clicked_cb(lv_event_t *e)
{
    current_blind_selection = (current_blind_selection + 1) % 2;
    if (ui_blind_image != NULL) {
        lv_img_set_src(ui_blind_image, blind_images[current_blind_selection]);
        if (ui_blind_selection_thumb != NULL) {
            lv_img_set_src(ui_blind_selection_thumb, blind_thumbs[current_blind_selection]);
        }
    }
}

lv_obj_t *ui_label = NULL;
lv_obj_t *ui_selection_box = NULL;
lv_obj_t *ui_selection_label = NULL;
lv_obj_t *ui_selection_thumb = NULL;
static lv_timer_t *image_toggle_timer = NULL;

typedef enum
{
    DOG_STATE_IDLE,
    DOG_STATE_TALKING,
    DOG_STATE_SCARED
} dog_state_t;

static dog_state_t current_dog_state = DOG_STATE_IDLE;
static uint8_t dog_anim_index = 0;
// We will trigger TALKING manually on single click, and it will revert to IDLE or SCARED after

static const lv_img_dsc_t *dog_idle_images[] = {&dog_idle_1_img, &dog_idle_2_img};
static const lv_img_dsc_t *dog_talking_images[] = {&dog_talking_1_img, &dog_talking_2_img};
static const lv_img_dsc_t *dog_scared_images[] = {&dog_scared_1_img, &dog_scared_2_img};

typedef enum
{
    IMAGE_SELECTION_MAGICRABIT = 0,
    IMAGE_SELECTION_CAVEMAN,
    IMAGE_SELECTION_MAMMOTH,
    IMAGE_SELECTION_DINO3,
    IMAGE_SELECTION_ASTEROID,
    IMAGE_SELECTION_COUNT,
} image_selection_t;

static image_selection_t current_selection = IMAGE_SELECTION_MAGICRABIT;
static const char *selection_names[IMAGE_SELECTION_COUNT] = {
    "MagicRabit.jpg",
    "CaveManTest.jpg",
    "MammothTest.jpg",
    "DinoTest3.jpg",
    "AsteroidDeath.jpg",
};
static const uint8_t *selection_buffers[IMAGE_SELECTION_COUNT] = {NULL};
static size_t selection_sizes[IMAGE_SELECTION_COUNT] = {0};

extern const lv_img_dsc_t dino_thumb_img;
extern const lv_img_dsc_t caveman_thumb_img;
extern const lv_img_dsc_t mammoth_thumb_img;
extern const lv_img_dsc_t dino3_thumb_img;

static const lv_img_dsc_t *thumb_images[IMAGE_SELECTION_COUNT] = {
    &dino_thumb_img,
    &caveman_thumb_img,
    &mammoth_thumb_img,
    &dino_thumb_img,    // Reuse dino_thumb_img for DinoTest3 since we didn't add a specific one
    &mammoth_thumb_img, // Reuse mammoth for asteroid for now, or add a new thumb
};

static void update_selection_label(void)
{
    if (ui_selection_label)
    {
        char text[64];
        snprintf(text, sizeof(text), "Selected:\n%s", selection_names[current_selection]);
        lv_label_set_text(ui_selection_label, text);
    }
    if (ui_selection_thumb)
    {
        lv_img_set_src(ui_selection_thumb, thumb_images[current_selection]);
    }
}

static void image_toggle_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    dog_anim_index = 1 - dog_anim_index;

    if (current_dog_state == DOG_STATE_TALKING)
    {
        lv_img_set_src(ui_image, dog_talking_images[dog_anim_index]);
    }
    else if (current_dog_state == DOG_STATE_SCARED)
    {
        lv_img_set_src(ui_image, dog_scared_images[dog_anim_index]);
    }
    else
    {
        lv_img_set_src(ui_image, dog_idle_images[dog_anim_index]);
    }
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
        current_dog_state = DOG_STATE_TALKING;
        dog_anim_index = 0;
        lv_img_set_src(ui_image, dog_talking_images[dog_anim_index]);
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

    gemini_client_send_image(img_buf, img_size, false);
}

static void lvgl_send_task(void *arg)
{
    button_single_click_cb(NULL);
    vTaskDelete(NULL);
}

static void ui_image_clicked_cb(lv_event_t *e)
{
    xTaskCreate(lvgl_send_task, "send_task", 8192, NULL, 5, NULL);
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

static void lvgl_double_click_task(void *arg)
{
    button_double_click_cb(NULL);
    vTaskDelete(NULL);
}

static void ui_selection_box_clicked_cb(lv_event_t *e)
{
    xTaskCreate(lvgl_double_click_task, "dbl_task", 4096, NULL, 5, NULL);
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

    // Set dog animation state based on response text threat level
    if (voice_line != NULL)
    {
        // Simple case-insensitive-like search or just check for exact substring since prompt forces format
        if (strstr(voice_line, "Dangerous") != NULL || strstr(voice_line, "dangerous") != NULL ||
            strstr(voice_line, "Mild") != NULL || strstr(voice_line, "mild") != NULL)
        {
            current_dog_state = DOG_STATE_SCARED;
        }
        else
        {
            current_dog_state = DOG_STATE_IDLE; // "Safe" or unknown defaults to idle
        }
        dog_anim_index = 0;
        if (ui_image)
        {
            if (current_dog_state == DOG_STATE_SCARED)
            {
                lv_img_set_src(ui_image, dog_scared_images[dog_anim_index]);
            }
            else
            {
                lv_img_set_src(ui_image, dog_idle_images[dog_anim_index]);
            }
        }
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

    selection_buffers[IMAGE_SELECTION_MAGICRABIT] = magicrabit_jpg;
    selection_sizes[IMAGE_SELECTION_MAGICRABIT] = magicrabit_jpg_size;
    selection_buffers[IMAGE_SELECTION_CAVEMAN] = cavemantest_jpg;
    selection_sizes[IMAGE_SELECTION_CAVEMAN] = cavemantest_jpg_size;
    selection_buffers[IMAGE_SELECTION_MAMMOTH] = mammothtest_jpg;
    selection_sizes[IMAGE_SELECTION_MAMMOTH] = mammothtest_jpg_size;
    selection_buffers[IMAGE_SELECTION_DINO3] = dinotest3_jpg;
    selection_sizes[IMAGE_SELECTION_DINO3] = dinotest3_jpg_size;
    selection_buffers[IMAGE_SELECTION_ASTEROID] = asteroid_death_jpg;
    selection_sizes[IMAGE_SELECTION_ASTEROID] = asteroid_death_jpg_size;

    // The view should have allocated ui_image and ui_label on ui_screen_openai
    // We just need to start the timer!
    if (ui_image != NULL)
    {
        lv_img_set_src(ui_image, dog_idle_images[dog_anim_index]);
        lv_obj_add_flag(ui_image, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui_image, ui_image_clicked_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui_selection_box != NULL)
    {
        lv_obj_add_flag(ui_selection_box, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui_selection_box, ui_selection_box_clicked_cb, LV_EVENT_CLICKED, NULL);
    }
    
    if (ui_blind_image != NULL) {
        lv_img_set_src(ui_blind_image, blind_images[current_blind_selection]);
        lv_obj_add_flag(ui_blind_image, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui_blind_image, blind_image_clicked_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui_blind_selection_box != NULL) {
        lv_obj_add_flag(ui_blind_selection_box, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui_blind_selection_box, blind_selection_box_clicked_cb, LV_EVENT_CLICKED, NULL);
        if (ui_blind_selection_thumb != NULL) {
            lv_img_set_src(ui_blind_selection_thumb, blind_thumbs[current_blind_selection]);
        }
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
