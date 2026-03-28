#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void usb_camera_init(void);
bool usb_camera_capture_image(uint8_t **out_buf, size_t *out_size);
void usb_camera_free_image(void);
