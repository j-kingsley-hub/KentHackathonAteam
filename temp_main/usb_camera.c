#include "usb_camera.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "USB_CAMERA";
static bool camera_ready = false;
static uint8_t *last_frame_buf = NULL;
static size_t last_frame_size = 0;

void usb_camera_init(void)
{
    camera_ready = false;
    ESP_LOGW(TAG, "UVC camera support is not enabled in this build.");
}

bool usb_camera_capture_image(uint8_t **out_buf, size_t *out_size)
{
    if (!camera_ready)
    {
        ESP_LOGE(TAG, "Camera not ready for capture.");
        return false;
    }

    if (last_frame_buf == NULL)
    {
        ESP_LOGW(TAG, "No frame captured yet.");
        return false;
    }

    *out_buf = last_frame_buf;
    *out_size = last_frame_size;
    return true;
}

void usb_camera_free_image(void)
{
    if (last_frame_buf)
    {
        free(last_frame_buf);
        last_frame_buf = NULL;
        last_frame_size = 0;
    }
}
