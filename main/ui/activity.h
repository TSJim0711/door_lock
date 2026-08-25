#ifndef ACTIVITY_H
#define ACTIVITY_H

#include "view.h"
#define ACTIVITY_STACK_DEPTH_MAX 24

//activity node
typedef struct activity_t
{
    enum view_type_e view_type;
    char view_name[24];
    void* view_structure;
}activity_t;

//activity stack
typedef struct activity_stack_t
{
    uint8_t cur_activity_depth;
    activity_t *activities[ACTIVITY_STACK_DEPTH_MAX];
}activity_stack_t;

void activity_stack_init();
activity_t *activity_stack_peek();

//activity lifecycle
activity_t* activity_create(enum view_type_e view_type,char* view_name, void* view_structure);
void activity_run(activity_t* to_run);
void activity_screen_refresh();
void activity_back();
void activity_destroy(activity_t* to_destroy);

#endif