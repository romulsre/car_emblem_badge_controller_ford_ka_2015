#pragma once

#include "esp_lcd_panel_ops.h"
#include <stdint.h>

// RGB565 examples: green 0x07E0, blue 0x001F, red 0xF800, amber 0xFD20.
// Call once after the display is initialized.
bool eye_animation_begin(esp_lcd_panel_handle_t panel_handle, uint16_t iris_rgb565);

// Call repeatedly from Arduino loop(), passing millis().  It is non-blocking
// and redraws at up to 40 frames per second.
void eye_animation_update(esp_lcd_panel_handle_t panel_handle, uint32_t now_ms);

// Can be called at any time; the new RGB565 color appears on the next frame.
void eye_animation_set_iris_color(uint16_t iris_rgb565);

// Release the 360 * 360 * 2 byte framebuffer if the eye will no longer be used.
void eye_animation_end();
