#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "string.h"

#include "app.h"
#include "fg_reader.h"
#include "esp_log.h"

SemaphoreHandle_t g_app_event_sem = NULL;

QueueHandle_t msg_queue;//message sent from hardware
activity_t* activity_fg_reader;
activity_t* activity_door_unlock;
activity_t* activity_psw_inpt;
activity_t* activity_home;
void app_init(void)
{
    msg_queue=xQueueCreate(MSG_BUFF_MAX, sizeof(event_msg_t));

    //prepare ui
    activity_fg_reader=activity_create(VIEW_TITLE_CONTENT,"3",view_title_content_setup(" ", " "));
    activity_door_unlock=activity_create(VIEW_TITLE_CONTENT,"2",view_title_content_setup("", ""));
    activity_psw_inpt=activity_create(VIEW_INPT_PAGE,"4",view_input_page_setup("Password:", "", "[#] to Confirm"));
    activity_home=activity_create(VIEW_TITLE_CONTENT,"1",view_title_content_setup("Hi there", "This door lock support FG print, password. bla bla bla long str"));
    activity_run(activity_home);
}

bool msg_sent_to_ui(hardware_e sender, void* msg_content,uint8_t content_size)
{
    void* new_event_content=(void*)malloc(content_size);//back up data to lifetime-scope
    memcpy(new_event_content, msg_content,content_size);
    if(xQueueSend(msg_queue,(&(event_msg_t){sender,new_event_content}),pdMS_TO_TICKS(100))==pdPASS)//send msg to queue, wait ui to read
        return true;
    ESP_LOGI("UI", "Event msg queue full");
    return false;
}

extern SemaphoreHandle_t g_dr_unlock_sem;
void ui_event_handler(void *pvParameters)
{
    event_msg_t msg_recv;
    char print_buff[WIDGET_CONTENT_MAXSIZE];
    while(1)
    {
        if(xQueueReceive(msg_queue, &msg_recv, portMAX_DELAY))//wait for event
        {
            switch (msg_recv.sender)
            {
            case DOOR_LOCK:
                switch (((doorlock_event_t*)msg_recv.msg_content)->lock_status)
                {
                    case DOOR_UNLOCK:
                        strncpy(((view_title_content_t*)(activity_door_unlock->view_structure))->tc_title,"门已开锁!",32);
                        sprintf(print_buff,"Welcome id:%d",fg_identified_fetch_id());
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
            case FG:
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
                            vTaskDelay(pdMS_TO_TICKS(800));
                            activity_back();
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
                            vTaskDelay(pdMS_TO_TICKS(800));
                            activity_back();
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
                            vTaskDelay(pdMS_TO_TICKS(800));
                            activity_back();
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
                            vTaskDelay(pdMS_TO_TICKS(800));
                            activity_back();
                        }
                        else if (fg_event_content->fg_job_progress==FG_JOB_DONE_FAIL) {
                            strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_title,"Delete Failed",64);
                            activity_screen_refresh();
                            vTaskDelay(pdMS_TO_TICKS(800));
                            activity_back();
                        }
                        break;
                    default:
                        break;
                }
                break;
            case INPT:
                inpt_event_t* inpt_event_content=(inpt_event_t*)msg_recv.msg_content;
                if(activity_stack_peek()==activity_home)//only launch inpt page at home page
                {
                    ((view_input_page*)(activity_psw_inpt->view_structure))->ipg_inpt_content[0]='\0';
                    activity_run(activity_psw_inpt);
                }
                if(inpt_event_content->content=='#')
                {
                    if(strncmp(((view_input_page*)(activity_psw_inpt->view_structure))->ipg_inpt_content,"123456",6)==0)
                    {
                        ESP_LOGI("UI_PSW","Psw ok");
                        activity_back();
                        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                        //send a ticket through signal
                        xSemaphoreGiveFromISR(g_dr_unlock_sem, &xHigherPriorityTaskWoken);//priority check
                        if (xHigherPriorityTaskWoken) {//if tsk_doorLock is highest priority then run now
                            portYIELD_FROM_ISR();
                        }
                    }else
                        activity_back();
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
            }
            free(msg_recv.msg_content);
        }
    }
}