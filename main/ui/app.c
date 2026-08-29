#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "string.h"

#include "app.h"
#include "fg_reader.h"
#include "esp_log.h"

#define MAX(x,y) ((x>y)?x:y)

SemaphoreHandle_t g_app_event_sem = NULL;

QueueHandle_t queue_ui;//event sent to ui
QueueHandle_t queue_system;
QueueHandle_t queue_doorlock;

activity_t* activity_fg_reader;
activity_t* activity_door_unlock;
activity_t* activity_psw_inpt;
activity_t* activity_home;
activity_t* activity_popup;

void system_service(void *pvParameters);
void app_init(void)
{
    queue_ui=xQueueCreate(MSG_BUFF_SIZE, sizeof(event_msg_t));
    queue_system=xQueueCreate(MAX(MSG_BUFF_SIZE/4,1), sizeof(event_msg_t));
    queue_doorlock=xQueueCreate(MAX(MSG_BUFF_SIZE/4,1), sizeof(event_msg_t));
    
    //prepare ui
    activity_fg_reader=activity_create(VIEW_TITLE_CONTENT,"3",view_title_content_setup("", ""));
    activity_door_unlock=activity_create(VIEW_TITLE_CONTENT,"2",view_title_content_setup("", ""));
    activity_psw_inpt=activity_create(VIEW_INPT_PAGE,"4",view_input_page_setup("Password:", "", "[#] to Confirm"));
    activity_home=activity_create(VIEW_TITLE_CONTENT,"1",view_title_content_setup("Hi there", "This door lock support FG print, password. bla bla bla long str"));
    activity_popup=activity_create(VIEW_TITLE_CONTENT,"5",view_title_content_setup("", ""));
        
    activity_run(activity_home);
}

bool event_publish(component_e sender,component_e recver, void* msg_content,uint8_t content_size)
{
    void* new_event_content=(void*)malloc(content_size);//back up data to lifetime-scope
    memcpy(new_event_content, msg_content,content_size);
    if(recver==SERV_UI)//only ui have private queue
        xQueueSend(queue_ui,(&(event_msg_t){sender,recver,new_event_content}),pdMS_TO_TICKS(100));//send msg to queue, wait ui to read
    else if(recver==SERV_AUTH || recver==SERV_INVOKE)
        xQueueSend(queue_system,(&(event_msg_t){sender,recver,new_event_content}),pdMS_TO_TICKS(100));
    else if(recver==HW_DOOR_LOCK)
        xQueueSend(queue_doorlock,(&(event_msg_t){sender,recver,new_event_content}),pdMS_TO_TICKS(100));
    else
        return false;
    return true;
}

extern SemaphoreHandle_t g_dr_unlock_sem;
void ui_event_handler(void *pvParameters)
{
    event_msg_t msg_recv;
    uint16_t unlock_id=0;
    char print_buff[WIDGET_CONTENT_MAXSIZE];
    while(1)
    {
        if(xQueueReceive(queue_ui, &msg_recv, portMAX_DELAY))//wait for event
        {
            switch (msg_recv.sender)
            {
            case HW_DOOR_LOCK:
                switch (((doorlock_event_t*)msg_recv.msg_content)->lock_status)
                {
                    case DOOR_UNLOCK:
                        strncpy(((view_title_content_t*)(activity_door_unlock->view_structure))->tc_title,"门已开锁!",32);
                        if(unlock_id==0)
                            sprintf(print_buff,"Welcome public.");
                        else
                            sprintf(print_buff,"Welcome id:%d",unlock_id);
                        strncpy(((view_title_content_t*)(activity_door_unlock->view_structure))->tc_content,print_buff,64);
                        activity_run(activity_door_unlock);
                        break;
                    case DOOR_UNLOCKED:
                        sprintf(print_buff,"%ds left",((doorlock_event_t*)msg_recv.msg_content)->detail);
                        strncpy(((view_title_content_t*)(activity_door_unlock->view_structure))->tc_content,print_buff,64);
                        activity_screen_refresh();
                        break;
                    case DOOR_RELOCK:
                        activity_back();
                        break;
                };
                break;
            case HW_FG:
                fg_event_t* fg_event_content=(fg_event_t*)msg_recv.msg_content;
                //fingerprint sensor
                switch (fg_event_content->fg_status) 
                {//translate from common type to self msg_fg_t struct
                    case FG_STATE_ENROLL:
                        if(fg_event_content->fg_job_progress==FG_JOB_START)
                        {
                            strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_title,"Enrolling'",32);
                            strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_title,"Pls wait'",32);
                            activity_run(activity_fg_reader);
                        }
                        else if(fg_event_content->fg_job_progress==FG_PROGESS_DONE_SUCC)
                        {
                            sprintf(print_buff,"[%d/8] done, pls retap.",fg_event_content->fg_job_detail);
                            strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_content,print_buff,64);
                            activity_screen_refresh();
                        }
                        else if(fg_event_content->fg_job_progress==FG_JOB_DONE_SUCC)
                        {
                            strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_content,"Done. Thank you.",64);
                            activity_screen_refresh();
                            event_publish(SERV_UI,SERV_INVOKE,&(invoke_event_t){xTaskGetTickCount()+pdMS_TO_TICKS(1000),&activity_back,NULL,NULL,NULL,NULL},sizeof(invoke_event_t));//close 1s later
                        }
                        else if(fg_event_content->fg_job_progress==FG_PROGESS_DONE_FAIL)
                        {
                            if(fg_event_content->fg_job_detail==FG_FAIL_TIMEOUT)
                                strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_content,"Fail: Time out.",64);
                            else if(fg_event_content->fg_job_detail==FG_FAIL_RE_ENROLL)
                                strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_content,"Fail: FG re-enroll.",64);
                            else
                                strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_content,"Fail, Pls check log.",64);
                            activity_screen_refresh();
                            event_publish(SERV_UI,SERV_INVOKE,&(invoke_event_t){xTaskGetTickCount()+pdMS_TO_TICKS(2000),&activity_back,NULL,NULL,NULL,NULL},sizeof(invoke_event_t));//close 1s later
                        }
                        break;
                    case FG_SEARCH_N_SIGNIN:
                        if(fg_event_content->fg_job_progress==FG_JOB_START)
                        {
                            strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_title,"Identifyn'",32);
                            strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_content,"Scanning...",64);
                            activity_run(activity_fg_reader);
                        }
                        else if(fg_event_content->fg_job_progress==FG_JOB_DONE_SUCC)
                        {
                            activity_back();
                        }
                        else if(fg_event_content->fg_job_progress==FG_JOB_DONE_FAIL)
                        {
                            if(fg_event_content->fg_job_detail==FG_FAIL_HESITATE)
                                strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_content,"Not confident with result.",64);
                            else if(fg_event_content->fg_job_detail==FG_FAIL_NOT_FOUND)
                                strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_content,"Not reconized.",64);
                            else
                                strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_content,"Some error occurs.",64);
                            activity_screen_refresh();
                            event_publish(SERV_UI,SERV_INVOKE,&(invoke_event_t){xTaskGetTickCount()+pdMS_TO_TICKS(1000),&activity_back,NULL,NULL,NULL,NULL},sizeof(invoke_event_t));//close 1s later
                        }
                        break;
                    case FG_DEL_ALL:
                        if(fg_event_content->fg_job_progress==FG_JOB_START)
                        {
                            strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_title,"Deleting",32);
                            strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_content,"All fingerprint enrolled would delete.",64);
                            activity_run(activity_fg_reader);
                        }
                        else if (fg_event_content->fg_job_progress==FG_JOB_DONE_SUCC) {
                            strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_title,"Delete Success",64);
                            activity_screen_refresh();
                            event_publish(SERV_UI,SERV_INVOKE,&(invoke_event_t){xTaskGetTickCount()+pdMS_TO_TICKS(1000),&activity_back,NULL,NULL,NULL,NULL},sizeof(invoke_event_t));//close 1s later
                        }
                        else if (fg_event_content->fg_job_progress==FG_JOB_DONE_FAIL) {
                            strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_title,"Delete Failed",64);
                            activity_screen_refresh();
                            event_publish(SERV_UI,SERV_INVOKE,&(invoke_event_t){xTaskGetTickCount()+pdMS_TO_TICKS(1000),&activity_back,NULL,NULL,NULL,NULL},sizeof(invoke_event_t));//close 1s later

                        }
                        break;
                    default:
                        break;
                }
                break;
            case HW_INPT:
                inpt_event_t* inpt_event_content=(inpt_event_t*)msg_recv.msg_content;
                if(activity_stack_peek()==activity_home)//only launch inpt page at home page
                {
                    ((view_input_page*)(activity_psw_inpt->view_structure))->ipg_inpt_content[0]='\0';
                    activity_run(activity_psw_inpt);
                }
                if(inpt_event_content->content=='#')
                {
                    activity_back();
                    auth_event_t auth_event={.trusted=false};//not trusted password, need auth service determine
                    strncpy(auth_event.key_code,((view_input_page*)(activity_psw_inpt->view_structure))->ipg_inpt_content,15);
                    event_publish(SERV_UI,SERV_AUTH,&auth_event,sizeof(auth_event_t));
                }else if(activity_stack_peek()==activity_psw_inpt && inpt_event_content->content>0x20 && inpt_event_content->content<0x80)//if under input page & is ascii char, then inpt
                {
                    for(uint8_t i=0;i<WIDGET_CONTENT_MAXSIZE-1;i++)
                        if(((view_input_page*)(activity_psw_inpt->view_structure))->ipg_inpt_content[i]=='\0')//move to inpt buff str final
                        {
                            ((view_input_page*)(activity_psw_inpt->view_structure))->ipg_inpt_content[i]=inpt_event_content->content;
                            ((view_input_page*)(activity_psw_inpt->view_structure))->ipg_inpt_content[i+1]='\0';
                            break;
                        }
                    wdg_lable_draw(((view_input_page*)activity_psw_inpt->view_structure)->ipg_inpt_wdg_id);//reprint inpt box only
                    oled_screen_update();
                }
                break;
            case SERV_AUTH:
                auth_result_event_t* auth_result_content=(auth_result_event_t*)msg_recv.msg_content;
                if(auth_result_content->trusted==true)
                    unlock_id=auth_result_content->id;
                else//psw err
                {
                    strncpy(((view_title_content_t*)(activity_popup->view_structure))->tc_title,"Ops!",32);
                    strncpy(((view_title_content_t*)(activity_popup->view_structure))->tc_content,"You got an wrong password.",64);
                    activity_run(activity_popup);
                    event_publish(SERV_UI,SERV_INVOKE,&(invoke_event_t){xTaskGetTickCount()+pdMS_TO_TICKS(1000),&activity_back,NULL,NULL,NULL,NULL},sizeof(invoke_event_t));//close 1s later
                }
                break;
            default:
                break;
            }
            free(msg_recv.msg_content);
        }
    }
}

invoke_event_t to_do_list[16];
void system_service(void *pvParameters)
{
    event_msg_t msg_recv;
    auth_event_t* auth_event;
    TickType_t cur_time;
    for(uint8_t i=0;i<16;i++)//init arr
        to_do_list[i].run_when=portMAX_DELAY;
    while(1)
    {
    if(xQueueReceive(queue_system, &msg_recv, pdMS_TO_TICKS(100)))
        {
            if(msg_recv.recver==SERV_AUTH)
            {
                ESP_LOGI("SYS","CPA1");
                auth_event=(auth_event_t*)msg_recv.msg_content;
                if(auth_event->trusted)
                {
                    event_publish(SERV_AUTH, SERV_UI, &(auth_result_event_t){true,auth_event->id}, sizeof(auth_result_event_t));
                    event_publish(SERV_AUTH, HW_DOOR_LOCK, &(doorlock_event_t){DOOR_UNLOCK,10}, sizeof(doorlock_event_t));//unlock door directly
                }else if(strncmp(auth_event->key_code, "123456", 7)==0)
                {
                    ESP_LOGI("SERV_AUTH","Psw ok");
                    event_publish(SERV_AUTH, SERV_UI, &(auth_result_event_t){true,0}, sizeof(auth_result_event_t));
                    event_publish(SERV_AUTH, HW_DOOR_LOCK, &(doorlock_event_t){DOOR_UNLOCK,10}, sizeof(doorlock_event_t));
                }else
                {
                    event_publish(SERV_AUTH, SERV_UI, &(auth_result_event_t){false,0}, sizeof(auth_result_event_t));//notify psw err
                }
            } else if(msg_recv.recver==SERV_INVOKE)//run a func later， something like invoke() in Unity engine
            {
                for(uint8_t i=0; i<16; i++)//put to do to empty list
                {
                    if(to_do_list[i].run_when==portMAX_DELAY)//occupy a space
                    {
                        to_do_list[i]=*(invoke_event_t*)(msg_recv.msg_content);//copy to list
                        break;
                    }
                }
            }
            free(msg_recv.msg_content);
        }else //handle 
        {
            ESP_LOGI("SYS","CPC1");
            cur_time=xTaskGetTickCount();
            for(uint8_t i=0; i<16; i++)
                if(to_do_list[i].run_when!=portMAX_DELAY && to_do_list[i].run_when<cur_time)//when is time
                {
                    ESP_LOGI("SYS","CPC2 i:%d",i);
                    if(to_do_list[i].func_arg0!=NULL)//run func
                        to_do_list[i].func_arg0();
                    else if(to_do_list[i].func_arg1!=NULL)
                        to_do_list[i].func_arg1(to_do_list[i].arg1);
                    else if(to_do_list[i].func_arg2!=NULL)
                        to_do_list[i].func_arg2(to_do_list[i].arg1,to_do_list[i].arg2);

                    to_do_list[i].run_when=portMAX_DELAY;//set as no task
                }
        }
    }
}
