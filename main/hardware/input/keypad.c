#include <uni_input.h>

static const char keypad_map[4][3]={    {KEYPAD_1, KEYPAD_2, KEYPAD_3},
                                        {KEYPAD_4, KEYPAD_5, KEYPAD_6},
                                        {KEYPAD_7, KEYPAD_8, KEYPAD_9},
                                        {KEYPAD_STAR, KEYPAD_0, KEYPAD_DASH}};

static SemaphoreHandle_t s_fg_key_in_sem;
extern SemaphoreHandle_t g_input_psw_sem;
void keypad_input_service(void *pvParameters)
{
    while(1)
    {
        if(xSemaphoreTake(s_fg_key_in_sem, portMAX_DELAY) == pdTRUE)  //spend a ticket then go (is binary)
        {   
            for(short col=0;col<3;col++)
            {
                gpio_set_level(col_gpio[0], (col==0?1:0));//control output
                gpio_set_level(col_gpio[1], (col==1?1:0));
                gpio_set_level(col_gpio[2], (col==2?1:0));
                vTaskDelay(5);//wait for above done
                for(short row=0;row<4;row++)
                    if(gpio_get_level(row_gpio[row]))//read input with 1 output
                        if(input_buff_push(keypad_map[row][col]))
                        {//if puah success, then inform psw input interface
                            ESP_EARLY_LOGI("INPT","User pressed btn id: %c",keypad_map[row][col]);
                            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                            //send a ticket through signal
                            xSemaphoreGiveFromISR(g_input_psw_sem, &xHigherPriorityTaskWoken);
                            if (xHigherPriorityTaskWoken)
                                portYIELD_FROM_ISR();
                        }
            }
            gpio_set_level(col_gpio[0], 1);
            gpio_set_level(col_gpio[1], 1);
            gpio_set_level(col_gpio[2], 1);
        }
        vTaskDelay(10);//消抖
    }
}

void keypad_input_handler(void* arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    //send a ticket through signal
    xSemaphoreGiveFromISR(s_fg_key_in_sem, &xHigherPriorityTaskWoken);//priority check
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
    gpio_isr_handler_add(row_gpio[0], keypad_input_handler, NULL);
    gpio_isr_handler_add(row_gpio[1], keypad_input_handler, NULL);
    gpio_isr_handler_add(row_gpio[2], keypad_input_handler, NULL);
    gpio_isr_handler_add(row_gpio[3], keypad_input_handler, NULL);

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

    s_fg_key_in_sem = xSemaphoreCreateBinary();
    xTaskCreate(keypad_input_service, "tsk_keypad_service", 4096, NULL, 10, NULL);
}

//uni_input
char input_buff[INPUT_BUFF_SIZE]={NONE};
char* input_buff_ptr_head=&input_buff[0];
char* input_buff_ptr_tail=&input_buff[0];
bool input_buff_push(char key_pressed)
{
    if(input_buff_ptr_head+1!=input_buff_ptr_tail || (input_buff_ptr_head==&input_buff[INPUT_BUFF_SIZE-1]&&input_buff_ptr_tail==&input_buff[0]))//check if buff not full
    {
        //move header to next pos
        input_buff_ptr_head++;//push head forward
        if(input_buff_ptr_head==&input_buff[INPUT_BUFF_SIZE])//if overflow, reset to [0]
            input_buff_ptr_head=&input_buff[0];
        *input_buff_ptr_head=key_pressed;//push key_pressed
        return true;
    }
    return false;//push failed
}

char input_buff_pop(void)
{
    if(input_buff_ptr_tail!=input_buff_ptr_head)//if not empty
    {
        input_buff_ptr_tail++;//push tail forward
        if(input_buff_ptr_tail==&input_buff[INPUT_BUFF_SIZE])//if overflow, reset to [0]
            input_buff_ptr_tail=&input_buff[0];
        return *input_buff_ptr_tail;
    }
    else
        return NONE;
}

void input_buff_clear(void)
{
    input_buff_ptr_head=&input_buff[0];
    input_buff_ptr_tail=&input_buff[0];
}