#include "usb_camera.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "usb_stream.h"

static const char *TAG = "USB_CAMERA";
static bool camera_ready = false;
static uint8_t *last_frame_buf = NULL;
static size_t last_frame_size = 0;

static void camera_frame_cb(uvc_frame_t *frame, void *ptr)
{
    if (frame->data_bytes > 0 && frame->data)
    {
        // Free previous frame if it exists
        if (last_frame_buf)
        {
            free(last_frame_buf);
            last_frame_buf = NULL;
        }

        // Allocate memory for new frame
        last_frame_buf = heap_caps_malloc(frame->data_bytes, MALLOC_CAP_SPIRAM);
        if (last_frame_buf)
        {
            memcpy(last_frame_buf, frame->data, frame->data_bytes);
            last_frame_size = frame->data_bytes;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to allocate memory for frame");
            last_frame_size = 0;
        }
    }
}

static char global_camera_error[128] = "USB Task Running: NO physical connection ever detected on D+/D- pins (Usually 19/20).";
static int camera_connect_cb_fired = 0;

static void camera_state_cb(usb_stream_state_t state, void *ptr)
{
    camera_connect_cb_fired++;
    if (state == STREAM_CONNECTED)
    {
        ESP_LOGI(TAG, "USB Camera Connected");
        camera_ready = true;
        snprintf(global_camera_error, sizeof(global_camera_error), "STATE CB: CONNECTED! (But no frame mapped to buffer yet)");
    }
    else
    {
        ESP_LOGI(TAG, "USB Camera Disconnected");
        camera_ready = false;
        snprintf(global_camera_error, sizeof(global_camera_error), "STATE CB: DISCONNECTED! (Device un-plugged or power dropped)");
    }
}

void usb_camera_init(void)
{
    ESP_LOGI(TAG, "Initializing UVC Camera...");

    uvc_config_t uvc_config = {
        .xfer_buffer_size = 40 * 1024,
        .xfer_buffer_a = heap_caps_malloc(40 * 1024, MALLOC_CAP_SPIRAM),
        .xfer_buffer_b = heap_caps_malloc(40 * 1024, MALLOC_CAP_SPIRAM),
        .frame_buffer_size = 150 * 1024,
        .frame_buffer = heap_caps_malloc(150 * 1024, MALLOC_CAP_SPIRAM),
        .frame_width = FRAME_RESOLUTION_ANY,
        .frame_height = FRAME_RESOLUTION_ANY,
        .frame_interval = FRAME_INTERVAL_FPS_15, // MUST be a valid interval!
        .format = UVC_FORMAT_MJPEG,
        .frame_cb = camera_frame_cb,
        .frame_cb_arg = NULL,
    };

    if (uvc_config.xfer_buffer_a == NULL || uvc_config.xfer_buffer_b == NULL || uvc_config.frame_buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for USB stream buffers in SPIRAM");
        snprintf(global_camera_error, sizeof(global_camera_error), "Init Fail: Out of SPIRAM memory.");
        return;
    }

    esp_err_t ret = uvc_streaming_config(&uvc_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to config UVC stream: %s", esp_err_to_name(ret));
        snprintf(global_camera_error, sizeof(global_camera_error), "Init Fail: uvc_streaming_config error.");
        return;
    }

    ret = usb_streaming_state_register(camera_state_cb, NULL);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register state cb: %s", esp_err_to_name(ret));
        snprintf(global_camera_error, sizeof(global_camera_error), "Init Fail: state_register error.");
    }

    ret = usb_streaming_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start UVC stream: %s", esp_err_to_name(ret));
        snprintf(global_camera_error, sizeof(global_camera_error), "Init fail: stream_start error (Check USB connection)");
    }
    else
    {
        ESP_LOGI(TAG, "UVC Camera setup complete. Waiting for connection...");
    }
}

bool usb_camera_capture_image(uint8_t **out_buf, size_t *out_size, const char **error_msg)
{
    // Check if the stream has connected silently
    esp_err_t wait_res = usb_streaming_connect_wait(0);

    if (!camera_ready)
    {
        ESP_LOGE(TAG, "Camera not ready for capture.");
        
        if (camera_connect_cb_fired == 0) {
            snprintf(global_camera_error, sizeof(global_camera_error), "Err: Wait returned %s. Interrupt CB never fired! Check USB Physical Port (D+/D-) & Hub Power.", esp_err_to_name(wait_res));
        }
        
        if (error_msg)
            *error_msg = global_camera_error;
        return false;
    }

    if (last_frame_buf == NULL)
    {
        ESP_LOGW(TAG, "No frame captured yet.");
        if (error_msg)
            *error_msg = "Device connected & callbacks fired, but MJPEG callback never gave a frame. Unsupported cam codec?";
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
