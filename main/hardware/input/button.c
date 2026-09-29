#include "esp_timer.h"
#include "uni_input.h"
#include "app.h"

static long long int last_press_time=0;
static uint8_t gpio_status_instant;
static uint16_t last_btn_press=0;
static void IRAM_ATTR btn_gpio_isr_handler(void* arg) //switch fingprint reader to do
{
    gpio_status_instant=gpio_get_level((uint32_t)arg);
    //ESP_EARLY_LOGI("btn", "Button press:%u,value:%d; when:%lld, gap:%lld",(uint32_t)arg,gpio_status_instant,esp_timer_get_time(),esp_timer_get_time()-last_press_time);
    if(esp_timer_get_time()-last_press_time>100000)//ignore >100ms gap, release btn also refresh last_press_time
    {
        if(last_btn_press==(uint32_t)arg)//is not releasing btn? gpio_get_level() can't trust
        {
            last_btn_press=0;//btn release, not doing anything and return
            last_press_time=esp_timer_get_time();
            return;
        }
        else
            last_btn_press=(uint32_t)arg;//btn press down,record it

        if((uint32_t)arg==GPIO_BTN_UP && !gpio_status_instant)
            event_publish(HW_INPT,SERV_UI,&(inpt_event_t){INPT_BTN_UP},sizeof(inpt_event_t));
        else if((uint32_t)arg==GPIO_BTN_DOWN && !gpio_status_instant)
            event_publish(HW_INPT,SERV_UI,&(inpt_event_t){INPT_BTN_DOWN},sizeof(inpt_event_t));
        else if((uint32_t)arg==GPIO_BTN_RETURN && !gpio_status_instant)
            event_publish(HW_INPT,SERV_UI,&(inpt_event_t){INPT_BTN_RETURN},sizeof(inpt_event_t));
        else if((uint32_t)arg==GPIO_BTN_ENTER && !gpio_status_instant)
            event_publish(HW_INPT,SERV_UI,&(inpt_event_t){INPT_BTN_ENTER},sizeof(inpt_event_t));
    }
    last_press_time=esp_timer_get_time();
}

void btn_init(void)
{
    //config btn
    gpio_config_t io_btn_conf = {
        .pin_bit_mask = (1ULL << GPIO_BTN_UP),//trigger by press down and release
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE  // 下降沿（按下）触发中断
    };
    gpio_config(&io_btn_conf);
    gpio_install_isr_service(0);//button inturrupt
    gpio_isr_handler_add(GPIO_BTN_UP, btn_gpio_isr_handler, (void*)GPIO_BTN_UP);

    io_btn_conf.pin_bit_mask=(1ULL << GPIO_BTN_DOWN);
    gpio_config(&io_btn_conf);
    gpio_isr_handler_add(GPIO_BTN_DOWN, btn_gpio_isr_handler, (void*)GPIO_BTN_DOWN);

    io_btn_conf.pin_bit_mask=(1ULL << GPIO_BTN_RETURN);
    gpio_config(&io_btn_conf);
    gpio_isr_handler_add(GPIO_BTN_RETURN, btn_gpio_isr_handler, (void*)GPIO_BTN_RETURN);
    
    io_btn_conf.pin_bit_mask=(1ULL << GPIO_BTN_ENTER);
    gpio_config(&io_btn_conf);
    gpio_isr_handler_add(GPIO_BTN_ENTER, btn_gpio_isr_handler, (void*)GPIO_BTN_ENTER);

}