#ifndef VIEW_H 
#define VIEW_H

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

//actions
enum action_type_e
{
    ACTION_LAUNCH,
    ACTION_I_SET,
    ACTION_PTR_SET
};
typedef struct activity_t activity_t;
typedef struct action_launch_t
{
    activity_t* act_activity_launch;
}action_launch_t;
typedef struct action_i_set_t
{
    void* act_targ_var;
    uint32_t act_var_val;
}action_i_set_t;
typedef struct action_ptr_set_t
{
    void* act_targ_var;
    void* act_var_val;
}action_ptr_set_t;

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


//VIEW_INPT_PAGE
typedef struct view_input_page
{
    widget_id_t* stated_widget[MAX_WIDGET_PER_VIEW];
    char* ipg_title;
    char* ipg_inpt_content;
    widget_id_t* ipg_inpt_wdg_id;
    char* ipg_tips;
}view_input_page;
view_input_page* view_input_page_setup (char* title, char* defualt_inpt, char* tips);


//VIEW_LIST
#define MAX_VIEW_LIST_ELEMENT 32
typedef struct vl_content_t
{
    enum action_type_e vlc_action_type;
    void* vlc_action;
    char* vlc_title;
    struct vl_content_t* vlc_prev_content;
    struct vl_content_t* vlc_next_content;
}vl_content_t;

typedef struct view_list
{
    widget_id_t* stated_widget[MAX_WIDGET_PER_VIEW];
    char* list_title;
    vl_content_t* vl_content_select;
    vl_content_t* vl_content_tail;
}view_list;
view_list* view_list_setup(char* title);
bool view_list_push_content(view_list* list_push_to,char* title, enum action_type_e action_type,uint8_t action_type_size,void* action);
void view_list_select_shift(view_list* list_shifting,signed char dir);

#endif