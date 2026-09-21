#pragma once

#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Отрисовка векторной boot-анимации заставки
 * @param panel_handle Дескриптор дисплея ESP-LCD
 */
void logo_draw_bitmap(esp_lcd_panel_handle_t panel_handle);

#ifdef __cplusplus
}
#endif