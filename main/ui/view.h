#include <stdint.h>
#include <stdbool.h>
#include "widget.h"

#define MAX_WIDGET_PER_VIEW 24

//view formats=========
enum view_type_e{   
                        VIEW_TITLE_CONTENT,             //a place to display text.
                        VIEW_TITLE_CONTENT_COUNTDOWN,   //Title and text, with countdown
                        VIEW_LIST,                      //a list for selection
                    };

//view body============
//adapt every view struct, so, turn other view struct to widget_arr_only when stating widget.
//e.g.:view_title_content_t->view_widget_arr_only
typedef struct view_widget_arr_only
{
    widget_id_t* stated_widget[MAX_WIDGET_PER_VIEW];
}view_widget_arr_only;

void view_print_widget(view_widget_arr_only* view);

//TITLE_CONTENT
typedef struct view_title_content_t
{
    widget_id_t* stated_widget[MAX_WIDGET_PER_VIEW];//this will help recovering the view quick
    char* tc_title;
    char* tc_content;
}view_title_content_t;
view_title_content_t* view_title_content_setup (char* title, char* content);


//VIEW_TITLE_CONTENT_COUNTDOWN
typedef struct view_title_content_countdown_t
{
    widget_id_t* stated_widget[MAX_WIDGET_PER_VIEW];
    char* tcc_title;
    char* tcc_content;
    uint16_t tcc_timer;
}view_title_content_countdown_t;