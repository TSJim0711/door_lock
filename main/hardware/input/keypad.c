#include <uni_input.h>
#include "app.h"

static const char keypad_map[4][3]={    {KEYPAD_1, KEYPAD_2, KEYPAD_3},
                                        {KEYPAD_4, KEYPAD_5, KEYPAD_6},
                                        {KEYPAD_7, KEYPAD_8, KEYPAD_9},
                                        {KEYPAD_STAR, KEYPAD_0, KEYPAD_DASH}};

static SemaphoreHandle_t s_keypad_in_sem;
void keypad_input_handler(void *pvParameters)
{
    uint8_t pressing_row;
    while(1)
    {
        if(xSemaphoreTake(s_keypad_in_sem, portMAX_DELAY) == pdTRUE)  //spend a ticket then go (is binary)
        {   
            for(short col=0;col<3;col++)
            {
                gpio_set_level(col_gpio[0], (col==0?1:0));//control output
                gpio_set_level(col_gpio[1], (col==1?1:0));
                gpio_set_level(col_gpio[2], (col==2?1:0));
                vTaskDelay(pdMS_TO_TICKS(5));//wait for above GPIO settled
                for(short row=0;row<4;row++)
                    if(gpio_get_level(row_gpio[row]))//read input with 1 output
                    {
                        ESP_LOGI("KP","Pressed:%c",keypad_map[row][col]);
                        event_publish(HW_INPT,SERV_UI,&(inpt_event_t){keypad_map[row][col]},sizeof(inpt_event_t));
                        pressing_row=row;
                    }
            }
            gpio_set_level(col_gpio[0], 1);
            gpio_set_level(col_gpio[1], 1);
            gpio_set_level(col_gpio[2], 1);
            while (gpio_get_level(row_gpio[pressing_row])) {
                vTaskDelay(pdMS_TO_TICKS(20)); //wait untill release, 抬起时有震荡
            }
        }
    }
}

void keypad_input_trigger(void* arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    //send a ticket through signal
    xSemaphoreGiveFromISR(s_keypad_in_sem, &xHigherPriorityTaskWoken);//priority check
    if (xHigherPriorityTaskWoken) {//if fg_service is highest priority then run now
        portYIELD_FROM_ISR();
    }
}

void keypad_init(void)
{
    //4 row as input
    gpio_config_t io_keypad_row_conf = {
        .pin_bit_mask = (1ULL << row_gpio[0])|(1ULL << row_gpio[1])|(1ULL << row_gpio[2])|(1ULL << row_gpio[3]),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE  // 上沿（按下）触发中断
    };
    gpio_config(&io_keypad_row_conf);
    gpio_install_isr_service(0);//install interrupt service
    gpio_isr_handler_add(row_gpio[0], keypad_input_trigger, NULL);
    gpio_isr_handler_add(row_gpio[1], keypad_input_trigger, NULL);
    gpio_isr_handler_add(row_gpio[2], keypad_input_trigger, NULL);
    gpio_isr_handler_add(row_gpio[3], keypad_input_trigger, NULL);

    //3 col as power
    gpio_config_t io_keypad_col_conf = {
        .pin_bit_mask = (1ULL << col_gpio[0]|(1ULL << col_gpio[1])|(1ULL << col_gpio[2])),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_keypad_col_conf);
    gpio_set_level(col_gpio[0], 1);//power up every col
    gpio_set_level(col_gpio[1], 1);
    gpio_set_level(col_gpio[2], 1);

    s_keypad_in_sem = xSemaphoreCreateBinary();
    xTaskCreate(keypad_input_handler, "tsk_keypad_service", 4096, NULL, 10, NULL);
}