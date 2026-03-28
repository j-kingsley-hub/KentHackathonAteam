#include "usb_camera.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/uvc_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "USB_CAMERA";
static uvc_host_device_handle_t uvc_dev = NULL;
static bool camera_ready = false;
static uint8_t *last_frame_buf = NULL;
static size_t last_frame_size = 0;

static void camera_frame_cb(uvc_frame_t *frame, void *ptr)
{
    ESP_LOGI(TAG, "Frame received: format=%d, width=%d, height=%d, data_bytes=%d",
             frame->frame_format, frame->width, frame->height, frame->data_bytes);

    // In a real app we'd copy the JPEG frame data. For this prototype sketch:
    if (frame->data_bytes > 0 && last_frame_buf == NULL)
    {
        last_frame_buf = malloc(frame->data_bytes);
        if (last_frame_buf)
        {
            memcpy(last_frame_buf, frame->data, frame->data_bytes);
            last_frame_size = frame->data_bytes;
            ESP_LOGI(TAG, "Captured frame of %d bytes", frame->data_bytes);
        }
    }
}

static void uvc_device_cb(uvc_host_device_handle_t uvc_device, int event, void *arg)
{
    if (event == UVC_HOST_DEVICE_EVENT_RX_DONE)
    {
        ESP_LOGI(TAG, "UVC Device Event: RX DONE");
    }
    else if (event == UVC_HOST_DEVICE_EVENT_DISCONNECTED)
    {
        ESP_LOGW(TAG, "UVC Device Disconnected");
        camera_ready = false;
    }
}

void usb_camera_init(void)
{
    ESP_LOGI(TAG, "Initializing USB Host UVC...");

    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    uvc_host_config_t uvc_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
    };
    ESP_ERROR_CHECK(uvc_host_install(&uvc_config));

    ESP_LOGI(TAG, "Waiting for USB Camera connection...");
    // A more robust implementation would poll `usb_host_device_info()`
    // or register a connection callback. We will simplify for now.
}

bool usb_camera_capture_image(uint8_t **out_buf, size_t *out_size)
{
    if (!camera_ready || uvc_dev == NULL)
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
