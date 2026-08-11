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

#define GPIO_BTN    GPIO_NUM_4
#define GPIO_LED_RED   GPIO_NUM_5
#define GPIO_LED_GREEN GPIO_NUM_6

static void IRAM_ATTR btn_gpio_isr_handler(void* arg) {//switch fingprint reader to do
    if(g_fg_next_state==FG_STATE_IDLE||g_fg_next_state==FG_SEARCH_N_SIGNIN)
    {
        g_fg_next_state=FG_STATE_ENROLL;
        ESP_EARLY_LOGI("FG","FG Reg");
    }
    else if (g_fg_next_state==FG_STATE_ENROLL)
    {
        g_fg_next_state=FG_DEL_ALL;
        ESP_EARLY_LOGI("FG","FG Del");
    }
    else if (g_fg_next_state==FG_DEL_ALL)
    {
        g_fg_next_state=FG_SEARCH_N_SIGNIN;
        ESP_EARLY_LOGI("FG","FG Bored");
    }
}

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

void app_main(void) {
    //create signal tunnel
    g_dr_unlock_sem = xSemaphoreCreateBinary();
    g_fg_pressed_sem = xSemaphoreCreateBinary();

    //config gpio for led
    gpio_reset_pin(GPIO_LED_RED);
    gpio_set_direction(GPIO_LED_RED, GPIO_MODE_OUTPUT);
    gpio_reset_pin(GPIO_LED_GREEN);
    gpio_set_direction(GPIO_LED_GREEN, GPIO_MODE_INPUT_OUTPUT);

    //config btn
    gpio_config_t io_btn_conf = {
        .pin_bit_mask = (1ULL << GPIO_BTN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE  // 下降沿（按下）触发中断
    };
    gpio_config(&io_btn_conf);
    gpio_install_isr_service(0);//button inturrupt
    gpio_isr_handler_add(GPIO_BTN, btn_gpio_isr_handler, NULL);

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

    //create doorlock service
    xTaskCreate(door_lock_task, "tsk_doorLock", 4096, NULL, 10, NULL);
    xTaskCreate(fg_service, "tsk_fg_reader_service", 4096, NULL, 10, NULL);
    vTaskDelete(NULL);
}