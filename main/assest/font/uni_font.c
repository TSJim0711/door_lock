#include <stdio.h>
#include <string.h>
#include "uni_font.h"

unsigned char const * get_char_msk(enum language lang,uint8_t size,char* chr)
{
    int idx=0;
    if(lang==Eng)
    {
        if(size==24)
        {
            do
            {
                if(strcmp(font24_en[idx].index,chr)==0)
                {
                    return &(font24_en[idx].msk[0]);
                    break;
                }
                idx++;
            }while(font24_en[idx].index[0]!='\0');
        }else if(size==16)
        {
            do
            {
                if(strcmp(font16_en[idx].index,chr)==0)
                {
                    return &(font16_en[idx].msk[0]);
                    break;
                }
                idx++;
            }while(font16_en[idx].index[0]!='\0');
        }

    }else if(lang==Hans || lang==Hant)
    {
        if(size==24)
        {
            do
            {
                if(strcmp(font24_zh[idx].index,chr)==0)
                {
                    return &(font24_zh[idx].msk[0]);
                    break;
                }
                idx++;
            }while(font24_zh[idx].index[0]!='\0');
        }
    }
    return NULL;
};
