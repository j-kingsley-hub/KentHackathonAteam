#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "bsp_board.h"
#include "lvgl.h"
#include "button_gpio.h"
#include "usb_camera.h"
#include "gemini_client.h"
#include "lv_port.h"
#include "bsp_btn.h"

// Declare the external image struct
extern const lv_img_dsc_t dogprototype_img;

static const char *TAG = "CAVEMEN_DOG";
// update_ui declaration (used by gemini client)
void update_ui(const char *voice_line, const char *color_hex_str);

#define TRIGGER_BUTTON_GPIO 0 // BOOT button for prototype, change to grove GPIO if mapped

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

static lv_obj_t *ui_label = NULL;

void update_ui(const char *voice_line, const char *color_hex_str)
{
    lv_port_sem_take();
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
    lv_port_sem_give();
}

// Dummy audio play logic (would normally write to I2S / bsp codec)
static void play_voice_line(const char *voice_line)
{
    ESP_LOGI(TAG, "Playing audio for: %s", voice_line);
    // TODO: implement actual I2S audio playback from SPIFFS/SD card here based on the line
}

static void app_lvgl_display(void)
{
    lv_port_sem_take();
    // Basic LVGL setup for initial UI
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0); // Black background

    // Create an image object and set the dog picture
    lv_obj_t *img = lv_img_create(scr);
    lv_img_set_src(img, &dogprototype_img);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, -40); // Align center, slightly up

    ui_label = lv_label_create(scr);
    lv_label_set_text(ui_label, "Cavemen's Best Friend\nWaiting for trigger...");
    lv_obj_align(ui_label, LV_ALIGN_CENTER, 0, 100);
    lv_obj_set_style_text_color(ui_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(ui_label, LV_TEXT_ALIGN_CENTER, 0);

    lv_port_sem_give();
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

    // Initialize LVGL port
    ESP_LOGI(TAG, "Initializing LVGL port...");
    lv_port_init();

    // Initialize Camera
    usb_camera_init();

    // Initialize Button using BSP
    bsp_btn_register_callback(BOARD_BTN_ID_USER, BUTTON_SINGLE_CLICK, button_single_click_cb, NULL);

    // Initialize LVGL content
    ESP_LOGI(TAG, "Initializing LVGL UI...");
    app_lvgl_display();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
