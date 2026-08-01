#ifndef OLED_H
#define OLED_H

enum language {Hans,Hant,Eng,Symbo,Emoji};

void i2c_master_init(void);
void OLED_Init(void);
void OLED_DrawPoint(uint8_t x,uint8_t y,uint8_t t);
void OLED_DrawLine(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2,uint8_t mode, uint8_t is_wrapped);
void OLED_DrawTetragon(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2,uint8_t mode, uint8_t is_wrapped);
void OLED_ShowChar(uint8_t x,uint8_t y,enum language lang, char* chr,uint8_t size,uint8_t mode, uint8_t is_wrapped);
void OLED_ShowStr(uint8_t x,uint8_t y, char* str,uint8_t size, uint8_t mode, uint8_t is_wrapped);
void OLED_Refresh(void);
void OLED_Clear(void);

#endif