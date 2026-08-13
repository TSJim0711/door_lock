#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/portmacro.h"
#include "driver/gpio.h"
#include "esp_random.h"
#include "esp_log.h"

#include "oled.h"
#include "fg_reader.h"
#include "uni_input.h"

#define GPIO_LED_RED   GPIO_NUM_21
#define GPIO_LED_GREEN GPIO_NUM_20

static void IRAM_ATTR fg_on_touch_handler(void* arg) //someone puts there finger on sensor
{
    ESP_EARLY_LOGI("FG","Finger detected");
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    //send a ticket through signal
    xSemaphoreGiveFromISR(g_fg_pressed_sem, &xHigherPriorityTaskWoken);//priority check
    if (xHigherPriorityTaskWoken) {//if fg_service is highest priority then run now
        portYIELD_FROM_ISR();
    }
}

SemaphoreHandle_t g_dr_unlock_sem = NULL;
void door_lock_task(void *pvParameters) {
    int countdown_cnt = 0;
    
    //default: red led on, green off
    gpio_set_level(GPIO_LED_GREEN, 0);
    gpio_set_level(GPIO_LED_RED, 1);

    char print_buff[64];
    oled_print_str(10,5,"门已上锁!",24,1,0);
    oled_print_str(10,30,"Door Locked",24,1,0);

    while (1) {
        //if in countdown then wait 50ms, else wait till die
        TickType_t xTicksToWait = (countdown_cnt > 0) ? pdMS_TO_TICKS(50) : portMAX_DELAY;

        //wait or if has ticket, then go on
        if (xSemaphoreTake(g_dr_unlock_sem, xTicksToWait) == pdTRUE) {
            //if have ticket(just pressed btn) reset timmer, green led on, red led off
            printf("Unlocked.\n");
            gpio_set_level(GPIO_LED_GREEN, 1);
            gpio_set_level(GPIO_LED_RED, 0);
            oled_screen_clear();
            sprintf(print_buff,"你好id:%d住户,\n",fg_identified_fetch_id());
            oled_print_str(10,5,print_buff,24,1,0);
            oled_print_str(10,30,"门已开锁!",24,1,0);
            countdown_cnt = 200; // 200 * 50ms = 10s countdown
        }

        //countdown
        countdown_cnt--;
        if(countdown_cnt%20==0)//20*50ms =1s, print countdown per sec
        {
            printf("%ds left...\n", countdown_cnt/20);

            oled_draw_rect(10,5,106,29,0,0);
            sprintf(print_buff,"还剩%d时",countdown_cnt/20);
            oled_print_str(10,5,print_buff,24,1,0);
            oled_print_str(10,30,"Door unlocked,",16,1,0);
            oled_draw_rect(10,46,22,29,0,0);
            sprintf(print_buff,"%d sec left.",countdown_cnt/20);
            oled_print_str(10,46,print_buff,16,1,0);
        }

        //time running, green led flash
        if (countdown_cnt <= 80 && countdown_cnt > 0) {
            //flash faster when  less time left
            int blink_freq = ((countdown_cnt+1)/ 10) + 1;
            if (countdown_cnt % blink_freq == 0) {
                gpio_set_level(GPIO_LED_GREEN, !gpio_get_level(GPIO_LED_GREEN));
            }
        }

        //countdown end, red led on, green off
        if (countdown_cnt == 0) {
            printf("Locked!\n");
            gpio_set_level(GPIO_LED_GREEN, 0);
            gpio_set_level(GPIO_LED_RED, 1);
            oled_screen_clear();
            oled_print_str(10,5,"门已上锁",24,1,0);
            oled_print_str(10,30,"Door Locked",16,1,0);
        }
    }
}

SemaphoreHandle_t g_input_psw_sem =NULL;
void psw_interface(void *pvParameters)
{
    char psw_buff[7],disp_buff[32];
    char* psw_buff_ptr=&psw_buff[0];
    for(int short i=0; i<6;i++)
        psw_buff[i]='x';
    short countdown_cnt=0; 
    while(1)
    {
        TickType_t heartbeat_tick = (countdown_cnt!=0) ? pdMS_TO_TICKS(1000) : portMAX_DELAY;//1sec per update
        if(xSemaphoreTake(g_input_psw_sem, heartbeat_tick) == pdTRUE)
        {
            countdown_cnt=11;//10 sec timeout, any input will refresh
            if(psw_buff_ptr==&psw_buff[0])
            {
                //switch to psw input interface
                oled_screen_clear();
                oled_print_str(10,2,"Key in",24,1,0);
            }
            *psw_buff_ptr=input_buff_pop();//pop an input from buff
            psw_buff_ptr++;
            sprintf(disp_buff,"[%-3.3s:%-3.3s]",psw_buff,psw_buff+3);//disp cur psw buff
            oled_print_str(10,28,disp_buff,16,0,0);
            if(psw_buff_ptr==&psw_buff[6])//if get 6 char, determine if valid password
            {
                if(strcmp(psw_buff,"123456")==0)
                {//correct pasword, unlock door
                    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                    //send a ticket through signal
                    xSemaphoreGiveFromISR(g_dr_unlock_sem, &xHigherPriorityTaskWoken);//priority check
                    if (xHigherPriorityTaskWoken) {//if tsk_doorLock is highest priority then run now
                        portYIELD_FROM_ISR();
                    }
                    countdown_cnt=0;
                }
                else
                {
                    oled_screen_clear();
                    oled_print_str(10,2,"Psw Err",24,1,0);
                    oled_print_str(10,28,"U've entered.",16,1,0);
                    oled_print_str(10,46,"wrong password.",16,1,0);
                    countdown_cnt=1;//reset screen after 1sec
                }
                psw_buff_ptr=&psw_buff[0];//clear psw buff
                for(int short i=0; i<6;i++)
                    psw_buff[i]='x';
                continue;
            }
        }
        countdown_cnt--;
        if(countdown_cnt==0)//timeout, reset screen
        {
            psw_buff_ptr=&psw_buff[0];//clear psw buff
            for(int short i=0; i<6;i++)
                psw_buff[i]='x';
            oled_screen_clear();
            oled_print_str(10,5,"门已上锁",24,1,0);
            oled_print_str(10,30,"Door Locked",16,1,0);
        }
        else
        {
            sprintf(disp_buff,"%.2d sec left.",countdown_cnt);
            oled_print_str(10,46,disp_buff,16,1,0);
        }

    }    
}

void app_main(void) {
    //create signal tunnel
    g_dr_unlock_sem = xSemaphoreCreateBinary();
    g_fg_pressed_sem = xSemaphoreCreateBinary();
    g_input_psw_sem = xSemaphoreCreateCounting(INPUT_BUFF_SIZE,0);

    //config gpio for led
    gpio_reset_pin(GPIO_LED_RED);
    gpio_set_direction(GPIO_LED_RED, GPIO_MODE_OUTPUT);
    gpio_reset_pin(GPIO_LED_GREEN);
    gpio_set_direction(GPIO_LED_GREEN, GPIO_MODE_INPUT_OUTPUT);

    //init i2c & oled
    i2c_master_init();
    oled_init();
    
    //init fingerprint reader
    fg_init();
    gpio_config_t io_fg_resp_conf = {
        .pin_bit_mask = (1ULL << GPIO_FGREAD_SX),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE  // 上升沿（按后释放）触发中断
    };
    gpio_config(&io_fg_resp_conf);
    gpio_install_isr_service(0);//button inturrupt
    gpio_isr_handler_add(GPIO_FGREAD_SX, fg_on_touch_handler, NULL);

    //input
    keypad_init();
    btn_init();

    //create doorlock service
    xTaskCreate(door_lock_task, "tsk_doorLock", 4096, NULL, 10, NULL);
    xTaskCreate(fg_service, "tsk_fg_reader_service", 4096, NULL, 10, NULL);
    xTaskCreate(psw_interface, "tsk_fg_reader_service", 4096, NULL, 10, NULL);
    vTaskDelete(NULL);
}