#ifndef OLED_H
#define OLED_H

#include <stdint.h>

void i2c_master_init(void);
void oled_init(void);
void oled_draw_px(uint8_t x,uint8_t y,bool inv_clr);
void oled_draw_line(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2,uint8_t mode, uint8_t is_wrapped);
void oled_draw_rect(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2,uint8_t mode, uint8_t is_wrapped);
void oled_screen_update(void);
void oled_screen_clear(void);

#endif