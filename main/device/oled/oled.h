#ifndef OLED_H
#define OLED_H

void i2c_master_init(void);
void OLED_Init(void);
void OLED_DrawPoint(uint8_t x,uint8_t y,uint8_t t);
void OLED_DrawLine(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2,uint8_t mode, uint8_t is_wrapped);
void OLED_DrawTetragon(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2,uint8_t mode, uint8_t is_wrapped);
void OLED_Refresh(void);

#endif