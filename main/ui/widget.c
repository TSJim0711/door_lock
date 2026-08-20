#include <stdlib.h>
#include "widget.h"
#include "uni_font.h"
#include "esp_log.h"

#define MAX(x,y) ((x>y)?x:y)

//molecule function
void oled_print_char(uint8_t x,uint8_t y,enum language_e lang, char* chr,uint8_t size, bool fill_bg)
{
	uint8_t byte, cur_byte_cont, bit, width, msk_size, msk_drop_bit=0;
	uint8_t x0=x,y0=y,x_shift=0,y_shift=0;
	unsigned char const *msk=get_char_msk(lang,size,chr);
	if(msk==NULL)
		return;
	
	width=size/(lang==Eng?2:1);//eng have half width
	msk_size=width*size/8;
	for(byte=0;byte<msk_size+(msk_drop_bit/8);byte++)
	{
		cur_byte_cont=*(msk+byte);
		for(bit=0;bit<8;bit++)
		{
			if(cur_byte_cont&0x01)//if cur bit need to draw or blank, then do so
				oled_draw_px(x+x_shift,y+y_shift,fill_bg);
			else
				oled_draw_px(x+x_shift,y+y_shift,!fill_bg);
			cur_byte_cont>>=1;//shift right, see next bit
			
			x_shift++;//next pixel to draw
			if(x_shift>=width)//width reach font size, drop cur byte
			{
				msk_drop_bit+=8-bit;//record droped bit in byte, for further msk_size reference
				break;
			}
		}
		if(x_shift>=width)//width reach font size, reset width, col+1
		{
			x_shift=0;
			y_shift++;
			if(y_shift>=size)//patch: msk_drop_bit may +1 even when last col, an underline may print, which not preffer
				break;
		}
  	}
}


//LABEL
//label create
widget_id_t* wdg_lable_setup (uint16_t ul_x,uint16_t ul_y,uint16_t dr_x,uint16_t dr_y,char* content,uint8_t font_size, bool fill_bg, bool boxed)
{
    widget_id_t* new_widget_id=(widget_id_t*)malloc(sizeof(widget_id_t));
    (*new_widget_id)=(widget_id_t){//load all input to struct
        .ul_x=ul_x,
        .ul_y=ul_y,
        .dr_x=dr_x,
        .dr_y=dr_y,
		.print_func_ptr=wdg_lable_draw,//load print function a widget id
        .uni_style_body.wdg_label_style={
            .font_size=font_size,
            .inv_clr=fill_bg,
            .boxed=boxed
        }
    };
    strncpy(new_widget_id->widget_content,content,WIDGET_CONTENT_MAXSIZE);
    return new_widget_id;
}

//print a label on screen
void wdg_lable_draw(widget_id_t const * wdg_id)
{
    char const * str_ptr=wdg_id->widget_content;//content
    uint8_t x_shift=0;//shift right after printing every char
	enum language_e cur_lang=-1;
	while(*str_ptr!='\0' && wdg_id->ul_x+x_shift<=MAX(127,wdg_id->dr_x))//print untill reach str end or screen edge 
	{
		//ESP_LOGI("UI", "x:%d, y:%d, str left:%s", wdg_id->ul_x+x_shift,wdg_id->ul_y, str_ptr);

		char chr[5]={'\0','\0','\0','\0','\0'};//char end anywhere
		if(*str_ptr<0x80)//merge char into str
		{
			chr[0]=*str_ptr++;
			cur_lang=Eng;
		}
		else if(0xC0<=*str_ptr && *str_ptr<0xD0)
		{
			chr[0]=*str_ptr++;
			chr[1]=*str_ptr++;
		}else if(0xE0<=*str_ptr && *str_ptr<0xF0)
		{
			chr[0]=*str_ptr++;
			chr[1]=*str_ptr++;
			chr[2]=*str_ptr++;
			cur_lang=Hans;
		}
		oled_print_char(wdg_id->ul_x+x_shift, wdg_id->ul_y, cur_lang, chr, wdg_id->uni_style_body.wdg_label_style.font_size,wdg_id->uni_style_body.wdg_label_style.inv_clr);
		x_shift+=wdg_id->ul_x+wdg_id->uni_style_body.wdg_label_style.font_size/(cur_lang==Eng?2:1);//shift x, eng width = half size. What Unicode Char DB? Never heard of it. X)
	}
}

void wdg_label_str_edit(widget_id_t * wdg_id, char* new_str)
{
	strncpy(&(wdg_id->widget_content[0]),new_str,WIDGET_CONTENT_MAXSIZE);
}