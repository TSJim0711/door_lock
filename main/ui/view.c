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
    widget_id_t* wdg_id_addr=wdg_lable_setup(0,0,127,24,title,24,0,0);//create label
    view_state_widget((view_widget_arr_only*)new_view,wdg_id_addr);//load wdg to activity
    new_view->tc_title=wdg_id_addr->widget_content;//link title to label str

    wdg_id_addr=wdg_lable_setup(0,28,127,63,content,16,1,0);//create label
    view_state_widget((view_widget_arr_only*)new_view,wdg_id_addr);//load wdg to activity
    new_view->tc_content=wdg_id_addr->widget_content;//link title to label str
    return new_view;
}