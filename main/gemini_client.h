#pragma once

#include <stdint.h>
#include <stddef.h>

void gemini_client_init(void);
#include <stdbool.h>
void gemini_client_send_image(uint8_t *img_buf, size_t img_size, bool is_blind);
