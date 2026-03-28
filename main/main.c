#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "bsp_board.h"
#include "lvgl.h"
#include "button_gpio.h"
#include "usb_camera.h"
#include "iot_button.h"
#include "gemini_client.h"

static const char *TAG = "CAVEMEN_DOG";
// update_ui declaration (used by gemini client)
void update_ui(const char *voice_line, const char *color_hex_str);

#define TRIGGER_BUTTON_GPIO 0 // BOOT button for prototype, change to grove GPIO if mapped

static void button_single_click_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "Button pressed! Capturing image...");

    uint8_t *img_buf = NULL;
    size_t img_size = 0;

    if (usb_camera_capture_image(&img_buf, &img_size))
    {
        ESP_LOGI(TAG, "Image captured! Size: %d bytes. Sending to Gemini...", img_size);
        gemini_client_send_image(img_buf, img_size);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to capture image.");
    }
}

static lv_obj_t *ui_label = NULL;

void update_ui(const char *voice_line, const char *color_hex_str)
{
    bsp_display_lock(0);
    lv_obj_t *scr = lv_scr_act();

    // Parse color (e.g. "#FF0000" or "FF0000")
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

    bsp_display_unlock();
}

// Dummy audio play logic (would normally write to I2S / bsp codec)
static void play_voice_line(const char *voice_line)
{
    ESP_LOGI(TAG, "Playing audio for: %s", voice_line);
    // TODO: implement actual I2S audio playback from SPIFFS/SD card here based on the line
}

static void app_lvgl_display(void)
{
    // Basic LVGL setup for initial UI
    bsp_display_lock(0);

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0); // Black background

    ui_label = lv_label_create(scr);
    lv_label_set_text(ui_label, "Cavemen's Best Friend\nWaiting for trigger...");
    lv_obj_center(ui_label);
    lv_obj_set_style_text_color(ui_label, lv_color_hex(0xFFFFFF), 0);

    bsp_display_unlock();
}

void app_main(void)
{
    ESP_LOGI(TAG, "Cavemen's Best Friend starting up!");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize BSP (Board Support Package) for SenseCAP Indicator
    ESP_LOGI(TAG, "Initializing BSP...");
    ESP_ERROR_CHECK(bsp_board_init());

    // Initialize Camera
    usb_camera_init();

    // Initialize Button
    button_config_t gpio_btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .gpio_button_config = {
            .gpio_num = TRIGGER_BUTTON_GPIO,
            .active_level = 0,
        },
    };
    button_handle_t gpio_btn = iot_button_create(&gpio_btn_cfg);
    if (NULL == gpio_btn)
    {
        ESP_LOGE(TAG, "Button create failed");
    }
    else
    {
        iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, button_single_click_cb, NULL);
    }

    // Initialize LCD and LVGL
    ESP_LOGI(TAG, "Initializing Display & LVGL...");
    ESP_ERROR_CHECK(bsp_board_lcd_init());
    bsp_display_start();
    bsp_board_lcd_set_backlight(100); // 100% backlight

    app_lvgl_display();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
