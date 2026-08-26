#include <stdint.h>
#include <stdbool.h>
#include "widget.h"

#define MAX_WIDGET_PER_VIEW 24

//view formats=========
enum view_type_e{   
                        VIEW_TITLE_CONTENT,             //a place to display text.
                        VIEW_INPT_PAGE,                 //with a input txt box 
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


//VIEW_INPT_TXT
typedef struct view_input_page
{
    widget_id_t* stated_widget[MAX_WIDGET_PER_VIEW];
    char* ipg_title;
    char* ipg_inpt_content;
    widget_id_t* ipg_inpt_wdg_id;
    char* ipg_tips;
}view_input_page;
view_input_page* view_input_page_setup (char* title, char* defualt_inpt, char* tips);