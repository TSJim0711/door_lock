#include "fg_reader.h"
#include "esp_random.h"
//=============================================================//

// 全局常量 ===================================================//
// 命令包定义，固定内容不需要更改数据
uint8_t const UART_EMPTY[1]={0};

uint8_t const FG_BAGHEAD[6] = {0xef,0x01,0xff,0xff,0xff,0xff};      // 包头和地址码

uint8_t const FG_READ_SENSOR[6] = {0x01,0x00,0x03,0x01,0x00,0x05};//capture sensor fg img
uint8_t const FG_GEN_PATTERN[7] = {0x01,0x00,0x04,0x02,0x01,0x00,0x08};//gen pattern from fg img, save to buff1
uint8_t const FG_SEARCH_PATTERN[11] = {0x01,0x00,0x08,0x04,0x01,0x00,0x01,0x00,0xff,0x01,0x0e};//search buff1 pattern from id1 to id 0xff
uint8_t const FG_AUTH_DEF[10] = {0x01, 0x00, 0x07, 0x13, 0x00, 0x00, 0x00, 0x01, 0x00, 0x1c};//auth with code 0x000...1， 把坑占住
uint8_t const FG_LED_ON[10] = {0x01,0x00,0x07,0x3c,0x03,0x03,0x03,0x00,0x00,0x4d};
uint8_t const FG_CLEAR_ALL_FG[10]  = {0x01,0x00,0x03,0x0d,0x00,0x11};// 清空指纹库
//=============================================================//


// 全局变量 ===================================================//
volatile uint8_t s_uart_tx_buf[UART1_MAX_RW_LEN];                // 发送包预存              
volatile uint8_t s_uart_rx_buf[UART1_MAX_RW_LEN]={0};                // 应答包缓存
volatile enum fg_status_e s_fg_state=FG_STATE_IDLE,g_fg_next_state=FG_SEARCH_N_SIGNIN; //what reader wwill do?
volatile short g_v_identified_id=0;
activity_t* g_activity_fg_reader;
//=============================================================//

// 调用变量 ===================================================//
// 引用2个10ms时基，定时器内完成计数
extern volatile unsigned int clk0;                             // 串口发送接收数据超时判断时基
extern volatile unsigned int clk1;                             // 通讯层超时判断时基
//=============================================================//



//=============================================================//
//                                                             //
// 功  能  : fg_getsum()计算校验和                                //
// 输  入  : *p          命令缓存                              //
//           len         计算长度                              //
// 输  出  : sum         累加和                                //
// 备  注  : 无                                                //
//                                                             //
//=============================================================//
uint32_t fg_getsum(volatile const uint8_t tx_buf[], uint16_t len)
{
    uint8_t i;
    uint32_t sum=0;

    for (i=0; i<len; i++)
    {
        sum += tx_buf[i];
    }

    return (sum);
}
//=============================================================//

//=============================================================//
//                                                             //
// 功  能  : FG_Init()初始化UART1, 初始化模块涉及的GPIO              //
// 输  入  : 无                                                //
// 输  出  : 无                                                //
//                                                             //
//=============================================================//
void fg_init(void)
{
        uart_config_t uart_config = {
        .baud_rate = UART1_BAUD,//BAUD rate 57600
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_param_config(UART_PORT_1, &uart_config);//apply above config to uart1
    uart_set_pin(UART_PORT_1, GPIO_FGREAD_TXD, GPIO_FGREAD_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);//set uart1 rx tx pin
    uart_driver_install(UART_PORT_1, UART1_BUFF_SIZE * 2, 0, 0, NULL, 0);//assign buffer memory, uart1 interupt r

    /*
    gpio_reset_pin(GPIO_FGREAD_SX);//finger on seneor alert
    gpio_set_direction(GPIO_FGREAD_SX, GPIO_MODE_INPUT);
    */
    gpio_reset_pin(GPIO_FGREAD_POWER);
    gpio_set_direction(GPIO_FGREAD_POWER, GPIO_MODE_OUTPUT);//pwr for hi performace cal
    gpio_set_level(GPIO_FGREAD_POWER, 1); //cut main power, stay eco mode
}
//=============================================================//



//=============================================================//
//                                                             //
// 功  能  : fg_cmd_transmit()命令通讯                                 //
// 输  入  : send_len         发送长度                       //
// 输  出  : TRUE        发送成功，接收应答包成功              //
//           OVER_TIME_S 发送超时                              //
//           OVER_TIME_R 接收超时                              //
//           FALSE       接收数据错误                          //
// 备  注  : Read write from globe var                         //
//           uart_tx_buf, uart_tx_buf                          //
//                                                             //
//=============================================================//
uint8_t fg_cmd_transmit(volatile const uint8_t tx_buf[], uint8_t send_len, uint32_t timeout)
{
    uint16_t body_len;                                             // 接收应答包包长度计数
    uint16_t body_recv_len; 

    if (send_len > 11) return false;                // 输入参数错误

    if(send_len>0)
    {
        uart_write_bytes(UART_PORT_1, (const uint8_t*)FG_BAGHEAD,sizeof(FG_BAGHEAD));//send content
        uart_write_bytes(UART_PORT_1, (const uint8_t*)tx_buf, send_len);//send content
        ESP_EARLY_LOGI("FG","Cmdcomm start, cmd code:0x%x",tx_buf[3]);
    }

    int sended_len = uart_read_bytes(UART_PORT_1, (uint8_t*)s_uart_rx_buf, 9, pdMS_TO_TICKS(timeout));//receive respond header
    if (sended_len < 9) {
        ESP_EARLY_LOGI("FG","err:timeout or mis-send, respond len:%d",sended_len);
        return false;
    }
    ESP_EARLY_LOGI("FG","Cmdcomm recv respond header len:%d",sended_len);

    // 以下判断表示不处理非应答包数据
    if (s_uart_rx_buf[0] != 0xef) {ESP_EARLY_LOGI("FG","cmdcomm head err"); return false;}                          // 包头错误
    if (s_uart_rx_buf[1] != 0x01) {ESP_EARLY_LOGI("FG","cmdcomm head err"); return false;}                          // 包头错误
    if (s_uart_rx_buf[2] != 0xff) {ESP_EARLY_LOGI("FG","cmdcomm addr err"); return false;}                          // 模块地址错误
    if (s_uart_rx_buf[3] != 0xff) {ESP_EARLY_LOGI("FG","cmdcomm addr err"); return false;}                          // 模块地址错误
    if (s_uart_rx_buf[4] != 0xff) {ESP_EARLY_LOGI("FG","cmdcomm addr err"); return false;}                          // 模块地址错误
    if (s_uart_rx_buf[5] != 0xff) {ESP_EARLY_LOGI("FG","cmdcomm addr err"); return false;}                          // 模块地址错误
    if (s_uart_rx_buf[6] != 0x07) {ESP_EARLY_LOGI("FG","cmdcomm ackp err"); return false;}                          // 应答包包标识错误
    
    body_len = s_uart_rx_buf[7]*256+s_uart_rx_buf[8];                     // 本次应答包包长度
    body_recv_len=uart_read_bytes(UART_PORT_1, (uint8_t *)s_uart_rx_buf+9, body_len, pdMS_TO_TICKS(timeout));//recieve respond body, put after the header
    ESP_EARLY_LOGI("FG","Respond :{%x,%x,%x,%x,%x,%x,%x,%x,%x},{%x,%x,%x,%x...",s_uart_rx_buf[0],s_uart_rx_buf[1],s_uart_rx_buf[2],s_uart_rx_buf[3],s_uart_rx_buf[4],s_uart_rx_buf[5],s_uart_rx_buf[6],s_uart_rx_buf[7],s_uart_rx_buf[8],s_uart_rx_buf[9],s_uart_rx_buf[10],s_uart_rx_buf[11],s_uart_rx_buf[12]);
    ESP_EARLY_LOGI("FG","Respond header pass check. Body len claimed:%d, Body len recv:%d",body_len,body_recv_len);

    if(s_uart_rx_buf[9]==0x13)
        fg_cmd_transmit(FG_AUTH_DEF,sizeof(FG_AUTH_DEF),200);

    if (body_len > body_recv_len) {//received body length shorter than it claimed
        ESP_EARLY_LOGI("FG","Len claimed > recv len, quitting cmdcom.");
        return false;
    }

    // 5. 校验和验证
    unsigned int calc_sum = fg_getsum((uint8_t*)&s_uart_rx_buf[6], body_len + 1); 
    unsigned int recv_sum = (s_uart_rx_buf[8 + body_len - 1] << 8) | s_uart_rx_buf[8 + body_len];

    ESP_EARLY_LOGI("FG", "Respond sum: calc 0x%04x =? recv 0x%04x", calc_sum, recv_sum);
    if (calc_sum != recv_sum)
    {
        ESP_EARLY_LOGI("FG", "Respond sum error");
        return false;
    }
    
    ESP_EARLY_LOGI("FG","Respond succ");
    return true;                                               // 通讯成功
}
//=============================================================//




//=============================================================//
//                                                             //
// 功  能  : fg_sleep()关闭指纹端口                             //
// 输  入  : 无                                                //
// 输  出  : 无                                                //
// 备  注  : 指纹模块断电，关闭串口                            //
//                                                             //
//=============================================================//
void fg_sleep(void)
{
    ESP_EARLY_LOGI("FG","Shut down module");
    gpio_set_level(GPIO_FGREAD_POWER, 0); //cut main power, stay eco mode
}
//=============================================================//




//=============================================================//
//                                                             //
// 功  能  : fg_wake()打开指纹端口                              //
// 输  入  : 无                                                //
// 输  出  : TRUE 打开成功                                     //
//           FALSE 打开失败                                    //
// 备  注  : 初始化串口，打开指纹模块电源                      //
//           等待接收模块上电初始化成功标志0x55                //
//                   //
//           需预先 FG_Init()                                //
//                                                             //
//=============================================================//
uint8_t fg_wake(void)
{
    uart_flush_input(UART_PORT_1);//clear up uart1
    gpio_set_level(GPIO_FGREAD_POWER, 1);//enable high power supply channel
    ESP_EARLY_LOGI("FG","Booting FG Reader=================================");


    //wait 200ms, or get respond
    TickType_t start_tick = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(1000)) 
    {
        //try fetch auto respond, if get 0x55, deviced booted, no need to wait
        uart_read_bytes(UART_PORT_1, (uint8_t *)s_uart_rx_buf, 1, pdMS_TO_TICKS(50));
        if (s_uart_rx_buf[0] == 0x55) 
            break;
    }
    ESP_EARLY_LOGI("FG","powered up finger print reader A");
    fg_cmd_transmit(FG_AUTH_DEF,sizeof(FG_AUTH_DEF),200);
    fg_cmd_transmit(FG_LED_ON,sizeof(FG_LED_ON),200);

    return true;                                          // 打开指纹端口失败
}
//=============================================================//




//=============================================================//
//                                                             //
// 功  能  : fg_enroll()注册指纹                                   //
// 输  入  : id    指定注册指纹存储序号                        //
// 输  出  : TRUE  注册成功                                    //
//           FALSE 失败                                        //
// 备  注  : id有效取值[0,FG_MAX-1]                            //
//                                                             //
//=============================================================//
uint8_t fg_enroll(uint16_t id)
{
    if (id >= FG_MAX) return false;                            // 参数输入错误
    strncpy(((view_title_content_t*)(activity_stack_peek()->view_structure))->tc_title,"Enroll",32);

    s_uart_tx_buf[0] = 0x01;                                             // 包标志，1命令包
    s_uart_tx_buf[1] = 0x00;                                             // 包长度，高8位
    s_uart_tx_buf[2] = 0x08;                                             // 包长度，低8位
    s_uart_tx_buf[3] = 0x31;                                       // 指令码
    s_uart_tx_buf[4] = (uint8_t)id>>8;//FG ID hi byte
    s_uart_tx_buf[5] = (uint8_t)id&0x00ff;//FG ID lo byte
    s_uart_tx_buf[6] = 0x08;//8x touches
    s_uart_tx_buf[7] = 0x00;
    s_uart_tx_buf[8] = 0b00011001;//bit5 no disable(yes) lift fg for next reg, yes disable(no) same finger no multi reg, yes overwrite id, no disable(yes) return intel during process, no preprocess, yes led always on bit 0                                              // 重复登记标志，1允许,0禁止
    uint32_t calc_sum = fg_getsum((uint8_t*)s_uart_tx_buf, 9);                    // 校验和
    s_uart_tx_buf[9] = (uint8_t)(calc_sum >> 8);   // 高 8 位
    s_uart_tx_buf[10] = (uint8_t)(calc_sum & 0xFF); // 低 8 位

    const TickType_t xTimeoutTicks = pdMS_TO_TICKS(5000); //5s内完成
    TickType_t xStartTime = xTaskGetTickCount();
    
    uart_flush_input(UART_PORT_1);//clean up UART1
    uart_write_bytes(UART_PORT_1, (const uint8_t*)FG_BAGHEAD, sizeof(FG_BAGHEAD));
    uart_write_bytes(UART_PORT_1, (const uint8_t*)s_uart_tx_buf, 11);
    
    char print_buff[WIDGET_CONTENT_MAXSIZE];
    ESP_EARLY_LOGI("FG","ADDFG request sent");
    while((xTaskGetTickCount() - xStartTime) < xTimeoutTicks)
    {   
        if(fg_cmd_transmit(UART_EMPTY,0,1000))
        {
            ESP_EARLY_LOGI("FG","Get respond:0x%x,0x%x,0x%x",s_uart_rx_buf[9],s_uart_rx_buf[10],s_uart_rx_buf[11]);
            if(s_uart_rx_buf[10]==0x00)
                printf("System validating ");
            else if(s_uart_rx_buf[10]==0x01)
                printf("Getting image ");
            else if(s_uart_rx_buf[10]==0x02)
                printf("Generating FG character ");
            else if(s_uart_rx_buf[10]==0x03)
                printf("Please lift up you finger because ");
            
            if(s_uart_rx_buf[11]<0xf0 && s_uart_rx_buf[11]>0x00)
                printf("in [%d/8], ", s_uart_rx_buf[11]);
            else if(s_uart_rx_buf[10]==0x04 && s_uart_rx_buf[11]==0xf0)
                printf("Merging as pattern, ");
            else if(s_uart_rx_buf[10]==0x05 && s_uart_rx_buf[11]==0xf1)
                printf("Enroll validating, ");
            else if(s_uart_rx_buf[10]==0x06 && s_uart_rx_buf[11]==0xf2)
                printf("Saving FG Pattern,");
            else if(s_uart_rx_buf[10]==0x00)
            {
                printf(" and launching.\n");
                continue;
            }
            
            if(s_uart_rx_buf[9]==0x00)//something success
            {
                printf("success.\n");
                xStartTime = xTaskGetTickCount();//reset timmer
                if(s_uart_rx_buf[10]==0x03)//tell user retap sensor
                {
                    sprintf(print_buff,"/8] done, pls retap[%d.",s_uart_rx_buf[11]);
                    strncpy(((view_title_content_t*)(activity_stack_peek()->view_structure))->tc_content,print_buff,64);
                    activity_screen_refresh();
                    continue;
                }

                if(s_uart_rx_buf[10]==0x06 && s_uart_rx_buf[11]==0xf2)
                {
                    strncpy(((view_title_content_t*)(activity_stack_peek()->view_structure))->tc_content,"Done. Thank you.",64);
                    activity_screen_refresh();
                    return true;
                }
            }
            else
            {
                if(s_uart_rx_buf[9]==0x1f)
                    printf("fail as storage full.\n");
                else if(s_uart_rx_buf[9]==0x26)
                {
                    printf("fail as timeout.\n");
                    strncpy(((view_title_content_t*)(activity_stack_peek()->view_structure))->tc_content,"Fail: Time out.",64);
                }
                else if(s_uart_rx_buf[9]==0x27)
                {
                    printf("fail as this fingerprint exist in system\n");
                    strncpy(((view_title_content_t*)(activity_stack_peek()->view_structure))->tc_content,"Fail: FG re-enroll.",64);
                }
                else
                {
                    printf("fail.\n");
                    strncpy(((view_title_content_t*)(activity_stack_peek()->view_structure))->tc_content,"Fail, Pls check log.",64);
                }
                activity_screen_refresh();
                return false;
            }  
        }
    }

    return false;                                              // 注册失败
}
//=============================================================//

//read finger from sensor, gen pattern and save to buffer
uint8_t fg_read_sensor()
{   //capture fingerprint image
    if(!fg_cmd_transmit(FG_READ_SENSOR,sizeof(FG_READ_SENSOR),500))
        return false;
    else if(s_uart_rx_buf[9]==02)
    {
        printf("Read fail. Where is ur finger?\n");
        return false;
    }
    else if(s_uart_rx_buf[9]!=00)
    {
        printf("Read fail.\n");
        return false;
    }

    //Gen pattern from fg image
    if(!fg_cmd_transmit(FG_GEN_PATTERN,sizeof(FG_GEN_PATTERN),500))
        return false;
    if(s_uart_rx_buf[9]==00)
        return true;
    else if(s_uart_rx_buf[9]==06)
        printf("Gen Ptrn fail, too mess.\n");
    else if(s_uart_rx_buf[9]==06)
        printf("Gen Ptrn fail, too common.\n");
    return false;
}

//=============================================================//
//                                                             //
// 功  能  : fg_identify()搜索指纹                                //
// 输  入  : 无                                                //
// 输  出  : TRUE  搜索成功                                    //
//           FALSE 失败                                        //
// 备  注  : 无                                                //
//                                                             //
//=============================================================//
uint8_t fg_identify(void)
{
    uint8_t state;
    ESP_EARLY_LOGI("FG","Search try");
    strncpy(((view_title_content_t*)(activity_stack_peek()->view_structure))->tc_title,"Identify",32);
    strncpy(((view_title_content_t*)(activity_stack_peek()->view_structure))->tc_content,"Reading sensor...",64);
    activity_screen_refresh();
    
    uart_flush_input(UART_PORT_1);//clean up UART1
    if(!fg_read_sensor())//load fg to buffer for indfy
        return false;

    state=fg_cmd_transmit(FG_SEARCH_PATTERN,sizeof(FG_SEARCH_PATTERN),500);
    if(state==true)
    {
        if (s_uart_rx_buf[9] == 0x00)                                // 搜索成功
        {
            g_v_identified_id= (s_uart_rx_buf[10] << 8) | s_uart_rx_buf[11];//id that fingerprint reg
            int identified_fg_id_score=(s_uart_rx_buf[12] << 8) | s_uart_rx_buf[13];
            if(identified_fg_id_score>50)
            {
                printf("Reconized FG, id:%d, score:%d\n",g_v_identified_id,identified_fg_id_score);
                return true;
            }
            else
            {
                printf("Not confident FG, id:%d, score:%d\n",g_v_identified_id,identified_fg_id_score);
                strncpy(((view_title_content_t*)(activity_stack_peek()->view_structure))->tc_content,"Not confident with result.",64);
            }
        }                 
        else if (s_uart_rx_buf[9] == 0x09)
        {
            printf("FG never Enroll.\n");
            strncpy(((view_title_content_t*)(activity_stack_peek()->view_structure))->tc_content,"Not reconized.",64);
        }
        else
        {
            printf("Search failed.\n");
            strncpy(((view_title_content_t*)(activity_stack_peek()->view_structure))->tc_content,"Some error occurs.",64);
        }        
        activity_screen_refresh();
        return false;
    };
    return false;
}

unsigned short fg_identified_fetch_id(void)//user id who unlock the door
{
    return g_v_identified_id;
}

//=============================================================//



//=============================================================//
//                                                             //
// 功  能  : fg_del_allfg()清空全部指纹                               //
// 输  入  : 无                                                //
// 输  出  : TRUE  清空成功                                    //
//           FALSE 失败                                        //
// 备  注  : 无                                                //
//                                                             //
//=============================================================//
uint8_t fg_del_allfg(void)
{
    strncpy(((view_title_content_t*)(activity_stack_peek()->view_structure))->tc_title,"Delete FGs",32);
    if(fg_cmd_transmit(FG_CLEAR_ALL_FG,sizeof(FG_CLEAR_ALL_FG),500))                                   // 发送清空命令
    {   
        if(s_uart_rx_buf[9]==0x00)
        {
            strncpy(((view_title_content_t*)(activity_stack_peek()->view_structure))->tc_title,"Delete Success",64);
            activity_screen_refresh();
            printf("Succ clear all fg.\n");
            return true;
        }
    }
    strncpy(((view_title_content_t*)(activity_stack_peek()->view_structure))->tc_title,"Delete Failed",64);
    activity_screen_refresh();
    return false;
}
//=============================================================//

SemaphoreHandle_t g_fg_pressed_sem = NULL;
extern SemaphoreHandle_t g_dr_unlock_sem;
void fg_service (void *pvParameters)
{   
    unsigned char status;

    while(1)
    {
        if (xQueuePeek((QueueHandle_t)g_fg_pressed_sem, NULL, portMAX_DELAY) == pdTRUE) //if have a ticket then go (g_fg_pressed_sem is binary， and ticket won't spend now)
        {
            s_fg_state=g_fg_next_state;//lock in status
            ESP_EARLY_LOGI("FG","Finger pressed on fg reader, will do:%d",s_fg_state);
            fg_wake();
            activity_run(g_activity_fg_reader);
            if(s_fg_state==FG_STATE_IDLE || s_fg_state==FG_STATE_SEARCH||s_fg_state==FG_SEARCH_N_SIGNIN)
            {
                status=fg_identify();
                if(s_fg_state==FG_SEARCH_N_SIGNIN  && status==true)//did match
                {
                    ESP_EARLY_LOGI("FG", "Pass, unlocking door.");
                    //unlock door
                    activity_back();//remove fg read activity
                    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                    //send a ticket through signal
                    xSemaphoreGiveFromISR(g_dr_unlock_sem, &xHigherPriorityTaskWoken);//priority check
                    if (xHigherPriorityTaskWoken) {//if tsk_doorLock is highest priority then run now
                        portYIELD_FROM_ISR();
                    }
                    fg_sleep();//fg readder eco mode
                    vTaskDelay(pdMS_TO_TICKS(800));//Giving time for finger to lift
                    if(xSemaphoreTake(g_fg_pressed_sem, portMAX_DELAY) == pdTRUE) {};//spend 1 ticket
                    continue;
                }
            }
            if(s_fg_state==FG_STATE_ENROLL)//registrate a finger print
            {
                fg_enroll(1);//id 1
            }
            if(s_fg_state==FG_DEL_ALL)
            {
                fg_del_allfg();
            }
            //if(g_fg_status==FG_DEL_CUR)
            //{
            //    fg_identify();//detect what fg id is
            //    DelFG(fg_indentified_fetch_id(),1);//del that
            //}
            fg_sleep();
            vTaskDelay(pdMS_TO_TICKS(800));//Giving time for finger to lift
            activity_back();//back to home activity
        }
        if(xSemaphoreTake(g_fg_pressed_sem, portMAX_DELAY) == pdTRUE) {};//spend 1 ticket
    }
}

