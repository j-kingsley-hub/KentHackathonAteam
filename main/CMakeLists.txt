idf_component_register(SRCS "lv_port.c" "gemini_client.c" "main.c" "usb_camera.c" "dogprototype_img.c"
                    INCLUDE_DIRS "."
                    REQUIRES bsp iot_button freertos esp_wifi nvs_flash lvgl esp_http_client json usb)