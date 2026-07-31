#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/portmacro.h"
#include "driver/gpio.h"

#include "oled.h"

#define GPIO_BTN    GPIO_NUM_4
#define GPIO_LED_RED   GPIO_NUM_5
#define GPIO_LED_GREEN GPIO_NUM_6

//btn pressed signal
static SemaphoreHandle_t s_btn_pressed_sem = NULL;

//btn send ticket service
static void IRAM_ATTR btn_gpio_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    //send a ticket through signal
    xSemaphoreGiveFromISR(s_btn_pressed_sem, &xHigherPriorityTaskWoken);//piority check
    if (xHigherPriorityTaskWoken) {//if tsk_doorLock is highest piority then run now
        portYIELD_FROM_ISR();
    }
}

//
void door_lock_task(void *pvParameters) {
    int relock_countdown = 0;
    
    //defualt: red led on, green off
    gpio_set_level(GPIO_LED_GREEN, 0);
    gpio_set_level(GPIO_LED_RED, 1);

    while (1) {
        //if in countdown then wait 50ms, else wait till die
        TickType_t xTicksToWait = (relock_countdown > 0) ? pdMS_TO_TICKS(50) : portMAX_DELAY;

        //wait or if has ticket, then go on
        if (xSemaphoreTake(s_btn_pressed_sem, xTicksToWait) == pdTRUE) {
            //if have ticket(just pressed btn) reset timmer, green led on, red led off
            printf("Unlocked.\n");
            gpio_set_level(GPIO_LED_GREEN, 1);
            gpio_set_level(GPIO_LED_RED, 0);
            relock_countdown = 200; // 200 * 50ms = 10s countdown
        }

        //countdown
        relock_countdown--;
        if(relock_countdown%20==0)//20*50ms =1s, print countdown per sec
        printf("%ds left...\n", relock_countdown/20);

        //time running, green led flash
        if (relock_countdown <= 80 && relock_countdown > 0) {
            //flash faster when  less time left
            int blink_freq = ((relock_countdown+1)/ 10) + 1;
            if (relock_countdown % blink_freq == 0) {
                gpio_set_level(GPIO_LED_GREEN, !gpio_get_level(GPIO_LED_GREEN));
            }
        }

        //countdown end, red led on, green off
        if (relock_countdown == 0) {
            printf("Locked!\n");
            gpio_set_level(GPIO_LED_GREEN, 0);
            gpio_set_level(GPIO_LED_RED, 1);
        }
    }
}

void app_main(void) {
    //create signal tunnel
    s_btn_pressed_sem = xSemaphoreCreateBinary();

    //config gpiofor led
    gpio_reset_pin(GPIO_LED_RED);
    gpio_set_direction(GPIO_LED_RED, GPIO_MODE_OUTPUT);
    gpio_reset_pin(GPIO_LED_GREEN);
    gpio_set_direction(GPIO_LED_GREEN, GPIO_MODE_INPUT_OUTPUT);

    //config btn
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_BTN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE // 下降沿（按下接地）触发中断
    };
    gpio_config(&io_conf);

    //reg btn interrupt
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_BTN, btn_gpio_isr_handler, NULL);

    //init i2c & oled
    i2c_master_init();
    OLED_Init();
    OLED_DrawTetragon(10,10,60,20,1,0);

    //create doorlock service
    xTaskCreate(door_lock_task, "tsk_doorLock", 4096, NULL, 10, NULL);
    vTaskDelete(NULL);
}