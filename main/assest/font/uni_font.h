#ifndef FONT_MSK_H
#define FONT_MSK_H
#include <stdint.h>

enum language {Hans,Hant,Eng,Symbo,Emoji};

typedef struct mskkit72 {
    unsigned char msk[72];
    char index[4];
} mskkit72;

typedef struct mskkit48{
    unsigned char msk[48];
    char index[4];
} mskkit48;

typedef struct mskkit16{
    unsigned char msk[16];
    char index[4];
} mskkit16;

extern const mskkit72 font24_zh[];
extern const mskkit48 font24_en[];
extern const mskkit16 font16_en[];


unsigned char const * get_char_msk(enum language lang,uint8_t size,char* chr);

#endif