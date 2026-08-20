#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "string.h"
#include "uni_font.h"

#define I2C_GPIO_SCL                GPIO_NUM_1      //SCL clock
#define I2C_GPIO_SDA                GPIO_NUM_2      //SDA
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          1250000
#define OLED_I2C_ADDRESS            0x3C             //oled addr

#define OLED_CMD  0x00
#define OLED_DATA 0x40

#define ROUND_UP()

uint8_t g_oled_buff[144][8];
static i2c_master_dev_handle_t s_oled_dev_handle = NULL;

void i2c_master_init(void) {
    //config i2c bus
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,//use sys clock
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_GPIO_SCL,
        .sda_io_num = I2C_GPIO_SDA,
        //.glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true, // 内部上拉
    };
    
    i2c_master_bus_handle_t bus_handle;
    //init i2c bus, get i2c control through handle
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    //config of oled in i2c
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_I2C_ADDRESS,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,//波特率
    };

    //reg oled to bus, send/get through handler
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &s_oled_dev_handle));
    
    printf("i2c inited.\n");
}

void oled_write_byte(uint8_t dat, uint8_t mode)
{
    uint8_t write_buf[2] = {mode, dat};
    i2c_master_transmit(s_oled_dev_handle, write_buf, sizeof(write_buf), -1);//send write_buf through handler
}

void oled_screen_update(void)
{
	//ESP_EARLY_LOGI("OLED", "Screen Refresh");
    uint8_t i;
    //send buff, 1 control byte+ 128 control byte
    uint8_t send_buf[129];
    send_buf[0] = 0x40; //control byte

    for (i = 0; i < 8; i++) {
        oled_write_byte(0xB0 + i, OLED_CMD); //设置行起始地址
        oled_write_byte(0x02, OLED_CMD);     //设置低列起始地址
        oled_write_byte(0x10, OLED_CMD);     //设置高列起始地址

        //load frame buf to send buf
        for (uint8_t n = 0; n < 128; n++) {
            send_buf[n + 1] = g_oled_buff[n][i];
        }

        //send through i2c
        i2c_master_transmit(s_oled_dev_handle, send_buf, sizeof(send_buf), -1);
    }
}

void oled_screen_clear(void)
{
    //set framebuff to full 0
    for(int i=0;i<128;i++)
		for(int j=0;j<8;j++)
			g_oled_buff[i][j]=0;
    
    //refresh screen
    oled_screen_update();
}

//画点 
//x:0~127
//y:0~63
//t:1 填充 0,清空	
void oled_draw_px(uint8_t x,uint8_t y,bool inv_clr)
{
	uint8_t i,m,n;
	i=y/8;
	m=y%8;
	n=1<<m;
	if (inv_clr) {
        g_oled_buff[x][i] |= n;//bit set to 1
    } else {
        g_oled_buff[x][i] &= ~n;//clear bit
    }
}

//画线
//x1,y1:起点坐标
//x2,y2:结束坐标
//t:1 填充 0,清空
void oled_draw_line(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2,uint8_t mode, uint8_t is_wrapped)
{
	short t; 
	int xerr=0,yerr=0,delta_x,delta_y,distance;
	int incx,incy,uRow,uCol;
	delta_x=x2-x1; //计算坐标增量 
	delta_y=y2-y1;
	uRow=x1;//画线起点坐标
	uCol=y1;
	if(delta_x>0)incx=1; //设置单步方向 
	else if (delta_x==0)incx=0;//垂直线 
	else {incx=-1;delta_x=-delta_x;}
	if(delta_y>0)incy=1;
	else if (delta_y==0)incy=0;//水平线 
	else {incy=-1;delta_y=-delta_x;}
	if(delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴 
	else distance=delta_y;
	for(t=0;t<distance+1;t++)
	{
		oled_draw_px(uRow,uCol,mode);//画点
		xerr+=delta_x;
		yerr+=delta_y;
		if(xerr>distance)
		{
			xerr-=distance;
			uRow+=incx;
		}
		if(yerr>distance)
		{
			yerr-=distance;
			uCol+=incy;
		}
	}

	if(!is_wrapped)
		oled_screen_update();
}

//画四边形
//x1,y1:UL坐标
//x2,y2:DR坐标
//mode:1 填充 0,清空
void oled_draw_rect(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2,uint8_t mode, uint8_t is_wrapped)
{
	uint8_t memset_fromy=y1+(8-y1%8)%8, memset_toy=y2-y2%8;
	for(int x=x1;x<=x2;x++)
	{
		if(memset_toy>memset_fromy)
		{
			for(int y=y1;y<memset_fromy;y++)//draw pt till next full byte
				oled_draw_px(x,y,mode);
			memset(g_oled_buff[x]+(int)(memset_fromy/8),(mode?0xFF:0x00),(int)((memset_toy-memset_fromy)/8));
			for(int y=memset_toy;y<=y2;y++)//draw pt till end
			oled_draw_px(x,y,mode);
		}else
		{
			for(int y=y1;y<=y2;y++)//draw pt till next full byte
				oled_draw_px(x,y,mode);
		}
	}

	if(!is_wrapped)
		oled_screen_update();
}

void oled_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(200));
    
 	oled_write_byte(0xAE,OLED_CMD);//--turn off oled panel
	oled_write_byte(0x00,OLED_CMD);//---set low column address
	oled_write_byte(0x10,OLED_CMD);//---set high column address
	oled_write_byte(0x40,OLED_CMD);//--set start line address  Set Mapping RAM Display Start Line (0x00~0x3F)
	oled_write_byte(0x81,OLED_CMD);//--set contrast control register
	oled_write_byte(0xCF,OLED_CMD);// Set SEG Output Current Brightness
	oled_write_byte(0xA1,OLED_CMD);//--Set SEG/Column Mapping     0xa0左右反置 0xa1正常
	oled_write_byte(0xC8,OLED_CMD);//Set COM/Row Scan Direction   0xc0上下反置 0xc8正常
	oled_write_byte(0xA6,OLED_CMD);//--set normal display
	oled_write_byte(0xA8,OLED_CMD);//--set multiplex ratio(1 to 64)
	oled_write_byte(0x3f,OLED_CMD);//--1/64 duty
	oled_write_byte(0xD3,OLED_CMD);//-set display offset	Shift Mapping RAM Counter (0x00~0x3F)
	oled_write_byte(0x00,OLED_CMD);//-not offset
	oled_write_byte(0xd5,OLED_CMD);//--set display clock divide ratio/oscillator frequency
	oled_write_byte(0x80,OLED_CMD);//--set divide ratio, Set Clock as 100 Frames/Sec
	oled_write_byte(0xD9,OLED_CMD);//--set pre-charge period
	oled_write_byte(0xF1,OLED_CMD);//Set Pre-Charge as 15 Clocks & Discharge as 1 Clock
	oled_write_byte(0xDA,OLED_CMD);//--set com pins hardware configuration
	oled_write_byte(0x12,OLED_CMD);
	oled_write_byte(0xDB,OLED_CMD);//--set vcomh
	oled_write_byte(0x30,OLED_CMD);//Set VCOM Deselect Level
	oled_write_byte(0x20,OLED_CMD);//-Set Page Addressing Mode (0x00/0x01/0x02)
	oled_write_byte(0x02,OLED_CMD);//
	oled_write_byte(0x8D,OLED_CMD);//--set Charge Pump enable/disable
	oled_write_byte(0x14,OLED_CMD);//--set(0x10) disable
	oled_write_byte(0xAF,OLED_CMD);
	oled_screen_clear();//clear screen
}
