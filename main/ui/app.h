#ifndef APP_H
#define APP_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "fg_reader.h"

#define MSG_BUFF_MAX 24

extern SemaphoreHandle_t g_app_event_sem;
typedef enum hardware_e {FG,UI,DOOR_LOCK,INPT_BTN,INPT_KEYPAD}hardware_e;

typedef struct event_msg_t
{
    uint8_t sender;//to know whos sending so how to handle of msg content
    void* msg_content;
}event_msg_t;

void app_init(void);
bool msg_sent_to_ui(hardware_e sender, void* msg_content,uint8_t content_size);
void ui_event_handler(void *pvParameters);

//hardware stuff
//door lock
typedef enum lock_state{DOOR_RELOCK, DOOR_UNLOCK, DOOR_UNLOCKED}lock_state;
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

#endif