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

//list_setup
view_list* view_list_setup(char* title)
{
    view_list* new_view=(view_list*)malloc(sizeof(view_list));
    for(short i=0;i<MAX_WIDGET_PER_VIEW;i++)//clean addr, make sure full NULL
        new_view->stated_widget[i]=NULL;

    widget_id_t* wdg_id_addr=wdg_lable_setup(0,0,127,16,title,16,ALIGN_TOP_MID,1,0,1);//create label
    view_state_widget((view_widget_arr_only*)new_view,wdg_id_addr);
    new_view->list_title=wdg_id_addr->widget_content;

    for(uint8_t i=15;i<16*3;i+=16)
    {
        //ESP_LOGI("L_V","List content y:%d",i);
        widget_id_t* wdg_id_addr=wdg_lable_setup(0,i,127,i+16,"",16,ALIGN_TOP_LEFT,(i==31?0:1),(i==31?1:0),0);//content box, middle content box be select
        view_state_widget((view_widget_arr_only*)new_view,wdg_id_addr);
    }
    strncpy(new_view->stated_widget[2]->widget_content,"List no content",64);
    
    new_view->vl_content_select=NULL;
    new_view->vl_content_tail=NULL;
    return new_view;
}

//update label text after content select changed
void view_list_widget_sycn(view_list* list_push_to)
{
    //stated_widget[1,2,3] is contant title label
    if(list_push_to->vl_content_select!=NULL)
    {
        strlcpy(list_push_to->stated_widget[2]->widget_content,list_push_to->vl_content_select->vlc_title,64);
        //ESP_LOGI("V_L","View list content cur:%s;",list_push_to->stated_widget[2]->widget_content);
    }else//no element in list
    {
        strlcpy(list_push_to->stated_widget[2]->widget_content,"Empty List",64);
        strlcpy(list_push_to->stated_widget[1]->widget_content,"",64);
        strlcpy(list_push_to->stated_widget[3]->widget_content,"",64);
        return;
    }
    if(list_push_to->vl_content_select->vlc_prev_content!=NULL)
    {
        strlcpy(list_push_to->stated_widget[1]->widget_content,list_push_to->vl_content_select->vlc_prev_content->vlc_title,64);
        //ESP_LOGI("V_L","View list content up:%s;",list_push_to->stated_widget[1]->widget_content);
    }else
        strlcpy(list_push_to->stated_widget[1]->widget_content,"",64);
    if(list_push_to->vl_content_select->vlc_next_content!=NULL)
    {
        strlcpy(list_push_to->stated_widget[3]->widget_content,list_push_to->vl_content_select->vlc_next_content->vlc_title,64);
        //ESP_LOGI("V_L","View list content down:%s;",list_push_to->stated_widget[3]->widget_content);
    }else
        strlcpy(list_push_to->stated_widget[3]->widget_content,"",64);
}

bool view_list_push_content(view_list* list_push_to,char* title, enum action_type_e action_type,uint8_t action_type_size ,void* action)
{
    vl_content_t* new_content=(vl_content_t*)malloc(sizeof(vl_content_t));
    if(new_content==NULL)
        return false;
    new_content->vlc_action_type=action_type;
    void* action_lifetime=(void*)malloc(action_type_size);//action will be free after init progress, save it
    if(action_lifetime==NULL)
    {
        free (new_content);
        return false;
    }
    memcpy(action_lifetime, action, action_type_size);
    new_content->vlc_action=action_lifetime;
    char* title_lifetime=(char*)malloc(strlen(title)+1);//move title str to lifetime storage
    strlcpy(title_lifetime, title, sizeof(title_lifetime));
    new_content->vlc_title=title_lifetime;
    new_content->vlc_next_content=NULL;
    new_content->vlc_prev_content=list_push_to->vl_content_tail;//get list content chain tail as prev
    if(list_push_to->vl_content_tail==NULL)
        list_push_to->vl_content_select=new_content;//if first content, let cur content be select
    else
        list_push_to->vl_content_tail->vlc_next_content=new_content;
    list_push_to->vl_content_tail=new_content;
    view_list_widget_sycn(list_push_to);
    return true;
}

void view_list_select_shift(view_list* list_shifting,signed char dir)
{
    if(dir==-1&&list_shifting->vl_content_select->vlc_prev_content)
        list_shifting->vl_content_select=list_shifting->vl_content_select->vlc_prev_content;
    else if(dir==1&&list_shifting->vl_content_select->vlc_next_content)
        list_shifting->vl_content_select=list_shifting->vl_content_select->vlc_next_content;
    view_list_widget_sycn(list_shifting);
}