#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "string.h"

#include "app.h"
#include "fg_reader.h"
#include "esp_log.h"
#include "uni_input.h"
#include "view.h"

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

activity_t* activity_manu;
activity_t* activity_fg_mode_set;
activity_t* activity_about;

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
    
    activity_manu=activity_create(VIEW_LIST,"6",view_list_setup("Manu"));
    activity_fg_mode_set=activity_create(VIEW_LIST,"7",view_list_setup("FG Mode Set"));
    view_list_push_content(activity_manu->view_structure, "Manu", ACTION_LAUNCH, sizeof(action_launch_t), &(action_launch_t){activity_fg_mode_set});//addd activity_fg_mode_set to activity_manu list
    view_list_push_content(activity_fg_mode_set->view_structure, "Scan",ACTION_I_SET,sizeof(action_i_set_t),&(action_i_set_t){(void*)&g_fg_next_state,FG_SEARCH_N_SIGNIN});
    view_list_push_content(activity_fg_mode_set->view_structure, "Register",ACTION_I_SET,sizeof(action_i_set_t),&(action_i_set_t){(void*)&g_fg_next_state,FG_STATE_ENROLL});
    view_list_push_content(activity_fg_mode_set->view_structure, "Delete",ACTION_I_SET,sizeof(action_i_set_t),&(action_i_set_t){(void*)&g_fg_next_state,FG_DEL_ALL});
    activity_about=activity_create(VIEW_TITLE_CONTENT,"8",view_title_content_setup("This UI", "Rate: 5****\nRated by me."));
    view_list_push_content(activity_manu->view_structure, "About Project", ACTION_LAUNCH, sizeof(action_launch_t), &(action_launch_t){activity_about});

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
    enum view_type_e cur_activity_view;
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
                            invoke(1000,&activity_back,0);//close 1s later
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
                            invoke(2000,&activity_back,0);
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
                            invoke(1000,&activity_back,0);
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
                            invoke(1000,&activity_back,0);
                        }
                        else if (fg_event_content->fg_job_progress==FG_JOB_DONE_FAIL) {
                            strncpy(((view_title_content_t*)(activity_fg_reader->view_structure))->tc_title,"Delete Failed",64);
                            activity_screen_refresh();
                            invoke(1000,&activity_back,0);
                        }
                        break;
                    default:
                        break;
                }
                break;
            case HW_INPT:
                inpt_event_t* inpt_event_content=(inpt_event_t*)msg_recv.msg_content;
                ESP_LOGI("UI","Recv inpt:%u",inpt_event_content->content);
                if(inpt_event_content->content>=0x80)
                {//control code
                    
                    if(activity_stack_peek()==activity_home)//launch setting screen on home screen
                        if(inpt_event_content->content==INPT_BTN_ENTER)
                        {
                            activity_run(activity_manu);
                            continue;
                        }

                    if(activity_stack_peek()->view_type==VIEW_LIST)//in list view
                    {   
                        if(inpt_event_content->content==INPT_BTN_UP)//select above obj and below obj
                        {
                            view_list_select_shift((view_list*)(activity_stack_peek()->view_structure), -1);
                            activity_screen_refresh();
                        }
                        else if(inpt_event_content->content==INPT_BTN_DOWN)
                        {
                            view_list_select_shift((view_list*)(activity_stack_peek()->view_structure), 1);
                            activity_screen_refresh();
                        }
                        else if(inpt_event_content->content==INPT_BTN_ENTER)
                        {
                            vl_content_t* action_head =((view_list*)(activity_stack_peek()->view_structure))->vl_content_select;
                            switch(action_head->vlc_action_type)
                            {
                                case ACTION_LAUNCH://launch an activity
                                    activity_run(((action_launch_t*)(action_head->vlc_action))->act_activity_launch);
                                    break;
                                case ACTION_I_SET://set a integer var val
                                    *(uint32_t*)(((action_i_set_t*)(action_head->vlc_action))->act_targ_var)=((action_i_set_t*)(action_head->vlc_action))->act_var_val;
                                    break;
                                case ACTION_PTR_SET:
                                    (((action_ptr_set_t*)(action_head->vlc_action))->act_targ_var)=((action_ptr_set_t*)(action_head->vlc_action))->act_var_val;
                                    break;
                            }
                        }
                    }
                    if(inpt_event_content->content==INPT_BTN_RETURN)
                        activity_back();
                }else
                {//normal char
                    if(activity_stack_peek()==activity_home)//only launch inpt page at home page
                    {
                        ((view_input_page*)(activity_psw_inpt->view_structure))->ipg_inpt_content[0]='\0';
                        ESP_LOGI("UI","Launch psw");
                        activity_run(activity_psw_inpt);
                    }
                    if(inpt_event_content->content=='#' && activity_stack_peek()==activity_psw_inpt)
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
                    invoke(1000,&activity_back,0);
                }
                break;
            default:
                break;
            }
            free(msg_recv.msg_content);
        }
    }
}

typedef struct invoke_event_t
{
    TickType_t req_when;
    TickType_t func_call_delay;
    void (*func_call)(void* arg1,void* arg2,void* arg3,void* arg4);
    void* arg[4];
    uint8_t arg_cnt;
}invoke_event_t;
static invoke_event_t to_do_list[16];
static SemaphoreHandle_t x_todo_list_mutex=NULL;
static TickType_t s_awake_cntdwn=portMAX_DELAY;
void system_service(void *pvParameters)
{
    event_msg_t msg_recv;
    auth_event_t* auth_event;
    for(uint8_t i=0;i<16;i++)//init arr
        to_do_list[i].func_call_delay=portMAX_DELAY;
    x_todo_list_mutex=xSemaphoreCreateMutex();
    TickType_t cur_time;
    while(1)
    {
        cur_time=xTaskGetTickCount();
        //ESP_LOGI("SYS","sys wake delay:%u,s_awake_cntdwn:%u,cur_time:%u",s_awake_cntdwn-cur_time,s_awake_cntdwn,cur_time);
        if(xQueueReceive(queue_system, &msg_recv, MAX(s_awake_cntdwn,cur_time)-cur_time))
        {
            if(msg_recv.recver==SERV_AUTH)
            {
                //ESP_LOGI("SYS","CPA1");
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
            }
            free(msg_recv.msg_content);
        }else //handle invoke
        {
            cur_time=xTaskGetTickCount();
            //ESP_LOGI("SYS","CPC1");
            if(!xSemaphoreTake(x_todo_list_mutex,100))
            {
                ESP_LOGI("SYS","Invoke op delayed, taking invoke list mutex failed.");
                s_awake_cntdwn=cur_time+200;//try later if cannot take lock
                continue;
            }
            s_awake_cntdwn=portMAX_DELAY;
            for(uint8_t i=0; i<INVOKE_SLOT_SIZE; i++)
            {
                if(to_do_list[i].func_call_delay!=portMAX_DELAY)
                {
                    if(cur_time-to_do_list[i].req_when<=to_do_list[i].func_call_delay)//when is time
                    {
                        if(to_do_list[i].arg_cnt==0)//run func
                            to_do_list[i].func_call(NULL,NULL,NULL,NULL);
                        else if(to_do_list[i].arg_cnt==1)
                            to_do_list[i].func_call(to_do_list[i].arg[0],NULL,NULL,NULL);
                        else if(to_do_list[i].arg_cnt==2)
                            to_do_list[i].func_call(to_do_list[i].arg[0],to_do_list[i].arg[1],NULL,NULL);
                        else if(to_do_list[i].arg_cnt==3)
                            to_do_list[i].func_call(to_do_list[i].arg[0],to_do_list[i].arg[1],to_do_list[i].arg[2],NULL);
                        else if(to_do_list[i].arg_cnt==4)
                            to_do_list[i].func_call(to_do_list[i].arg[0],to_do_list[i].arg[1],to_do_list[i].arg[2],to_do_list[i].arg[3]);
                        to_do_list[i].func_call_delay=portMAX_DELAY;//set as no task
                    }else //not u now, find if is next earlist task for queue timeout
                    {
                        //ESP_LOGI("SYS","CPC3 i:%d",i);
                        if(to_do_list[i].req_when+to_do_list[i].func_call_delay<s_awake_cntdwn)
                            s_awake_cntdwn=to_do_list[i].req_when+to_do_list[i].func_call_delay-cur_time;
                    }
                }
            }
            xSemaphoreGive(x_todo_list_mutex);
        }
    }
}

//run a func later，something like invoke() in Unity engine >_<!!!
//check arg_count, can't catch this err when arg_count > arg sent
bool invoke(TickType_t call_dalay_ms, void (*func_call)(), int arg_count ,...)
{
    if (arg_count>4)
    {
        ESP_LOGI("SYS","Invoke req dropped, too many arg, %d/4",arg_count);
        return false;
    }
    
    va_list args;
    if(!xSemaphoreTake(x_todo_list_mutex, pdMS_TO_TICKS(100)))
    {
        ESP_LOGI("SYS","Invoke req ignored, taking invoke list mutex failed.");
        return false;
    }
    for(uint8_t i=0; i<INVOKE_SLOT_SIZE; i++)//put to do to empty list
    {
        if(to_do_list[i].func_call_delay==portMAX_DELAY)//occupy a space
        {
            to_do_list[i].arg_cnt=arg_count;//state func arg count
            to_do_list[i].req_when=xTaskGetTickCount();
            to_do_list[i].func_call_delay=pdMS_TO_TICKS(call_dalay_ms);
            if(to_do_list[i].func_call_delay<s_awake_cntdwn)//earlist func to call set as delay
            {
                s_awake_cntdwn=xTaskGetTickCount()+to_do_list[i].func_call_delay;
                ESP_LOGI("SYS","Invoke when:%u",s_awake_cntdwn);
                xQueueSend(queue_system,(&(event_msg_t){SERV_INVOKE,SERV_SYS,NULL}),pdMS_TO_TICKS(100));//send a dummy, refresh system_service queue timeout
            }
            to_do_list[i].func_call=(void(*)(void*,void*,void*,void*))func_call;

            va_start(args, arg_count);
            for(uint16_t arg_idx=0; arg_idx<arg_count; arg_idx++)//load arg
            {
                to_do_list[i].arg[arg_idx]=va_arg(args,void*);
            }
            va_end(args);
            
            xSemaphoreGive(x_todo_list_mutex);
            break;
        }
    }
    return true;
}
