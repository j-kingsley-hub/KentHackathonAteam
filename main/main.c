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

#include "indicator_model.h"
#include "indicator_view.h"
// #include "indicator_controller.h"

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

ESP_EVENT_DEFINE_BASE(VIEW_EVENT_BASE);
esp_event_loop_handle_t view_event_handle;

// Define for dog UI
lv_obj_t *ui_dog_label = NULL;
extern lv_obj_t *ui_screen_openai;

void update_ui(const char *voice_line, const char *color_hex_str)
{
    lv_port_sem_take();

    int color_val = 0;
    if (color_hex_str != NULL && strlen(color_hex_str) >= 6)
    {
        if (color_hex_str[0] == '#')
            color_hex_str++;
        color_val = (int)strtol(color_hex_str, NULL, 16);
    }

    if (ui_screen_openai)
    {
        lv_obj_set_style_bg_color(ui_screen_openai, lv_color_hex(color_val), 0);
    }

    if (ui_dog_label)
    {
        lv_label_set_text(ui_dog_label, voice_line ? voice_line : "Unknown Dog noise");
    }
    lv_port_sem_give();
}

static void button_single_click_cb(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "Button pressed! Capturing image...");

    uint8_t *img_buf = NULL;
    size_t img_size = 0;

    if (usb_camera_capture_image(&img_buf, &img_size))
    {
        ESP_LOGI(TAG, "Image captured! Size: %zu bytes. Sending to Gemini...", img_size);
        gemini_client_send_image(img_buf, img_size);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to capture image.");
    }
}

void app_main(void)
{
    ESP_LOGI("", SENSECAP, VERSION, __DATE__, __TIME__);

    ESP_ERROR_CHECK(bsp_board_init());

    // Initialize Camera
    usb_camera_init();

    // Initialize Onboard User Button (GPIO 38, top of the SenseCAP Indicator)
    bsp_btn_register_callback(BOARD_BTN_ID_USER, BUTTON_SINGLE_CLICK, button_single_click_cb, NULL);

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
    lv_port_sem_give();

    indicator_model_init();
    indicator_controller_init();

    static char buffer[128]; /* Make sure buffer is enough for `sprintf` */
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
