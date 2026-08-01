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
                if(strcmp(en_font24[idx].index,chr)==0)
                {
                    return &(en_font24[idx].msk[0]);
                    break;
                }
                idx++;
            }while(en_font24[idx].index[0]!='\0');
        }else if(size==16)
        {
            do
            {
                if(strcmp(en_font16[idx].index,chr)==0)
                {
                    return &(en_font16[idx].msk[0]);
                    break;
                }
                idx++;
            }while(en_font16[idx].index[0]!='\0');
        }

    }else if(lang==Hans || lang==Hant)
    {
        if(size==24)
        {
            do
            {
                if(strcmp(zh_font24[idx].index,chr)==0)
                {
                    return &(zh_font24[idx].msk[0]);
                    break;
                }
                idx++;
            }while(zh_font24[idx].index[0]!='\0');
        }
    }
    return NULL;
};
