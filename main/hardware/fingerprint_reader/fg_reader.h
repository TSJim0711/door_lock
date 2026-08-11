#ifndef FG_READER_H
#define FG_READER_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"

//pin def
#define GPIO_FGREAD_TXD GPIO_NUM_11
#define GPIO_FGREAD_RXD GPIO_NUM_12
#define GPIO_FGREAD_POWER GPIO_NUM_13
#define GPIO_FGREAD_SX GPIO_NUM_14

//def uart
#define UART_PORT_1 UART_NUM_1
#define UART1_BAUD     57600
#define UART1_BUFF_SIZE      256
#define UART1_MAX_RW_LEN      72

#define   LACK_ARG              0xff
#define   OVER_TIME_S		0xfe
#define   OVER_TIME_R		0xfd// 时间常数定义
#define	  T_300MS		30
#define	  T_500MS		50
#define   T_1S			100
#define   T_5S			500// 指纹模块相关常数定义
//FG_STATUS指纹工作状态
#define   CMD_STOR		0x06		// 存储
#define   CMD_DEL		0x0c		// 删除
#define   CMD_ALOGIN		0x54		// 自动注册
#define   FG_MAX		29		// 使用指纹模板最大数
#define   FG_GETIMG_CNT   	2 		// 自动注册待指次数，只能设置为2或3
#define   FG_GETIMG_DLY   	0x80 		// 采样间隔，高四位有效，0x00时无间隔，0xF0间隔最大，可根据实际调整
    
enum fg_status_e{FG_STATE_IDLE,FG_STATE_ENROLL,FG_STATE_SEARCH,FG_SEARCH_N_SIGNIN,FG_DEL_ALL,FG_DEL_CUR};//待机，注册指纹，判定指纹，删除指纹,删除目前识别到的指纹
volatile extern enum fg_status_e g_fg_next_state; //s_fg_state may be mutex lock in future, lets change that at very last time 


//outer functions
void fg_init(void);

void fg_sleep(void);
uint8_t fg_wake(void);
unsigned short fg_identified_fetch_id(void);

//btn pressed signal
extern SemaphoreHandle_t g_fg_pressed_sem;
void fg_service (void *pvParameters);


#endif