#include <stdlib.h>
#include <string.h>
#include "activity.h"
#include "esp_log.h"

//activity stack functions
activity_stack_t activity_stack;

void activity_init()//send here after declared the stack
{
    activity_stack.cur_activity_depth=0;//set to defualt, empty stack.
    for(short i=0;i<ACTIVITY_STACK_DEPTH_MAX;i++)
        activity_stack.activities[i]=NULL;
}


bool activity_stack_push(activity_t *activity_new)
{
    if(activity_stack.cur_activity_depth<ACTIVITY_STACK_DEPTH_MAX)//check if stack have space(cur depth<max depth)
    {
        activity_stack.activities[activity_stack.cur_activity_depth]=activity_new;//push activity to stack
        activity_stack.cur_activity_depth++;//depth +1
        return true;
    }
    return false;
}

activity_t *activity_stack_peek()//just look, no pop
{
    if(activity_stack.cur_activity_depth!=0)//check not empty
        return activity_stack.activities[activity_stack.cur_activity_depth-1];
    else 
        return NULL;
}

activity_t *activity_stack_pop()
{
    if(activity_stack.cur_activity_depth!=0)//check not empty
    {//reset last slot addr, cur_depth -1
        activity_t *activity_temp=activity_stack.activities[activity_stack.cur_activity_depth-1];
        activity_stack.activities[activity_stack.cur_activity_depth-1]=NULL;
        activity_stack.cur_activity_depth--;
        return activity_temp;
    }
    return NULL;
}

//print stated widgets in view
void activity_print(activity_t* activity_toload)
{
    oled_screen_clear();
    view_print_widget((view_widget_arr_only*)(activity_toload->view_structure));
    oled_screen_update();
}


//activity lifecycle
//create
activity_t* activity_create(enum view_type_e view_type,char* view_name, void* view_structure)
{   //load inputs to struct
    activity_t* new_activity=(activity_t*)malloc(sizeof(activity_t));
    *new_activity=(activity_t){
        .view_type=view_type,
        .view_structure=view_structure
    };
    strncpy(new_activity->view_name, view_name, 24);
    return new_activity;
}

void activity_run(activity_t* to_run)
{
    ESP_LOGI("UI","Activity start:%s",to_run->view_name);
    activity_stack_push(to_run);
    activity_print(to_run);
}

void activity_screen_refresh()//update screen content after changed
{
    activity_print(activity_stack_peek());
}

void activity_back()//back to last activity, remove cur activity and re-load last activity
{
    ESP_LOGI("UI","Activity back:%s",activity_stack_pop()->view_name);
    //activity_stack_pop();
    oled_screen_clear();
    activity_print(activity_stack_peek());
}

void activity_destroy(activity_t* to_destroy)
{
    for(short i=0;i<MAX_WIDGET_PER_VIEW;i++)
    {
        //free all widget of to_destroy activity
        if(((view_widget_arr_only*)to_destroy->view_structure)->stated_widget[i]!=NULL)
            free(((view_widget_arr_only*)to_destroy->view_structure)->stated_widget[i]);
    }
        free(to_destroy->view_structure);//free view and itself
        free(to_destroy);
}