#ifndef UNI_INPUT_H
#define UNI_INPUT_H
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define GPIO_BTN_ENTER GPIO_NUM_42
static const gpio_num_t row_gpio[4]={GPIO_NUM_10,GPIO_NUM_9,GPIO_NUM_46,GPIO_NUM_3};
static const gpio_num_t col_gpio[3]={GPIO_NUM_8,GPIO_NUM_18,GPIO_NUM_17};

#define NONE        0x00
#define KEYPAD_1    '1'
#define KEYPAD_2    '2'
#define KEYPAD_3    '3'
#define KEYPAD_4    '4'
#define KEYPAD_5    '5'
#define KEYPAD_6    '6'
#define KEYPAD_7    '7'
#define KEYPAD_8    '8'
#define KEYPAD_9    '9'
#define KEYPAD_0    '0'
#define KEYPAD_STAR '*'
#define KEYPAD_DASH '#'

void keypad_init(void);
void btn_init(void);
extern void keypad_input_handler(void *pvParameters);

#endif