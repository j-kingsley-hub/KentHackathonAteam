file(GLOB_RECURSE PAIRING_SOURCES "*.cpp" "*.c")

idf_component_register(SRCS ${PAIRING_SOURCES}
                    INCLUDE_DIRS "."
                    REQUIRES bsp iot_button freertos esp_wifi nvs_flash lvgl esp_http_client json usb)