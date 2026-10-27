#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "string.h"

#include "app.h"
#include "fg_reader.h"
#include "esp_log.h"
#include "uni_input.h"
#include "view.h"
#include "flashdb.h"

#define MAX(x,y) ((x>y)?x:y)

SemaphoreHandle_t g_app_event_sem = NULL;

QueueHandle_t queue_ui;//event sent to ui
QueueHandle_t queue_system;
QueueHandle_t queue_doorlock;

activity_t* activity_fg_reader;
activity_t* activity_door_unlock;
activity_t* activity_psw_auth;
activity_t* activity_home;
activity_t* activity_popup;

activity_t* activity_manu;
activity_t* activity_fg_mode_set;
activity_t* activity_about;
activity_t* activity_psw_alter;

void system_service(void *pvParameters);
void app_init(void)
{
    queue_ui=xQueueCreate(MSG_BUFF_SIZE, sizeof(event_msg_t));
    queue_system=xQueueCreate(MAX(MSG_BUFF_SIZE/4,1), sizeof(event_msg_t));
    queue_doorlock=xQueueCreate(MAX(MSG_BUFF_SIZE/4,1), sizeof(event_msg_t));
    
    //prepare ui
    activity_fg_reader=activity_create(VIEW_TITLE_CONTENT,"3",view_title_content_setup("", ""));
    activity_door_unlock=activity_create(VIEW_TITLE_CONTENT,"2",view_title_content_setup("", ""));
    activity_psw_auth=activity_create(VIEW_INPT_PAGE,"4",view_input_page_setup(":", "", ""));
    activity_home=activity_create(VIEW_TITLE_CONTENT,"1",view_title_content_setup("Hi there", "This door lock support FG print, password. bla bla bla long str"));
    activity_popup=activity_create(VIEW_TITLE_CONTENT,"5",view_title_content_setup("", ""));
    
    activity_manu=activity_create(VIEW_LIST,"6",view_list_setup("Manu"));
    activity_fg_mode_set=activity_create(VIEW_LIST,"7",view_list_setup("FG Mode Set"));
    view_list_push_content(activity_manu->view_structure, "FG Mode", ACTION_LAUNCH, sizeof(action_launch_t), &(action_launch_t){activity_fg_mode_set});//addd activity_fg_mode_set to activity_manu list
    view_list_push_content(activity_fg_mode_set->view_structure, "Scan",ACTION_I_SET,sizeof(action_i_set_t),&(action_i_set_t){(void*)&g_fg_next_state,FG_SEARCH_N_SIGNIN});
    view_list_push_content(activity_fg_mode_set->view_structure, "Register",ACTION_I_SET,sizeof(action_i_set_t),&(action_i_set_t){(void*)&g_fg_next_state,FG_STATE_ENROLL});
    view_list_push_content(activity_fg_mode_set->view_structure, "Delete",ACTION_I_SET,sizeof(action_i_set_t),&(action_i_set_t){(void*)&g_fg_next_state,FG_DEL_ALL});
    view_list_push_content(activity_manu->view_structure, "Set password", ACTION_AUTH, sizeof(action_auth_t), &(action_auth_t){SERVOP_PASW_ALTER});//start auth progress
    activity_about=activity_create(VIEW_TITLE_CONTENT,"8",view_title_content_setup("This UI", "Rate: 5****\nRated by me."));
    view_list_push_content(activity_manu->view_structure, "About Project", ACTION_LAUNCH, sizeof(action_launch_t), &(action_launch_t){activity_about});
    activity_psw_alter=activity_create(VIEW_INPT_PAGE,"9",view_input_page_setup("New Password:", "", "[#] to Confirm"));
    
    activity_run(activity_home);
}

bool event_publish(component_e sender,component_e recver, void* msg_content,enum event_struct_e event_struct,uint8_t content_size)
{
    void* new_event_content=(void*)malloc(content_size);//back up data to lifetime-scope
    memcpy(new_event_content, msg_content,content_size);
    ESP_EARLY_LOGI("EVENT", "[%d==(%d)=>%d]: %p",sender, event_struct, recver, new_event_content,new_event_content);
    if(recver==SERV_UI)//only ui have private queue
        xQueueSend(queue_ui,(&(event_msg_t){sender,recver,event_struct,new_event_content}),pdMS_TO_TICKS(100));//send msg to queue, wait ui to read
    else if(recver==SERV_AUTH || recver==SERV_INVOKE|| recver==SERV_SYSOP)
        xQueueSend(queue_system,(&(event_msg_t){sender,recver,event_struct,new_event_content}),pdMS_TO_TICKS(100));
    else if(recver==HW_DOOR_LOCK)
        xQueueSend(queue_doorlock,(&(event_msg_t){sender,recver,event_struct,new_event_content}),pdMS_TO_TICKS(100));
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
            //反射式, no logic, just display what event asking
            if(msg_recv.event_struct==ui_reflex_event_tt)
            {
                switch (((ui_reflex_event_t*)msg_recv.msg_content)->ui_prefab)
                {
                    case UI_PASW_VALID_POP:
                        strncpy(((view_title_content_t*)(activity_popup->view_structure))->tc_title,"Continue...",32);
                        strncpy(((view_title_content_t*)(activity_popup->view_structure))->tc_content,"Password valid.",64);
                        break;
                    case UI_PASW_WRONG_POP:
                        strncpy(((view_title_content_t*)(activity_popup->view_structure))->tc_title,"Ops!",32);
                        strncpy(((view_title_content_t*)(activity_popup->view_structure))->tc_content,"You got an wrong password.",64);
                        break;
                    case UI_ALTER_SUCCS_POP:
                        strncpy(((view_title_content_t*)(activity_popup->view_structure))->tc_title,"Done!",32);
                        strncpy(((view_title_content_t*)(activity_popup->view_structure))->tc_content,"Alter sussecss.",64);
                        break;
                    case UI_ALTER_FAIL_POP:
                        strncpy(((view_title_content_t*)(activity_popup->view_structure))->tc_title,"Ops!",32);
                        strncpy(((view_title_content_t*)(activity_popup->view_structure))->tc_content,"Alter failed.",64);
                        break;
                    default:
                    ESP_LOGI("UI", "Unknown prefab");
                    break;
                }
                activity_run(activity_popup);
                invoke(1000,&activity_back,0);
            }
            else switch (msg_recv.sender)
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
                                case ACTION_AUTH:
                                    pasw_auth_activity_launch(((action_auth_t*)(action_head->vlc_action))->auth_for);
                                    break;
                            }
                        }
                    }
                    if(inpt_event_content->content==INPT_BTN_RETURN)
                        activity_back();
                }else
                {//normal char
                    if(activity_stack_peek()==activity_home && inpt_event_content->content!='#')//only launch inpt page at home page and its not comfirm btn
                    {
                        ((view_input_page*)(activity_psw_auth->view_structure))->ipg_inpt_content[0]='\0';
                        ESP_LOGI("UI","Launch psw");
                        pasw_auth_activity_launch(SERVOP_DOOR_UNLOCK);//launch auth activity
                    }
                    if(inpt_event_content->content=='#')
                    {
                        if(activity_stack_peek()==activity_psw_auth)
                        {
                            auth_event_t auth_event={.trusted=false,.valid_pasw_screen_echo=0,.wrong_pasw_screen_echo=1};//not trusted password, need auth service determine
                            strncpy(auth_event.pasw,((view_input_page*)(activity_psw_auth->view_structure))->ipg_inpt_content,16);//fetch pasw from inpt
                            memcpy(&auth_event.auth_for, activity_psw_auth->avtivity_cache, sizeof(serv_sysop_e));//get authfor from activity cache
                            memcpy(&auth_event.auth_result_respond_to, activity_psw_auth->avtivity_cache+sizeof(serv_sysop_e), sizeof(enum component_e));//fetch auth respond to from activity cache 
                            activity_back();
                            ((view_input_page*)(activity_psw_auth->view_structure))->ipg_inpt_content[0]='\0';//clear inpt
                            event_publish(SERV_UI,SERV_AUTH,&auth_event,auth_event_tt,sizeof(auth_event_t));
                        }else if(activity_stack_peek()==activity_psw_alter)//change paswword, second step
                        {
                            ESP_LOGI("UI", "Changing pasw");
                            serv_sysop_t serv_sysop;
                            serv_sysop_pasw_alter_t* serv_sysop_pasw_alter=(serv_sysop_pasw_alter_t*)malloc(sizeof(serv_sysop_pasw_alter_t));
                            serv_sysop.operation=SERVOP_PASW_ALTER;
                            ESP_LOGI("UI", "I set op: %d(real:%d)",serv_sysop.operation,SERVOP_PASW_ALTER);
                            strncpy(serv_sysop.pasw_grant, activity_psw_alter->avtivity_cache, 16);
                            strncpy(serv_sysop_pasw_alter->new_pasw, ((view_input_page*)(activity_psw_alter->view_structure))->ipg_inpt_content, 16);
                            serv_sysop.op_para=serv_sysop_pasw_alter;
                            ((view_input_page*)(activity_psw_auth->view_structure))->ipg_inpt_content[0]='\0';//clear inpt
                            activity_back();
                            event_publish(SERV_UI,SERV_SYSOP,&serv_sysop,serv_sysop_tt,sizeof(serv_sysop_t));
                        }
                    }else if(activity_stack_peek()->view_type==VIEW_INPT_PAGE && inpt_event_content->content>0x20 && inpt_event_content->content<0x80)//if under input page & is ascii char, then inpt
                    {
                        for(uint8_t i=0;i<WIDGET_CONTENT_MAXSIZE-1;i++)
                            if(((view_input_page*)(activity_stack_peek()->view_structure))->ipg_inpt_content[i]=='\0')//move to inpt buff str final
                            {
                                ((view_input_page*)(activity_stack_peek()->view_structure))->ipg_inpt_content[i]=inpt_event_content->content;
                                ((view_input_page*)(activity_stack_peek()->view_structure))->ipg_inpt_content[i+1]='\0';
                                break;
                            }
                        wdg_lable_draw(((view_input_page*)activity_stack_peek()->view_structure)->ipg_inpt_wdg_id);//reprint inpt box only
                        oled_screen_update();
                    }
                }
                break;
            case SERV_AUTH:
                switch (((auth_result_t*)msg_recv.msg_content)->auth_for)
                {
                    case SERVOP_PASW_ALTER: //alter pasword
                        if(((auth_result_t*)msg_recv.msg_content)->pasw_valid)//if inputed pasw is valid
                        {
                            snprintf(activity_psw_alter->avtivity_cache,24,"%s",((auth_result_t*)msg_recv.msg_content)->pasw);//store that valid pasw for changing pasw
                            activity_run(activity_psw_alter);//run it 
                        }
                        break;
                    default:
                        ESP_LOGI("UI", "WTF");
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
    auth_event_t* event_op;
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
            //ESP_LOGI("SYS", "SYS got a message for: %d ",msg_recv.recver);
            if(msg_recv.recver==SERV_AUTH && msg_recv.event_struct==auth_event_tt)//just verify pasw, cur door unlock use this, need to migrate to serv_sys_op
            {
                //ESP_LOGI("SYS","CPA1");
                auth_event_t* event_op=(auth_event_t*)msg_recv.msg_content;
                if(event_op->trusted)
                {
                    event_publish(SERV_AUTH, SERV_UI, &(auth_result_t){true,event_op->id,"",SERVOP_DOOR_UNLOCK},auth_result_tt, sizeof(auth_result_t));
                    event_publish(SERV_AUTH, HW_DOOR_LOCK, &(doorlock_event_t){DOOR_UNLOCK,10},doorlock_event_tt, sizeof(doorlock_event_t));//unlock door directly
                }else
                {
                    auth_result_t auth_result;
                    char* corr_pasw=fdb_kv_get(&kvdb, "doorlock_pasw");
                    ESP_LOGI("SYS","Input pasw:%s",event_op->pasw);
                    ESP_LOGI("SYS","Corr pasw:%s",corr_pasw);
                    
                    if(strcmp(corr_pasw,event_op->pasw)==0)//check if inpt pasw = pasw store in flash
                    {//valid pasw
                        ESP_LOGI("SERV_AUTH","Psw ok");
                        if(event_op->valid_pasw_screen_echo==true)//display auth result on screen if needed
                            event_publish(SERV_AUTH, SERV_UI, &(ui_reflex_event_t){UI_PASW_VALID_POP,0}, ui_reflex_event_tt,sizeof(ui_reflex_event_t));
                        auth_result.pasw_valid=true;
                    }else
                    {
                        ESP_LOGI("SERV_AUTH","Psw err");
                        if (event_op->wrong_pasw_screen_echo==true)
                            event_publish(SERV_AUTH, SERV_UI, &(ui_reflex_event_t){UI_PASW_WRONG_POP,0}, ui_reflex_event_tt,sizeof(ui_reflex_event_t));
                        auth_result.pasw_valid=false;
                    }
                    auth_result.auth_for=event_op->auth_for;
                    snprintf(auth_result.pasw, 16,"%s",event_op->pasw);
                    ESP_LOGI("SERV_AUTH","Send result to: %d, auth for:%d",event_op->auth_result_respond_to,auth_result.auth_for);
                    if(event_op->auth_result_respond_to!=NO_ONE)//send auth result to needed
                        event_publish(SERV_AUTH, event_op->auth_result_respond_to, &auth_result, auth_result_tt,sizeof(auth_result_t));
                }
            }else if(msg_recv.recver==SERV_SYSOP && msg_recv.event_struct==serv_sysop_tt)
            {
                serv_sysop_t* event_op=(serv_sysop_t*)msg_recv.msg_content;
                ESP_LOGI("SYSOP", "SYS Op: %d, content:%p",((serv_sysop_t*)msg_recv.msg_content)->operation,((serv_sysop_t*)msg_recv.msg_content));
                switch (event_op->operation)//understand what op to do 
                {
                    case SERVOP_PASW_ALTER:
                        char* pasw_old = fdb_kv_get(&kvdb, "doorlock_pasw");
                        //ESP_LOGI("SYSOP", "SYSOP will %s change pasw, (%s=?%s) -> %s",(strcmp(pasw_old,event_op->pasw_grant)==0?"":"not"),pasw_old,event_op->pasw_grant,((serv_sysop_pasw_alter_t*)(event_op->op_para))->new_pasw);
                        if(strcmp(pasw_old,event_op->pasw_grant)==0)//comfirm with auth
                        {
                            ESP_LOGI("SYSOP", "SYS Op Changing pasw");
                            if(fdb_kv_set(&kvdb, "doorlock_pasw",((serv_sysop_pasw_alter_t*)event_op->op_para)->new_pasw)==FDB_NO_ERR)//overwrite as new pasw;
                                event_publish(SERV_AUTH, SERV_UI, &(ui_reflex_event_t){UI_ALTER_SUCCS_POP,0}, ui_reflex_event_tt,sizeof(ui_reflex_event_t));
                            else 
                                event_publish(SERV_AUTH, SERV_UI, &(ui_reflex_event_t){UI_ALTER_FAIL_POP,0}, ui_reflex_event_tt,sizeof(ui_reflex_event_t));
                        }
                        else
                            event_publish(SERV_AUTH, SERV_UI, &(ui_reflex_event_t){UI_PASW_WRONG_POP,0}, ui_reflex_event_tt,sizeof(ui_reflex_event_t));//not granted
                        break;
                    case SERVOP_DOOR_UNLOCK:
                        //SERV_AUTH can handle it, idk why, but it just can (?_?), leave it blank
                        break;
                }
                if(event_op->op_para!=NULL)
                        free(event_op->op_para);//free after malloc
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
                ESP_LOGI("SYS","Invoke when: %u",s_awake_cntdwn);
                xQueueSend(queue_system,(&(event_msg_t){SERV_INVOKE,SERV_SYS,empty_t,NULL}),pdMS_TO_TICKS(100));//send a dummy, refresh system_service queue timeout
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

void pasw_auth_activity_launch(serv_sysop_e auth_for)
{
    //set up onscreen display txt
    enum component_e respond_to=NO_ONE;
    if(auth_for==SERVOP_DOOR_UNLOCK)
    {
        strlcpy(((view_input_page*)(activity_psw_auth->view_structure))->ipg_title,"Password:",64);
        strlcpy(((view_input_page*)(activity_psw_auth->view_structure))->ipg_tips,"[#] to Confirm:",64);
        respond_to=HW_DOOR_LOCK;
    }
    else if(auth_for==SERVOP_PASW_ALTER)
    {
        strlcpy(((view_input_page*)(activity_psw_auth->view_structure))->ipg_title,"Old password:",64);
        strlcpy(((view_input_page*)(activity_psw_auth->view_structure))->ipg_tips,"[#] to Confirm:",64);
        respond_to=SERV_UI;
    }
    memcpy(activity_psw_auth->avtivity_cache,&auth_for,sizeof(serv_sysop_e));//store respond targ for further op
    memcpy(activity_psw_auth->avtivity_cache+sizeof(serv_sysop_e),&respond_to,sizeof(component_e));//store respond to to cache, cache size 24 enough.
    activity_run(activity_psw_auth);//launch pasw inpt activity
}