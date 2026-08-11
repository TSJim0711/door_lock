#ifndef OLED_H
#define OLED_H

enum language {Hans,Hant,Eng,Symbo,Emoji};

void i2c_master_init(void);
void oled_init(void);
void oled_draw_px(uint8_t x,uint8_t y,uint8_t t);
void oled_draw_line(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2,uint8_t mode, uint8_t is_wrapped);
void oled_draw_rect(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2,uint8_t mode, uint8_t is_wrapped);
void oled_print_char(uint8_t x,uint8_t y,enum language lang, char* chr,uint8_t size,uint8_t mode, uint8_t is_wrapped);
void oled_print_str(uint8_t x,uint8_t y, char* str,uint8_t size, uint8_t mode, uint8_t is_wrapped);
void oled_screen_update(void);
void oled_screen_clear(void);

#endif