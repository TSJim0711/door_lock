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
#include "activity.h"
#include "app.h"

#define GPIO_LED_RED   GPIO_NUM_21
#define GPIO_LED_GREEN GPIO_NUM_20

static void IRAM_ATTR fg_on_touch_handler(void* arg) //someone puts there finger on sensor
{
    //ESP_EARLY_LOGI("FG", "SX TRIGGERED========================");
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

    while (1) {
        //if in countdown then wait 50ms, else wait till die
        TickType_t xTicksToWait = (countdown_cnt > 0) ? pdMS_TO_TICKS(50) : portMAX_DELAY;

        //wait or if has ticket, then go on
        if (xSemaphoreTake(g_dr_unlock_sem, xTicksToWait) == pdTRUE) 
        {
            //if have ticket(just pressed btn) reset timmer, green led on, red led off
            printf("Unlocked.\n");
            gpio_set_level(GPIO_LED_GREEN, 1);
            gpio_set_level(GPIO_LED_RED, 0);
            //load activity
            event_send_to(DOOR_LOCK,(&(doorlock_event_t){DOOR_UNLOCK,0}),sizeof(doorlock_event_t));
            countdown_cnt = 200; // 200 * 50ms = 10s countdown
        }

        //countdown
        countdown_cnt--;
        if(countdown_cnt%20==0)//20*50ms =1s, print countdown per sec
        {
            printf("%ds left...\n", countdown_cnt/20);
            event_send_to(DOOR_LOCK,(&(doorlock_event_t){DOOR_UNLOCKED,countdown_cnt/20}),sizeof(doorlock_event_t));
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
            activity_back();
        }
    }
}

void app_main(void) 
{
    g_dr_unlock_sem = xSemaphoreCreateBinary();
    g_fg_pressed_sem = xSemaphoreCreateBinary();

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

    app_init();

    //run hardware func
    xTaskCreate(door_lock_task, "tsk_doorLock", 4096, NULL, 10, NULL);
    xTaskCreate(fg_service, "tsk_fg_reader_service", 4096, NULL, 10, NULL);
    xTaskCreate(ui_event_handler, "tsk_ui", 4096, NULL, 10, NULL);
}