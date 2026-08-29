#ifndef APP_H
#define APP_H

#include <stdarg.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "fg_reader.h"

#define MSG_BUFF_SIZE 24
extern QueueHandle_t queue_system;
extern QueueHandle_t queue_doorlock;
extern SemaphoreHandle_t g_app_event_sem;
typedef enum component_e {HW_FG,SERV_UI,HW_DOOR_LOCK,HW_INPT,SERV_SYS,SERV_AUTH,SERV_INVOKE}component_e;

typedef struct event_msg_t
{
    uint8_t sender;//to know whos sending so how to handle of msg content
    uint8_t recver;
    void* msg_content;
}event_msg_t;

void app_init(void);
bool event_publish(component_e sender, component_e recver, void* msg_content,uint8_t content_size);
void ui_event_handler(void *pvParameters);

//hardware stuff
//door lock
typedef enum lock_state{DOOR_RELOCK, DOOR_UNLOCKED, DOOR_UNLOCK}lock_state;
typedef struct doorlock_event_t
{
    lock_state lock_status;
    uint8_t detail;
}doorlock_event_t;

//fg_reader
typedef enum fg_status_e{FG_INIT,FG_STATE_IDLE,FG_STATE_ENROLL,FG_STATE_SEARCH,FG_SEARCH_N_SIGNIN,FG_DEL_ALL,FG_DEL_CUR}fg_status_e;//待机，注册指纹，判定指纹，删除指纹,删除目前识别到的指纹
typedef enum fg_progress_e{FG_JOB_START,FG_IN_PROGESS,FG_PROGESS_DONE_SUCC,FG_PROGESS_DONE_FAIL,FG_JOB_DONE_SUCC,FG_JOB_DONE_FAIL}fg_progress_e;
typedef enum fg_detail{FG_FAIL_TIMEOUT,FG_FAIL_RE_ENROLL,FG_FAIL_HESITATE,FG_FAIL_NOT_FOUND} fg_detail;
volatile extern enum fg_status_e g_fg_next_state; //s_fg_state may be mutex lock in future, lets change that at very last time 
//fg send ui
typedef struct fg_event_t
{
    fg_status_e fg_status;//what fg reader job right now
    fg_progress_e fg_job_progress;//hows it going
    uint8_t fg_job_detail;//hows it going
}fg_event_t;

//inputs
typedef enum inpt_btn_e{INPT_BTN_ENTER=0x80,INPT_BTN_BACK,INPT_BTN_UP,INPT_BTN_DOWN,INPT_BTN_LEFT,INPT_BTN_RIGHT}inpt_btn_e;//larger than normal ascii
typedef struct inpt_t
{
    char content;
}inpt_event_t;

//sys serv
//psw auth
typedef struct auth_event_t
{
    uint16_t id;
    bool trusted;//id auth and comfirmed by other hardware, eg. fg_reader
    char key_code[16];
    uint8_t relock_cnt_down;
}auth_event_t;
//tells ui next door unlock id is, 
typedef struct auth_result_event_t
{
    bool trusted;
    uint16_t id;
}auth_result_event_t;
typedef struct invoke_event_t
{
    TickType_t run_when;
    void (*func_arg0)();
    void (*func_arg1)(void* arg1);
    void (*func_arg2)(void* arg1,void* arg2);
    void* arg1;
    void* arg2;
}invoke_event_t;
void system_service(void *pvParameters);

#endif