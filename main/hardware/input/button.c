#include <uni_input.h>
#include "app.h"

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

void btn_init(void)
{
    //config btn
    gpio_config_t io_btn_conf = {
        .pin_bit_mask = (1ULL << GPIO_BTN_ENTER),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE  // 下降沿（按下）触发中断
    };
    gpio_config(&io_btn_conf);
    gpio_install_isr_service(0);//button inturrupt
    gpio_isr_handler_add(GPIO_BTN_ENTER, btn_gpio_isr_handler, NULL);
}