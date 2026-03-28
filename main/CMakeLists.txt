idf_component_register(SRCS "gemini_client.c" "main.c" "usb_camera.c"
                    INCLUDE_DIRS "."
                    REQUIRES bsp iot_button freertos esp_wifi nvs_flash lvgl esp_http_client json usb)