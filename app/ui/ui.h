#ifndef __UI_H__
#define __UI_H__

#include <stdint.h>
#include "lcd.h"

#define UI_WIDTH  LCD_WIDTH
#define UI_HEIGHT LCD_HEIGHT

void ui_init(void);
void ui_clear(uint16_t color);
void ui_fill_color(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void ui_show_string(uint16_t x, uint16_t y, const char *str, uint16_t fc, uint16_t bc, const font_t *font);
void ui_show_photo(uint16_t x, uint16_t y, const image_t *image);

#endif
