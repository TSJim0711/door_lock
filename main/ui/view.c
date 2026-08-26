#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "view.h"

bool view_state_widget(view_widget_arr_only* view, widget_id_t* widget_id)
{//load a widget detail to view
    for(short i=0;i<MAX_WIDGET_PER_VIEW;i++)
        if(view->stated_widget[i]==NULL)
        {
            view->stated_widget[i]=widget_id;
            return true;
        }
    return false;
}

//print all stated widgets of view
void view_print_widget(view_widget_arr_only* view)
{
    for(short i=0;i<MAX_WIDGET_PER_VIEW;i++)
        if(view->stated_widget[i]!=NULL)//if find widget
            view->stated_widget[i]->print_func_ptr(view->stated_widget[i]);//run widget print function
}

//view structures setup
view_title_content_t* view_title_content_setup (char* title, char* content)
{
    view_title_content_t* new_view=(view_title_content_t*)malloc(sizeof(view_title_content_t));
    for(short i=0;i<MAX_WIDGET_PER_VIEW;i++)//clean addr, make sure full NULL
        new_view->stated_widget[i]=NULL;
    
    //load title_content widget
    widget_id_t* wdg_id_addr=wdg_lable_setup(0,0,127,24,title,24,ALIGN_TOP_MID,0,1,0);//create label
    view_state_widget((view_widget_arr_only*)new_view,wdg_id_addr);//load wdg to activity
    new_view->tc_title=wdg_id_addr->widget_content;//link title to label str

    wdg_id_addr=wdg_lable_setup(0,25,127,63,content,16,ALIGN_MID_LEFT,1,0,0);//create label
    view_state_widget((view_widget_arr_only*)new_view,wdg_id_addr);//load wdg to activity
    new_view->tc_content=wdg_id_addr->widget_content;//link content to label str
    return new_view;
}

//view structures setup
view_input_page* view_input_page_setup (char* title, char* defualt_inpt, char* tips)
{
    view_input_page* new_view=(view_input_page*)malloc(sizeof(view_input_page));
    for(short i=0;i<MAX_WIDGET_PER_VIEW;i++)//clean addr, make sure full NULL
        new_view->stated_widget[i]=NULL;
    
    //load title_content widget
    widget_id_t* wdg_id_addr=wdg_lable_setup(0,0,127,16,title,16,ALIGN_TOP_MID,1,0,0);//create label
    view_state_widget((view_widget_arr_only*)new_view,wdg_id_addr);//load wdg to activity
    new_view->ipg_title=wdg_id_addr->widget_content;

    //input textbox
    wdg_id_addr=wdg_lable_setup(20,18,107,45,defualt_inpt,24,ALIGN_MID_MID,0,1,0);//create label
    view_state_widget((view_widget_arr_only*)new_view,wdg_id_addr);
    new_view->ipg_inpt_content=wdg_id_addr->widget_content;
    new_view->ipg_inpt_wdg_id=wdg_id_addr;

    wdg_id_addr=wdg_lable_setup(0,46,127,63,tips,16,ALIGN_TOP_LEFT,1,0,0);//create label
    view_state_widget((view_widget_arr_only*)new_view,wdg_id_addr);
    new_view->ipg_tips=wdg_id_addr->widget_content;
    return new_view;
}