#pragma once

#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Draw the vector boot-logo animation.
 * @param panel_handle ESP-LCD display handle.
 */
void logo_draw_bitmap(esp_lcd_panel_handle_t panel_handle);

#ifdef __cplusplus
}
#endif
