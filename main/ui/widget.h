#include <stdint.h>
#include <stdbool.h>
#include "oled.h"
#include <string.h>

#define WIDGET_CONTENT_MAXSIZE 64

enum language_e {Hans,Hant,Eng,Symbo,Emoji};

//LABEL; to display text.
typedef struct wdg_label_style_t
{
    uint8_t font_size;
    bool inv_clr;//invert color, to let background be white and char be black
    bool boxed;//unable to use
}wdg_label_style_t;

//pakage
typedef union wdg_uni_style_e//能塞进widget_style
{
    uint8_t uni_style_body[12];
    wdg_label_style_t wdg_label_style;
}wdg_uni_style_e;

typedef struct widget_id_t widget_id_t;
struct widget_id_t
{
    uint16_t ul_x;//up left point
    uint16_t ul_y;
    uint16_t dr_x;//down right point
    uint16_t dr_y;
    void (*print_func_ptr)(const widget_id_t*);//print method
    wdg_uni_style_e uni_style_body;//store widget font size/ mode/ etc...
    char widget_content[WIDGET_CONTENT_MAXSIZE];
};

//Label function
widget_id_t* wdg_lable_setup(uint16_t ul_x,uint16_t ul_y,uint16_t dr_x,uint16_t dr_y,char* content,uint8_t font_size, bool fill_bg, bool boxed);
void wdg_lable_draw(widget_id_t const * wdg_id);