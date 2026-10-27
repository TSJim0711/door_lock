#ifndef APP_H
#define APP_H

#include <stdarg.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "fg_reader.h"

#define MSG_BUFF_SIZE 24
#define INVOKE_SLOT_SIZE 16
extern QueueHandle_t queue_system;
extern QueueHandle_t queue_doorlock;
extern SemaphoreHandle_t g_app_event_sem;
typedef enum component_e {NO_ONE=0,PUBLIC,HW_FG,SERV_UI,HW_DOOR_LOCK,HW_INPT,SERV_SYS,SERV_AUTH,SERV_INVOKE,SERV_SYSOP}component_e;
typedef enum event_struct_e {empty_t,doorlock_event_tt,fg_event_tt,inpt_event_tt,auth_event_tt,auth_result_tt,ui_reflex_event_tt,pasw_change_req_tt,serv_sysop_tt}struct_type_e;//double t prevent u me them mix up with type name
typedef enum {SERVOP_DOOR_UNLOCK,SERVOP_PASW_ALTER}serv_sysop_e;
typedef struct event_msg_t
{
    uint8_t sender;//to know whos sending so how to handle of msg content
    uint8_t recver;
    enum event_struct_e event_struct;
    void* msg_content;
}event_msg_t;
//activity cache contains
typedef union uni_type_u
{
    char str[24];
    serv_sysop_e auth_for;
}uni_type_u;

void app_init(void);
bool event_publish(component_e sender,component_e recver, void* msg_content,enum event_struct_e event_struct,uint8_t content_size);
void ui_event_handler(void *pvParameters);
//ui
typedef enum ui_prefab_e {UI_PASW_VALID_POP,UI_PASW_WRONG_POP, UI_ALTER_SUCCS_POP, UI_ALTER_FAIL_POP}ui_prefab;
typedef struct ui_reflex_event_t
{
    enum ui_prefab_e ui_prefab;
    uint16_t data;
}ui_reflex_event_t;

//hardware stuff
//door lock
typedef enum lock_state{DOOR_RELOCK, DOOR_UNLOCKED, DOOR_UNLOCK}lock_state;
typedef struct doorlock_event_t
{
    uint8_t lock_status;
    uint16_t detail;
}doorlock_event_t;//doorlock_event_t struct same as auth_result_t front ;)

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
typedef enum inpt_btn_e{INPT_BTN_ENTER=0x80,INPT_BTN_RETURN,INPT_BTN_UP,INPT_BTN_DOWN,INPT_BTN_LEFT,INPT_BTN_RIGHT}inpt_btn_e;//larger than normal ascii
typedef struct inpt_event_t
{
    uint16_t content;
}inpt_event_t;

//sys serv
//psw auth
void system_service(void *pvParameters);

typedef struct auth_event_t
{
    uint16_t id;
    bool trusted;//id auth and comfirmed by other hardware, eg. fg_reader
    char pasw[16];
    enum component_e auth_result_respond_to;//Null means no need return
    serv_sysop_e auth_for;//tells what to do for further op after auth
    uint8_t valid_pasw_screen_echo;//0 means no need display auth result on screen
    uint8_t wrong_pasw_screen_echo;
}auth_event_t;
typedef struct auth_result_t//if request return auth result
{
    uint8_t pasw_valid;//1 or 0
    uint16_t id;//tells ui next door unlock id is, 
    char pasw[16];
    serv_sysop_e auth_for;
}auth_result_t;
void pasw_auth_activity_launch(serv_sysop_e auth_for);

typedef struct serv_sysop_t//request system operation
{
    char pasw_grant[16];
    serv_sysop_e operation;
    void* op_para; 
    uint8_t succs_screen_echo;//0 means no need display result on screen
    uint8_t fail_screen_echo;
}serv_sysop_t;
typedef struct serv_sysop_pasw_alter_t
{
    char new_pasw[16];
}serv_sysop_pasw_alter_t;

bool invoke(TickType_t call_dalay_ms, void (*func_call)(), int arg_count ,...);



#endif