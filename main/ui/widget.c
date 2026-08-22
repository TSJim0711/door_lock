#include <stdlib.h>
#include "widget.h"
#include "uni_font.h"
#include "esp_log.h"

#define MAX(x,y) ((x>y)?x:y)
#define MIN(x,y) ((x<y)?x:y)

//molecule function
void oled_print_char(uint8_t x,uint8_t y,enum language_e lang, char* chr,uint8_t size, bool txt_clr)
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
				oled_draw_px(x+x_shift,y+y_shift,txt_clr);
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
widget_id_t* wdg_lable_setup (uint16_t ul_x,uint16_t ul_y,uint16_t dr_x,uint16_t dr_y,char* content,uint8_t font_size,enum align_e align, bool txt_clr,uint8_t bg_clr, bool boxed)
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
			.align=align,
            .txt_clr=txt_clr,
			.bg_clr=bg_clr,
            .boxed=boxed
        }
    };
    strncpy(new_widget_id->widget_content,content,WIDGET_CONTENT_MAXSIZE);
    return new_widget_id;
}

//print a label on screen
struct column_start_end_t
{
	const char* start;
	const char* end;
	uint8_t x_size_px;
};
void wdg_lable_draw(widget_id_t const * wdg_id)
{
	if(wdg_id->uni_style_body.wdg_label_style.bg_clr==1)//draw background
		oled_draw_rect(wdg_id->ul_x, wdg_id->ul_y, wdg_id->dr_x, wdg_id->dr_y, 1, 0);

	char const * str_ptr=wdg_id->widget_content;//content
    uint8_t x_shift=0, y_shift=0, content_y_size=wdg_id->uni_style_body.wdg_label_style.font_size;//shift right after printing every char
	uint8_t free_x_space=MIN(wdg_id->dr_x-wdg_id->ul_x,127), free_y_space=MIN(wdg_id->dr_y-wdg_id->ul_y,63);

	//layout elf -> (/^u^)/ : I shal helpen to amenden hit
	uint8_t max_col_hold=(uint8_t)(free_y_space/wdg_id->uni_style_body.wdg_label_style.font_size);
	struct column_start_end_t column_start_end [max_col_hold];//find out when to skip line, free y space/font size
	uint16_t column_cnt=0;
	//ESP_EARLY_LOGI("UI", "CP1 cc:%d, cys:%d<fys:%d",column_cnt,content_y_size,free_y_space);
	for(column_cnt=0;column_cnt<max_col_hold && *str_ptr!='\0' && content_y_size<=free_y_space;column_cnt++)//loop untill reach str end / y space use up
	{
		column_start_end[column_cnt].start=str_ptr;
		//ESP_EARLY_LOGI("UI", "CP2 str:%s",column_start_end[column_cnt].start);
		x_shift=0;//new column reset x shift, lastmet space
		column_start_end[column_cnt].end=NULL;
		while(*str_ptr!='\0'&&(x_shift+wdg_id->uni_style_body.wdg_label_style.font_size<free_x_space||(*str_ptr<0x80 && x_shift+wdg_id->uni_style_body.wdg_label_style.font_size/2<free_x_space)))//end column when reach end sign/ enter sign/ no x space for another letter
		{
			//ESP_EARLY_LOGI("UI", "CP3 char:%c", *str_ptr);
			if(*str_ptr<0x80)//pend x_shift and str_ptr shift amount. Unicode Char DB? Never heard of it ;)
			{
				if((*str_ptr>=0x20&&*str_ptr<=0x2F)||(*str_ptr>=0x3A&&*str_ptr<=0x40)||(*str_ptr>=0x5B&&*str_ptr<=0x60))
				{
					column_start_end[column_cnt].end=str_ptr;
					column_start_end[column_cnt].x_size_px=x_shift;
				}
				if(*str_ptr=='\n')//switch column when reach 换行符
				{
					column_start_end[column_cnt].end=str_ptr;
					column_start_end[column_cnt].x_size_px=x_shift;
					str_ptr+=1;
					continue;
				}
				str_ptr+=1;
				x_shift+=wdg_id->uni_style_body.wdg_label_style.font_size/2;
			}else if(0xC0<=*str_ptr && *str_ptr<0xD0)
			{
				str_ptr+=2;
				x_shift+=wdg_id->uni_style_body.wdg_label_style.font_size;
			}else if(0xE0<=*str_ptr && *str_ptr<0xF0)
			{
				str_ptr+=3;
				x_shift+=wdg_id->uni_style_body.wdg_label_style.font_size;
				column_start_end[column_cnt].end=str_ptr;//every CHI char can be divider
				column_start_end[column_cnt].x_size_px=x_shift;
			}
			//ESP_EARLY_LOGI("UI", "CP3E xSize:%d < xMax%d", x_shift+wdg_id->uni_style_body.wdg_label_style.font_size,free_x_space);
		}
		if(*str_ptr=='\0')//set end ptr when met NUL
		{
			column_start_end[column_cnt].end=str_ptr;
			column_start_end[column_cnt].x_size_px=x_shift;
		}
		if(column_start_end[column_cnt].end==NULL)//no divider, switch line at vary end
		{
			column_start_end[column_cnt].end=str_ptr;//every CHI char can be divider
			column_start_end[column_cnt].x_size_px=x_shift;
		}
		//ESP_EARLY_LOGI("UI", "CP1E cc:%d, cys:%d<fys:%d",column_cnt,content_y_size,free_y_space);
		content_y_size+=wdg_id->uni_style_body.wdg_label_style.font_size;//down shift for next column
	}
	content_y_size-=wdg_id->uni_style_body.wdg_label_style.font_size;//undo final content_y_size add

	str_ptr=wdg_id->widget_content;//reset pointer
    x_shift=0;
	//ESP_EARLY_LOGI("UI", "CP4 fys:%d, cys:%d",free_y_space,content_y_size);
	//adjust starting y-pos according to y align
	if((wdg_id->uni_style_body.wdg_label_style.align&0x0C)==0x00)//align top (2&3 bit is 00)
		y_shift=0;
	else if((wdg_id->uni_style_body.wdg_label_style.align&0x0C)==0x04)//align mid (2&3 bit is 01)
		y_shift=(uint8_t)(free_y_space/2-content_y_size/2);
	else if((wdg_id->uni_style_body.wdg_label_style.align&0x0C)==0x08)//align bottom (2&3 bit is 10)
		y_shift=(uint8_t)(free_y_space-content_y_size);
	
	enum language_e cur_lang=-1;
	char chr[5];
	//ESP_EARLY_LOGI("UI", "CP5 yPos:%d < yMax:%d; ulY:%d yShift:%d fontSize:%d",wdg_id->ul_y+y_shift+wdg_id->uni_style_body.wdg_label_style.font_size,MIN(63,wdg_id->dr_y),wdg_id->ul_y,y_shift,wdg_id->uni_style_body.wdg_label_style.font_size);
	for(column_cnt=0;wdg_id->ul_y+y_shift+wdg_id->uni_style_body.wdg_label_style.font_size<=MIN(63,wdg_id->dr_y);column_cnt++)//second protect layer: make sure have enough y_space
	{
		//ESP_EARLY_LOGI("UI", "CP6");
		if ((wdg_id->uni_style_body.wdg_label_style.align&0x03)==0x00)//align left (0&1 bit is 00)
			x_shift=0;
		if ((wdg_id->uni_style_body.wdg_label_style.align&0x03)==0x01)//align Mid (0&1 bit is 01)
			x_shift=(uint8_t)(free_x_space/2-column_start_end[column_cnt].x_size_px/2);
		if ((wdg_id->uni_style_body.wdg_label_style.align&0x03)==0x02)//align right (0&1 bit is 10)
			x_shift=free_x_space-column_start_end[column_cnt].x_size_px;

		str_ptr=column_start_end[column_cnt].start;//move to col first char
		//ESP_EARLY_LOGI("UI", "CP7 str:%s, ul_x:%d,x_shift:%d < xMax:%d",column_start_end[column_cnt].start,wdg_id->ul_x,x_shift,MIN(127,wdg_id->dr_x));
		while(*str_ptr!='\0' && str_ptr<column_start_end[column_cnt].end && wdg_id->ul_x+x_shift<=MIN(127,wdg_id->dr_x))//print untill reach str end/ column end/ screen edge 
		{
			////ESP_LOGI("UI", "CP8 x:%d, y:%d, str left:%.*s", wdg_id->ul_x+x_shift,wdg_id->ul_y, (int)(column_start_end[column_cnt].end-str_ptr),str_ptr);
			memset(chr, 0, sizeof(chr));//char end anywhere
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
			//ESP_EARLY_LOGI("UI", "CP9");
			oled_print_char(wdg_id->ul_x+x_shift, wdg_id->ul_y+y_shift, cur_lang, chr, wdg_id->uni_style_body.wdg_label_style.font_size,wdg_id->uni_style_body.wdg_label_style.txt_clr);
			x_shift+=wdg_id->uni_style_body.wdg_label_style.font_size/(cur_lang==Eng?2:1);//shift x, eng width = half size. What Unicode Char DB? Never heard of it. X)
			//ESP_EARLY_LOGI("UI", "CP7E str:%s, ul_x:%d,x_shift:%d < xMax:%d",column_start_end[column_cnt].start,wdg_id->ul_x,x_shift,MIN(127,wdg_id->dr_x));
		}
		//ESP_EARLY_LOGI("UI", "CP5E yPos:%d < yMax:%d; ulY:%d yShift:%d fontSize:%d",wdg_id->ul_y+y_shift+wdg_id->uni_style_body.wdg_label_style.font_size,MIN(63,wdg_id->dr_y),wdg_id->ul_y,y_shift,wdg_id->uni_style_body.wdg_label_style.font_size);
		y_shift+=wdg_id->uni_style_body.wdg_label_style.font_size;//shift downward
	}
}

void wdg_label_str_edit(widget_id_t * wdg_id, char* new_str)
{
	strncpy(&(wdg_id->widget_content[0]),new_str,WIDGET_CONTENT_MAXSIZE);
}