#include "fg_reader.h"
#include "esp_random.h"
//=============================================================//

// 全局常量 ===================================================//
// 命令包定义，固定内容不需要更改数据
uchar const EMPTY[1]={0};

uchar const BAGHEAD[6] = {0xef,0x01,0xff,0xff,0xff,0xff};      // 包头和地址码

uchar const READ_SENSOR[6] = {0x01,0x00,0x03,0x01,0x00,0x05};//capture sensor fg img
uchar const GEN_PATTERN[7] = {0x01,0x00,0x04,0x02,0x01,0x00,0x08};//gen pattern from fg img, save to buff1
uchar const SEARCH_PATTERN[11] = {0x01,0x00,0x08,0x04,0x01,0x00,0x01,0x00,0xff,0x01,0x0e};//search buff1 pattern from id1 to id 0xff
uchar const AUTH_DEF[10] = {0x01, 0x00, 0x07, 0x13, 0x00, 0x00, 0x00, 0x01, 0x00, 0x1c};//auth with code 0x000...1， 把坑占住
uchar const LED_ON[10] = {0x01,0x00,0x07,0x3c,0x03,0x03,0x03,0x00,0x00,0x4d};
uchar const CLEAR_ALL_FG[10]  = {0x01,0x00,0x03,0x0d,0x00,0x11};// 清空指纹库
//=============================================================//


// 全局变量 ===================================================//
volatile uchar uart_tx_buf[UART1_MAX_RW_LEN];                // 发送包预存              
volatile uchar uart_rx_buf[UART1_MAX_RW_LEN]={0};                // 应答包缓存
volatile enum fg_status_list g_fg_status=FG_BORED,g_fg_status_2B=FG_PEND_N_SIGNIN; //what reader wwill do?
volatile short searched_fg_id=0;
//=============================================================//

// 调用变量 ===================================================//
// 引用2个10ms时基，定时器内完成计数
extern volatile unsigned int clk0;                             // 串口发送接收数据超时判断时基
extern volatile unsigned int clk1;                             // 通讯层超时判断时基
//=============================================================//



//=============================================================//
//                                                             //
// 功  能  : GetSum()计算校验和                                //
// 输  入  : *p          命令缓存                              //
//           len         计算长度                              //
// 输  出  : sum         累加和                                //
// 备  注  : 无                                                //
//                                                             //
//=============================================================//
uint GetSum(volatile const uchar tx_buf[],uint len)
{
    uchar i;
    uint sum=0;

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
    gpio_set_level(GPIO_FGREAD_POWER, 0); //cut main power, stay eco mode
}
//=============================================================//




//=============================================================//
//                                                             //
// 功  能  : CmdComm()命令通讯                                 //
// 输  入  : send_len         发送长度                       //
// 输  出  : TRUE        发送成功，接收应答包成功              //
//           OVER_TIME_S 发送超时                              //
//           OVER_TIME_R 接收超时                              //
//           FALSE       接收数据错误                          //
// 备  注  : Read write from globe var                         //
//           uart_tx_buf, uart_tx_buf                          //
//                                                             //
//=============================================================//
uchar CmdComm(volatile const uchar tx_buf[], uchar send_len, uint timeout)
{
    uint body_len;                                             // 接收应答包包长度计数
    uint body_recv_len; 

    if (send_len > 11) return FALSE;                // 输入参数错误

    if(send_len>0)
    {
        uart_write_bytes(UART_PORT_1, (const uchar*)BAGHEAD,sizeof(BAGHEAD));//send content
        uart_write_bytes(UART_PORT_1, (const uchar*)tx_buf, send_len);//send content
        ESP_EARLY_LOGI("FG","Cmdcomm start, cmd code:0x%x",tx_buf[3]);
    }

    int sended_len = uart_read_bytes(UART_PORT_1, (uchar*)uart_rx_buf, 9, pdMS_TO_TICKS(timeout));//recieve respond header
    if (sended_len < 9) {
        ESP_EARLY_LOGI("FG","err:timeout or mis-send, respond len:%d",sended_len);
        return false;
    }
    ESP_EARLY_LOGI("FG","Cmdcomm recv respond header len:%d",sended_len);

    // 以下判断表示不处理非应答包数据
    if (uart_rx_buf[0] != 0xef) {ESP_EARLY_LOGI("FG","cmdcomm head err"); return FALSE;}                          // 包头错误
    if (uart_rx_buf[1] != 0x01) {ESP_EARLY_LOGI("FG","cmdcomm head err"); return FALSE;}                          // 包头错误
    if (uart_rx_buf[2] != 0xff) {ESP_EARLY_LOGI("FG","cmdcomm addr err"); return FALSE;}                          // 模块地址错误
    if (uart_rx_buf[3] != 0xff) {ESP_EARLY_LOGI("FG","cmdcomm addr err"); return FALSE;}                          // 模块地址错误
    if (uart_rx_buf[4] != 0xff) {ESP_EARLY_LOGI("FG","cmdcomm addr err"); return FALSE;}                          // 模块地址错误
    if (uart_rx_buf[5] != 0xff) {ESP_EARLY_LOGI("FG","cmdcomm addr err"); return FALSE;}                          // 模块地址错误
    if (uart_rx_buf[6] != 0x07) {ESP_EARLY_LOGI("FG","cmdcomm ackp err"); return FALSE;}                          // 应答包包标识错误
    
    body_len = uart_rx_buf[7]*256+uart_rx_buf[8];                     // 本次应答包包长度
    body_recv_len=uart_read_bytes(UART_PORT_1, (uchar *)uart_rx_buf+9, body_len, pdMS_TO_TICKS(timeout));//recieve respond body, put after the header
    ESP_EARLY_LOGI("FG","Respond :{%x,%x,%x,%x,%x,%x,%x,%x,%x},{%x,%x,%x,%x...",uart_rx_buf[0],uart_rx_buf[1],uart_rx_buf[2],uart_rx_buf[3],uart_rx_buf[4],uart_rx_buf[5],uart_rx_buf[6],uart_rx_buf[7],uart_rx_buf[8],uart_rx_buf[9],uart_rx_buf[10],uart_rx_buf[11],uart_rx_buf[12]);
    ESP_EARLY_LOGI("FG","Respond header pass check. Body len claimed:%d, Body len recv:%d",body_len,body_recv_len);

    if(uart_rx_buf[9]==0x13)
        CmdComm(AUTH_DEF,sizeof(AUTH_DEF),200);

    if (body_len > body_recv_len) {//recieved body length shorter than it claimed
        ESP_EARLY_LOGI("FG","Len claimed > recv len, quitting cmdcom.");
        return false;
    }

    // 5. 校验和验证
    unsigned int calc_sum = GetSum((uchar*)&uart_rx_buf[6], body_len + 1); 
    unsigned int recv_sum = (uart_rx_buf[8 + body_len - 1] << 8) | uart_rx_buf[8 + body_len];

    ESP_EARLY_LOGI("FG", "Respond sum: calc 0x%04x =? recv 0x%04x", calc_sum, recv_sum);
    if (calc_sum != recv_sum)
    {
        ESP_EARLY_LOGI("FG", "Respond sum error");
        return FALSE;
    }
    
    ESP_EARLY_LOGI("FG","Respond succ");
    return TRUE;                                               // 通讯成功
}
//=============================================================//




//=============================================================//
//                                                             //
// 功  能  : CloseFG()关闭指纹端口                             //
// 输  入  : 无                                                //
// 输  出  : 无                                                //
// 备  注  : 指纹模块断电，关闭串口                            //
//                                                             //
//=============================================================//
void CloseFG(void)
{
    ESP_EARLY_LOGI("FG","Shut down modual");
    gpio_set_level(GPIO_FGREAD_POWER, 0); //cut main power, stay eco mode
}
//=============================================================//




//=============================================================//
//                                                             //
// 功  能  : OpenFG()打开指纹端口                              //
// 输  入  : 无                                                //
// 输  出  : TRUE 打开成功                                     //
//           FALSE 打开失败                                    //
// 备  注  : 初始化串口，打开指纹模块电源                      //
//           等待接收模块上电初始化成功标志0x55                //
//                   //
//           需预先 FG_Init()                                //
//                                                             //
//=============================================================//
uchar OpenFG(void)
{
    uart_flush_input(UART_PORT_1);//clear up uart1
    gpio_set_level(GPIO_FGREAD_POWER, 1);//enable high power supply channel
    ESP_EARLY_LOGI("FG","Booting FG Reader=================================");


    //wait 200ms, or get respond
    TickType_t start_tick = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(1000)) 
    {
        //try fetch auto respond, if get 0x55, deviced booted, no need to wait
        uart_read_bytes(UART_PORT_1, (uchar *)uart_rx_buf, 1, pdMS_TO_TICKS(50));
        if (uart_rx_buf[0] == 0x55) 
            break;
    }
    ESP_EARLY_LOGI("FG","powered up finger print reader A");
    CmdComm(AUTH_DEF,sizeof(AUTH_DEF),200);
    CmdComm(LED_ON,sizeof(LED_ON),200);

    return TRUE;                                          // 打开指纹端口失败
}
//=============================================================//




//=============================================================//
//                                                             //
// 功  能  : AddFG()注册指纹                                   //
// 输  入  : id    指定注册指纹存储序号                        //
// 输  出  : TRUE  注册成功                                    //
//           FALSE 失败                                        //
// 备  注  : id有效取值[0,FG_MAX-1]                            //
//                                                             //
//=============================================================//
uchar AddFG(uint id)
{
    if (id >= FG_MAX) return FALSE;                            // 参数输入错误

    uart_tx_buf[0] = 0x01;                                             // 包标志，1命令包
    uart_tx_buf[1] = 0x00;                                             // 包长度，高8位
    uart_tx_buf[2] = 0x08;                                             // 包长度，低8位
    uart_tx_buf[3] = 0x31;                                       // 指令码
    uart_tx_buf[4] = (uchar)id>>8;//FG ID hi byte
    uart_tx_buf[5] = (uchar)id&0x00ff;//FG ID lo byte
    uart_tx_buf[6] = 0x08;//8x touches
    uart_tx_buf[7] = 0x00;
    uart_tx_buf[8] = 0b00011001;//bit5 no disable(yes) lift fg for next reg, yes disable(no) same finger no multi reg, yes overwrite id, no disable(yes) return intel during process, no preprocess, yes led always on bit 0                                              // 重复登记标志，1允许,0禁止
    uint calc_sum = GetSum((uchar*)uart_tx_buf, 9);                    // 校验和
    uart_tx_buf[9] = (uchar)(calc_sum >> 8);   // 高 8 位
    uart_tx_buf[10] = (uchar)(calc_sum & 0xFF); // 低 8 位

    uart_flush_input(UART_PORT_1);//clean up UART1
    uart_write_bytes(UART_PORT_1, (const uchar*)BAGHEAD, sizeof(BAGHEAD));
    uart_write_bytes(UART_PORT_1, (const uchar*)uart_tx_buf, 11);

    const TickType_t xTimeoutTicks = pdMS_TO_TICKS(10000); //10s内完成
    TickType_t xStartTime = xTaskGetTickCount();
    
    ESP_EARLY_LOGI("FG","ADDFG request sent");
    while((xTaskGetTickCount() - xStartTime) < xTimeoutTicks)
    {   
        if(CmdComm(EMPTY,0,1000))
        {
            ESP_EARLY_LOGI("FG","Get respond:0x%x,0x%x,0x%x",uart_rx_buf[9],uart_rx_buf[10],uart_rx_buf[11]);
            if(uart_rx_buf[10]==0x00)
                printf("System validating ");
            else if(uart_rx_buf[10]==0x01)
                printf("Getting image ");
            else if(uart_rx_buf[10]==0x02)
                printf("Generating FG charactor ");
            else if(uart_rx_buf[10]==0x03)
                printf("Please lift up you finger beacause ");
            
            if(uart_rx_buf[11]<0xf0 && uart_rx_buf[11]>0x00)
                printf("in [%d/8], ", uart_rx_buf[11]);
            else if(uart_rx_buf[10]==0x04 && uart_rx_buf[11]==0xf0)
                printf("Merging as pattern, ");
            else if(uart_rx_buf[10]==0x05 && uart_rx_buf[11]==0xf1)
                printf("Reg validating, ");
            else if(uart_rx_buf[10]==0x06 && uart_rx_buf[11]==0xf2)
                printf("Saving FG Pattern,");
            else if(uart_rx_buf[10]==0x00)
            {
                printf(" and launching.\n");
                continue;
            }
            
            if(uart_rx_buf[9]==0x00)
            {
                printf("success.\n");
                if(uart_rx_buf[10]==0x06 && uart_rx_buf[11]==0xf2)
                    return TRUE;
            }
            else
            {
                if(uart_rx_buf[9]==0x1f)
                    printf("fail as storage full.\n");
                else if(uart_rx_buf[9]==0x26)
                    printf("fail as timeout.\n");
                else if(uart_rx_buf[9]==0x27)
                    printf("fail as this fingerprint exist in system\n");
                else
                    printf("fail.\n");
                return FALSE;
            }  
        }
    }

    return FALSE;                                              // 注册失败
}
//=============================================================//

//read finger from sensor, gen pattern and save to buffer
uchar ReadFG()
{   //capture fingerprint image
    if(!CmdComm(READ_SENSOR,sizeof(READ_SENSOR),500))
        return FALSE;
    else if(uart_rx_buf[9]==02)
    {
        printf("Read fail. Where is ur finger?\n");
        return FALSE;
    }
    else if(uart_rx_buf[9]!=00)
    {
        printf("Read fail.\n");
        return FALSE;
    }

    //Gen pattern from fg image
    if(!CmdComm(GEN_PATTERN,sizeof(GEN_PATTERN),500))
        return FALSE;
    if(uart_rx_buf[9]==00)
        return TRUE;
    else if(uart_rx_buf[9]==06)
        printf("Gen Ptrn fail, too mess.\n");
    else if(uart_rx_buf[9]==06)
        printf("Gen Ptrn fail, too common.\n");
    return FALSE;
}

//=============================================================//
//                                                             //
// 功  能  : SearchFG()搜索指纹                                //
// 输  入  : 无                                                //
// 输  出  : TRUE  搜索成功                                    //
//           FALSE 失败                                        //
// 备  注  : 无                                                //
//                                                             //
//=============================================================//
uchar SearchFG(void)
{
    uchar state;
    ESP_EARLY_LOGI("FG","Search try");
    
    uart_flush_input(UART_PORT_1);//clean up UART1
    if(!ReadFG())//load fg to buffer for searching
        return FALSE;

    state=CmdComm(SEARCH_PATTERN,sizeof(SEARCH_PATTERN),500);
    if(state==TRUE)
    {
        if (uart_rx_buf[9] == 0x00)                                // 搜索成功
        {
            searched_fg_id= (uart_rx_buf[10] << 8) | uart_rx_buf[11];//id that fingerprint reg
            int searched_fg_id_score=(uart_rx_buf[12] << 8) | uart_rx_buf[13];
            if(searched_fg_id_score>50)
            {
                printf("Reconized FG, id:%d, score:%d\n",searched_fg_id,searched_fg_id_score);
                return TRUE;
            }
            else
                printf("Not confident FG, id:%d, score:%d\n",searched_fg_id,searched_fg_id_score);
        }                 
        else if (uart_rx_buf[9] == 0x09)
            printf("FG not reg.\n");
        else
            printf("Search failed.\n");
        return FALSE;
    };
    return FALSE;
}

unsigned short fg_search_fetch_id(void)
{
    return searched_fg_id;
}

//=============================================================//



//=============================================================//
//                                                             //
// 功  能  : ClrFG()清空全部指纹                               //
// 输  入  : 无                                                //
// 输  出  : TRUE  清空成功                                    //
//           FALSE 失败                                        //
// 备  注  : 无                                                //
//                                                             //
//=============================================================//
uchar ClrFG(void)
{
    if(CmdComm(CLEAR_ALL_FG,sizeof(CLEAR_ALL_FG),500))                                   // 发送清空命令
    {   
        if(uart_rx_buf[9]==0x00)
        {
            printf("Succ clear all fg.\n");
            return TRUE;
        }
    }
    return FALSE;
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
            g_fg_status=g_fg_status_2B;//lock in status
            ESP_EARLY_LOGI("FG","Finger pressed on fg reader, will do:%d",g_fg_status);
            OpenFG();
            if(g_fg_status==FG_BORED || g_fg_status==FG_PEND||g_fg_status==FG_PEND_N_SIGNIN)
            {
                status=SearchFG();
                if(g_fg_status==FG_PEND_N_SIGNIN  && status==true)
                {
                    ESP_EARLY_LOGI("FG", "Pass, unlocking door.");
                    //unlock door
                    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                    //send a ticket through signal
                    xSemaphoreGiveFromISR(g_dr_unlock_sem, &xHigherPriorityTaskWoken);//piority check
                    if (xHigherPriorityTaskWoken) {//if tsk_doorLock is highest piority then run now
                        portYIELD_FROM_ISR();
                    }
                }
            }
            if(g_fg_status==FG_REG)//registrate a finger print
            {
                AddFG(1);//id 1
            }
            if(g_fg_status==FG_DEL)
            {
                ClrFG();
            }
            //if(g_fg_status==FG_CUR_DEL)
            //{
            //    SearchFG();//detect what fg id is
            //    DelFG(fg_search_fetch_id(),1);//del that
            //}
            CloseFG();
        }
        if(xSemaphoreTake(g_fg_pressed_sem, portMAX_DELAY) == pdTRUE) {};//spend 1 ticket
    }
}

