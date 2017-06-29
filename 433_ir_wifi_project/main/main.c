/* GPIO、433、IR、WIFI TEST Example

   This example code is Contains the GPIO, 433, IR, WIFI function test.

   According to the following start each test module:
   1.By default, after power on RGBled began to shine
   2.Each press next RF button, it will switch testing capabilities,the order is GPIO->433->IR->WIFI->GPIO.
   3.When in 433、IR and wifi test mode, the program will spontaneously from the data, when receiving the specified data or connect to ap, GPIO13 LED will flash.
   4.you can see through the serial port test process.
*/

#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/timer.h"
#include "driver/rmt.h"
#include "soc/rmt_reg.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>
#include "freertos/task.h"

#define RMT_RX_ACTIVE_LEVEL  0  
#define RMT_TX_CARRIER_EN    1

#define NEC_HEADER_HIGH_UP_US    	4000     //4.5ms
#define NEC_HEADER_HIGH_DOWN_US    	3200 
                   
#define NEC_HEADER_LOW_UP_US     	7600  	 //9ms
#define NEC_HEADER_LOW_DOWN_US     	6800                       

#define NEC_BIT_ONE_HIGH_UP_US   	1520     //1.68ms
#define NEC_BIT_ONE_HIGH_DOWN_US   	1168 
                   
#define NEC_BIT_ONE_LOW_UP_US    	624 	//560us
#define NEC_BIT_ONE_LOW_DOWN_US    	272  	//

#define NEC_BIT_ZERO_HIGH_UP_US  	624 	//560us
#define NEC_BIT_ZERO_HIGH_DOWN_US  	272		
                        
#define NEC_BIT_ZERO_LOW_UP_US   	624  	//560us
#define NEC_BIT_ZERO_LOW_DOWN_US   	272         

#define RMT_CLK_DIV      			100
#define rmt_item32_tIMEOUT_US  		7200
#define RMT_TICK_10_US    			(80000000/RMT_CLK_DIV/8000)
#define NEC_DATA_ITEM_NUM   		34

#define RMT_RECV_CHANNEL    		0
#define RMT_SEND_CHANNEL    		1
///////////////////////////////////////////////////////IO# 
#define GPIO_EXT_INFR_SEND_IO   	18
#define GPIO_EXT_INFR_RECV_IO		15

#define GPIO_433_RECV_IO   			37
#define READ_433_PIN_VAL			gpio_get_level(GPIO_433_RECV_IO)
#define GPIO_433_SEND_IO   			16

#define GPIO_KEY_IO					38
#define READ_KEY_PIN_VAL			gpio_get_level(GPIO_KEY_IO)

#define ECHO_TEST_RXD 				3
#define ECHO_TEST_TXD 				1

#define GPIO_RGB_RED_IO             25
#define GPIO_RGB_GREEN_IO           26
#define GPIO_RGB_BLUE_IO            27

#define GPIO_YELLOW_LED             13
//////////////////////////////////////////////////////FUN
#define RGB_TEST					0
#define WIRELESS_433_TEST			1
#define IR_TEST						2
#define WIFI_TEST					3

#define AP_READ_CNT		            30

int timer1_group = TIMER_GROUP_0;
int timer1_idx = TIMER_1;
static const char* NEC_TAG = "NEC";
static xQueueHandle evt_queue = NULL;

static int fun_choice = 0;

bool key_status = 1;
bool key_old = 1;
unsigned char longkey = 0;
unsigned char key_down_time=0;
unsigned char key_up_time=0;
unsigned int key_long_time = 0;

uint8_t scan_flag = 0;
bool wifi_ok_flag = 0;
wifi_ap_record_t ap_records[AP_READ_CNT];

void decode_433_1527(void);
void crcode_433_1527(void);

esp_err_t event_handler(void *ctx, system_event_t *event)
{	
	switch(event->event_id)
	{
		case SYSTEM_EVENT_STA_START:
			uart_write_bytes(UART_NUM_0,(const char*)"wifi_test_start\r\n",17);
			break;
		case SYSTEM_EVENT_WIFI_READY:
			//uart_write_bytes(UART_NUM_0,(const char*)"WIFI_STA_STATUS_READY",21);
			break;
		case SYSTEM_EVENT_SCAN_DONE:
			//scan_flag = 2;
			//uart_write_bytes(UART_NUM_0,(const char*)"WIFI_STA_STATUS_SCDON\r\n",23);
			break;
		default:break;
	}
    return ESP_OK;
}

void IRAM_ATTR timer_group0_isr(void *para)
{
	int timer_idx = (int)para;
	uint32_t intr_status = TIMERG0.int_st_timers.val;
	
	if((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0)
	{
		TIMERG0.hw_timer[timer_idx].update = 1;
		TIMERG0.int_clr_timers.t0 = 1;
		decode_433_1527();
		TIMERG0.hw_timer[timer_idx].config.alarm_en = 1;
	}
}

void uart0_init(void)
{
	uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	
	uart_param_config(UART_NUM_0,&uart_config);
	uart_set_pin(UART_NUM_0,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
	uart_driver_install(UART_NUM_0,512*2,0,0,NULL,0);
	
}

void timer0_init(void)
{
	timer_config_t config;
	int timer_group = TIMER_GROUP_0;
	int timer_idx = TIMER_0;
	
	config.alarm_en = 1;
	config.auto_reload = 1;
	config.counter_dir = TIMER_COUNT_UP;
	config.divider = 16;
	config.intr_type = TIMER_INTR_LEVEL;
	config.counter_en = TIMER_PAUSE;
	
	timer_init(timer_group,timer_idx,&config);
	
	timer_set_counter_value(timer_group,timer_idx,0x00000000ULL);
	timer_set_alarm_value(timer_group,timer_idx,0x00000050ULL);
	
	timer_enable_intr(timer_group,timer_idx);
    timer_isr_register(timer_group, timer_idx, timer_group0_isr, (void *)timer_idx, ESP_INTR_FLAG_SHARED, NULL); 
	
	timer_start(timer_group,timer_idx);
}

void timer1_init(void)
{
	timer_config_t config;

	
	config.alarm_en = 1;
	config.auto_reload = 0;
	config.counter_dir = TIMER_COUNT_UP;
	config.divider = 16;
	config.intr_type = TIMER_INTR_LEVEL;
	config.counter_en = TIMER_PAUSE;
	
	timer_init(timer1_group,timer1_idx,&config);
	
	timer_set_counter_value(timer1_group,timer1_idx,0x00000000ULL);
}

void RGB_gpio_init(void)
{
	uint8_t i = 0;

	uint8_t gpio_pin_array[3] = {
        GPIO_RGB_RED_IO,GPIO_RGB_GREEN_IO,
        GPIO_RGB_BLUE_IO
	};
	
	for(i=0;i<3;i++)
	{
		gpio_pad_select_gpio(gpio_pin_array[i]);
		gpio_set_direction(gpio_pin_array[i], GPIO_MODE_OUTPUT);
	}
}

void RGB_light_open(int num)
{
	switch(num)
	{
        case GPIO_RGB_RED_IO:
            gpio_set_level(GPIO_RGB_RED_IO, 1); 
            gpio_set_level(GPIO_RGB_GREEN_IO, 0); 
            gpio_set_level(GPIO_RGB_BLUE_IO, 0); 
			break;
        case GPIO_RGB_GREEN_IO:
            gpio_set_level(GPIO_RGB_RED_IO, 0); 
            gpio_set_level(GPIO_RGB_GREEN_IO, 1); 
            gpio_set_level(GPIO_RGB_BLUE_IO, 0); 
			break;
        case GPIO_RGB_BLUE_IO:
            gpio_set_level(GPIO_RGB_RED_IO, 0); 
            gpio_set_level(GPIO_RGB_GREEN_IO, 0); 
            gpio_set_level(GPIO_RGB_BLUE_IO, 1); 
			break;
		default:
            gpio_set_level(GPIO_RGB_RED_IO, 0); 
            gpio_set_level(GPIO_RGB_GREEN_IO, 0); 
            gpio_set_level(GPIO_RGB_BLUE_IO, 0); 
			break;
	}
}

void Recv_433_gpio_init(void)
{	
	gpio_pad_select_gpio(GPIO_433_RECV_IO);
	gpio_set_direction(GPIO_433_RECV_IO,GPIO_MODE_INPUT);
}

void Send_433_gpio_init(void)
{
	gpio_pad_select_gpio(GPIO_433_SEND_IO);
	gpio_set_direction(GPIO_433_SEND_IO,GPIO_MODE_OUTPUT);
	gpio_set_pull_mode(GPIO_433_SEND_IO,GPIO_PULLUP_ONLY);
}

void key_gpio_init(void)
{
	gpio_pad_select_gpio(GPIO_KEY_IO);
	gpio_set_direction(GPIO_KEY_IO,GPIO_MODE_INPUT);
}

void yellow_led_gpio_init(void)
{
    gpio_pad_select_gpio(GPIO_YELLOW_LED);
    gpio_set_direction(GPIO_YELLOW_LED, GPIO_MODE_DEF_OUTPUT); 
}

static void nec_recv_init()
{
	rmt_config_t rmt_rx;
	
	rmt_rx.channel = RMT_RECV_CHANNEL;
	rmt_rx.gpio_num = GPIO_EXT_INFR_RECV_IO;
	rmt_rx.clk_div = RMT_CLK_DIV;
	rmt_rx.mem_block_num = 1;
	rmt_rx.rmt_mode = RMT_MODE_RX;
	rmt_rx.rx_config.filter_en = true;
	rmt_rx.rx_config.filter_ticks_thresh = 100;
	rmt_rx.rx_config.idle_threshold = 8000;
	rmt_config(&rmt_rx);
    rmt_driver_install(rmt_rx.channel, 1000, 0);
}

static void nec_tx_init()
{
	rmt_config_t rmt_tx;
	
	rmt_tx.channel = RMT_SEND_CHANNEL;
	rmt_tx.gpio_num = GPIO_EXT_INFR_SEND_IO;
	rmt_tx.mem_block_num = 1;
	rmt_tx.clk_div = RMT_CLK_DIV;
	rmt_tx.tx_config.loop_en = false;
	rmt_tx.tx_config.carrier_duty_percent = 30;
	rmt_tx.tx_config.carrier_freq_hz = 38000;
	rmt_tx.tx_config.carrier_level = 1;
	rmt_tx.tx_config.carrier_en = RMT_TX_CARRIER_EN;
	rmt_tx.tx_config.idle_level = 0;
	rmt_tx.tx_config.idle_output_en = true;
	rmt_tx.rmt_mode = 0;
	rmt_config(&rmt_tx);
	rmt_driver_install(rmt_tx.channel,0,0);
}

void wifi_sta_mode_init(void)
{
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    int ret = 0;

	tcpip_adapter_init();
	esp_event_loop_init(event_handler,NULL);
	esp_wifi_deinit();
	ret = esp_wifi_init(&config);
    printf("ret:%d\r\n",ret);
    esp_wifi_set_mode(WIFI_MODE_STA);
	esp_wifi_start();
}

static uint32_t decodeWireless(void)
{
	static bool syn_code_flag = false;
	static uint16_t low_cnt = 0;
	static uint8_t bit_cnt = 0;
	static uint32_t wireless_data = 0;
	uint32_t ret_data = 0;
	static uint16_t high_cnt = 0;
	
	if(!syn_code_flag)
	{
		if(READ_433_PIN_VAL == 0)
		{
			low_cnt++;
		}
		if(READ_433_PIN_VAL == 1)
		{
			if(low_cnt > 550 && low_cnt < 1500)
			{
				syn_code_flag = true;
			}
			low_cnt = 0;
		}
	}
	if(syn_code_flag)
	{
		if(READ_433_PIN_VAL == 1)
		{
			high_cnt++;
		}
		else{
			if((high_cnt) >= 60 && high_cnt < 150)
			{
				wireless_data = (wireless_data << 1) | 0x01;
				bit_cnt++;
			}
			 else if((high_cnt) >= 7 && (high_cnt < 60))
            {
            	wireless_data = wireless_data << 1;
                bit_cnt++;
            }
            else if(high_cnt > 0)
            {
                syn_code_flag = false;
                bit_cnt = 0;
                wireless_data = 0;
            }
            high_cnt = 0;
		}
		if(bit_cnt >= 24)
        {
            ret_data = wireless_data;
            syn_code_flag = false;
            bit_cnt = 0;
            wireless_data = 0;
        }
	}
	return ret_data;
}

void decode_433_1527(void)
{
	uint32_t decode_data = 0;
	
	decode_data = decodeWireless();

	if(decode_data != 0)
	{
		//uart_write_bytes(UART_NUM_0,&decode_data,4);
		xQueueSendFromISR(evt_queue, &decode_data, NULL);
	}
}

void send_433_one_bit(void)
{
	uint64_t high_time = 0;
	uint64_t low_time = 0;
	
	timer_start(TIMER_GROUP_0,TIMER_1);
	gpio_set_level(GPIO_433_SEND_IO, 1);
	while(1)
	{
		timer_get_counter_value(TIMER_GROUP_0,TIMER_1,&high_time);
		if(high_time>7500)
		{
			break;
		}
	}
	timer_pause(TIMER_GROUP_0,TIMER_1);
	timer_set_counter_value(timer1_group,timer1_idx,0x00000000ULL);
	timer_start(TIMER_GROUP_0,TIMER_1);
	high_time = 0;
	gpio_set_level(GPIO_433_SEND_IO, 0);
	while(1)
	{
		timer_get_counter_value(TIMER_GROUP_0,TIMER_1,&low_time);
		if(low_time>2500)
		{
			break;
		}
	}
	gpio_set_level(GPIO_433_SEND_IO, 1);
	timer_pause(TIMER_GROUP_0,TIMER_1);
	timer_set_counter_value(timer1_group,timer1_idx,0x00000000ULL);
}

void send_433_zero_bit(void)
{
	uint64_t high_time = 0;
	uint64_t low_time = 0;
	
	timer_start(TIMER_GROUP_0,TIMER_1);
	gpio_set_level(GPIO_433_SEND_IO, 1);
	while(1)
	{
		timer_get_counter_value(TIMER_GROUP_0,TIMER_1,&high_time);
		if(high_time>2500)
		{
			break;
		}
	}
	timer_pause(TIMER_GROUP_0,TIMER_1);
	timer_set_counter_value(timer1_group,timer1_idx,0x00000000ULL);
	timer_start(TIMER_GROUP_0,TIMER_1);
	high_time = 0;
	gpio_set_level(GPIO_433_SEND_IO, 0);
	while(1)
	{
		timer_get_counter_value(TIMER_GROUP_0,TIMER_1,&low_time);
		if(low_time>7500)
		{
			break;
		}
	}
	gpio_set_level(GPIO_433_SEND_IO, 1);
	timer_pause(TIMER_GROUP_0,TIMER_1);
	timer_set_counter_value(timer1_group,timer1_idx,0x00000000ULL);
}

void build_433_head(void)
{
	uint64_t high_time = 0;
	uint64_t low_time = 0;
	
	timer_start(TIMER_GROUP_0,TIMER_1);
	gpio_set_level(GPIO_433_SEND_IO, 1);
	while(1)
	{
		timer_get_counter_value(TIMER_GROUP_0,TIMER_1,&high_time);
		if(high_time>2500)
		{
			break;
		}
	}
	timer_pause(TIMER_GROUP_0,TIMER_1);
	timer_set_counter_value(timer1_group,timer1_idx,0x00000000ULL);
	timer_start(TIMER_GROUP_0,TIMER_1);
	high_time = 0;
	gpio_set_level(GPIO_433_SEND_IO, 0);
	while(1)
	{
		timer_get_counter_value(TIMER_GROUP_0,TIMER_1,&low_time);
		if(low_time>57500)
		{
			break;
		}
	}	
	timer_pause(TIMER_GROUP_0,TIMER_1);
	timer_set_counter_value(timer1_group,timer1_idx,0x00000000ULL);
}

void build_433_data(uint32_t addr,uint32_t key)
{
	uint32_t data = ((addr<<4)|key)<<8;
	uint32_t i = 0;
	
	for(i=0;i<24;i++)
	{
		if(data & 0x80000000)
			send_433_one_bit();
		else
			send_433_zero_bit();
		
		data <<= 1;
	}
}

void crcode_433_1527(void)
{
	uint32_t addr = 0x95e01; //addr 	20bit
	uint32_t key = 0x01;	//key_code  4bit
	
	if(fun_choice==WIRELESS_433_TEST)
	{
		gpio_set_level(GPIO_NUM_16, 1);
		build_433_head();
		build_433_data(addr,key);
	}
}

static bool nec_header_if(rmt_item32_t* item)
{
	if((item->duration0 > NEC_HEADER_LOW_DOWN_US) && (item->duration0 < NEC_HEADER_LOW_UP_US))
	{
		if((item->duration1 > NEC_HEADER_HIGH_DOWN_US) && (item->duration1 < NEC_HEADER_HIGH_UP_US))
		{
			return true;
		}
	}
	
	return false;
}

static bool nec_bit_one_if(rmt_item32_t* item)
{
	if((item->duration0 > NEC_BIT_ONE_LOW_DOWN_US) && (item->duration0 < NEC_BIT_ONE_LOW_UP_US))
	{
		if((item->duration1 > NEC_BIT_ONE_HIGH_DOWN_US) && (item->duration1 < NEC_BIT_ONE_HIGH_UP_US))
		{
			return true;
		}
	}
	
	return false;
}

static bool nec_bit_zero_if(rmt_item32_t* item)
{
	if((item->duration0 > NEC_BIT_ZERO_LOW_DOWN_US) && (item->duration0 < NEC_BIT_ZERO_LOW_UP_US))
	{
		if((item->duration1 > NEC_BIT_ZERO_HIGH_DOWN_US) && (item->duration1 < NEC_BIT_ZERO_HIGH_UP_US))
		{
			return true;
		}
	}
	
	return false;
}

uint8_t Reverse_data(uint8_t dat)
{
    uint8_t val;
	val = ((dat & 0x01) << 7) | ((dat & 0x02) << 5) | ((dat & 0x04) << 3) | ((dat & 0x08) << 1) |
      ((dat & 0x10) >> 1) | ((dat & 0x20) >> 3) | ((dat & 0x40) >> 5) | ((dat & 0x80) >> 7);
	  
    return  val;
} 

static int nec_parse_items(rmt_item32_t* item, int item_num, uint8_t* addr, uint8_t* data)
{
	int w_len = item_num;
	int i = 0, j = 0;
	uint8_t addr_high_8 = 0;
	uint8_t addr_low_8 = 0;
	uint8_t data_high_8 = 0;
	uint8_t data_low_8 = 0;
	
	if(w_len < NEC_DATA_ITEM_NUM)
	{
		//printf("out num less:%d\r\n",w_len);
		return -1;
	}
	
	//printf("num ok:%d\r\n",w_len);
	if(!nec_header_if(item++))
	{
		//printf("out head\r\n");
		return -1;
	}
	
    for(j = 0; j < 8; j++) {
        if(nec_bit_one_if(item)) {
            addr_high_8 |= (1 << j);
        } else if(nec_bit_zero_if(item)) {
            addr_high_8 |= (0 << j);
        } else {
            return -1;
        }
		//printf("head level0:%d\r\n",item->level0);
		//printf("head level1:%d\r\n",item->level1);
		//printf("head duration0:%d\r\n",item->duration0);
		//printf("head duration1:%d\r\n",item->duration1);
		item++;
        i++;
    }
	
    for(j = 0; j < 8; j++) {
        if(nec_bit_one_if(item)) {
            addr_low_8 |= (1 << j);
        } else if(nec_bit_zero_if(item)) {
            addr_low_8 |= (0 << j);
        } else {
            return -1;
        }
        //printf("head level0:%d\r\n",item->level0);
		//printf("head level1:%d\r\n",item->level1);
		//printf("head duration0:%d\r\n",item->duration0);
		//printf("head duration1:%d\r\n",item->duration1);
		item++;
        i++;
    }
	
	for(j = 0; j < 8; j++) {
        if(nec_bit_one_if(item)) {
            data_high_8 |= (1 << j);
        } else if(nec_bit_zero_if(item)) {
            data_high_8 |= (0 << j);
        } else {
            return -1;
        }
        //printf("head level0:%d\r\n",item->level0);
		//printf("head level1:%d\r\n",item->level1);
		//printf("head duration0:%d\r\n",item->duration0);
		//printf("head duration1:%d\r\n",item->duration1);
		item++;
        i++;
    }
	
    for(j = 0; j < 8; j++) {
        if(nec_bit_one_if(item)) {
            data_low_8 |= (1 << j);
        } else if(nec_bit_zero_if(item)) {
            data_low_8 |= (0 << j);
        } else {
            return -1;
        }
        //printf("head level0:%d\r\n",item->level0);
		//printf("head level1:%d\r\n",item->level1);
		//printf("head duration0:%d\r\n",item->duration0);
		//printf("head duration1:%d\r\n",item->duration1);
		item++;
        i++;
    }
	
	//ESP_LOGI(NEC_TAG, "RMT RCV --- addr_H: 0x%02x addr_L: 0x%02x  data_H: 0x%02x  data_L: 0x%02x", addr_high_8, addr_low_8,data_high_8,data_low_8);
	printf("rmt recv addr_h:0x%x addr_l:0x%x data_h:0x%x data_l:0x%x\r\n",addr_high_8, addr_low_8,data_high_8,data_low_8);
	*addr = addr_high_8;
	*data = data_high_8;
	
    if (addr_high_8==0x00 && data_high_8==0xaa)
    {
        gpio_set_level(GPIO_YELLOW_LED, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_YELLOW_LED, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
	
	return i;
}

static void nec_fill_item_header(rmt_item32_t *item)
{
	item->level0 = 1;
	item->duration0 = 7200;
	item->level1 = 0;
	item->duration1 = 3600;
}

static void nec_fill_item_bit_one(rmt_item32_t *item)
{
	item->level0 = 1;
	item->duration0 = 488;
	item->level1 = 0;
	item->duration1 = 1344;
}

static void nec_fill_item_bit_zero(rmt_item32_t *item)
{
	item->level0 = 1;
	item->duration0 = 448;
	item->level1 = 0;
	item->duration1 = 448;
}

static void nec_fill_item_end(rmt_item32_t* item)
{
	item->level0 = 1;
	item->duration0 = 488;
	item->level1 = 0;
	item->duration1 = 1344;
}

static int nec_build_items(int channel,rmt_item32_t *item,int item_num,uint8_t addr,uint8_t data)
{
	int i = 0, j = 0;
	uint8_t addr_reverse = ~addr;
	uint8_t data_reverse = ~data;
	
	if(fun_choice!=IR_TEST)
	{
		return -1;
	}
	
	if(item_num < NEC_DATA_ITEM_NUM)
	{
		return -1;
	}
	
	nec_fill_item_header(item++);
	i++;
	
	for(j=0;j<8;j++)
	{
		if(addr & 0x01)
			nec_fill_item_bit_one(item);
		else
			nec_fill_item_bit_zero(item);
		
		item++;
		i++;
		addr >>= 1;
	}
	
	for(j=0;j<8;j++)
	{
		if(addr_reverse & 0x01)
			nec_fill_item_bit_one(item);
		else
			nec_fill_item_bit_zero(item);
		
		item++;
		i++;
		addr_reverse >>= 1;
	}
	
	for(j=0;j<8;j++)
	{
		if(data & 0x01)
			nec_fill_item_bit_one(item);
		else
			nec_fill_item_bit_zero(item);
		
		item++;
		i++;
		data >>= 1;
	}
	
	for(j=0;j<8;j++)
	{
		if(data_reverse & 0x01)
			nec_fill_item_bit_one(item);
		else
			nec_fill_item_bit_zero(item);
		
		item++;
		i++;
		data_reverse >>= 1;
	}
	
	nec_fill_item_end(item);
	i++;
	return i;
}

void Rgb_test(int gpio)
{
	if(fun_choice==RGB_TEST)
	{
        RGB_light_open(gpio); 
	}
}

static void RGB_flashing_task()
{
	RGB_gpio_init();
	
	while(1)
	{
        if (fun_choice==RGB_TEST)
        {
            Rgb_test(GPIO_RGB_RED_IO);
            vTaskDelay(1500 / portTICK_PERIOD_MS);
            Rgb_test(GPIO_RGB_GREEN_IO); 
            vTaskDelay(1500 / portTICK_PERIOD_MS);
            Rgb_test(GPIO_RGB_BLUE_IO); 
            vTaskDelay(1500 / portTICK_PERIOD_MS);
        }
        else
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

	}
	vTaskDelete(NULL);
}

static void RMT_nec_recv_task()
{
	int channel = RMT_RECV_CHANNEL;
	int res = 0;
	
	nec_recv_init();
	RingbufHandle_t rb = NULL;
	
	rmt_get_ringbuf_handler(channel,&rb);
	rmt_rx_start(channel,1);
	
	while(rb)
	{
		size_t rx_size = 0;
		
		rmt_item32_t *item = (rmt_item32_t*)xRingbufferReceive(rb,&rx_size,1000);
		if(item)
		{
			uint8_t rmt_addr;
			uint8_t rmt_cmd;
			int offset = 0;
			while(1)
			{
				res = nec_parse_items(item+offset,(rx_size/4)-offset,&rmt_addr,&rmt_cmd);
				if(res > 0)
				{
					offset += res + 1;
				}
				else{
					break;
				}
			}
			vRingbufferReturnItem(rb, (void*) item);
		}
	}
	vTaskDelete(NULL);
}

static void RMT_nec_send_task()
{
	#if 1
	vTaskDelay(10);
	int channel = RMT_SEND_CHANNEL;
	int nec_tx_num = 1;
	uint8_t addr = 0x00;
	uint8_t data = 0xaa;
	size_t size;
	int item_num = 0;
	
	nec_tx_init();
	esp_log_level_set(NEC_TAG, ESP_LOG_INFO);
	
	for(;;)
	{
		
		size = (sizeof(rmt_item32_t) * NEC_DATA_ITEM_NUM * nec_tx_num);
		rmt_item32_t *item = (rmt_item32_t*)malloc(size);
		item_num = NEC_DATA_ITEM_NUM*nec_tx_num;
		memset((void*)item,0,size);
		
		int i, offset = 0;
		while(1)
		{
			i = nec_build_items(channel,item+offset,item_num-offset,addr,data);
			if(i < 0)
			{
				break;
			}
			offset += i;
		}
		rmt_write_items(channel,item,item_num,true);
		rmt_wait_tx_done(channel);
		free(item);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
	vTaskDelete(NULL);
	#endif
}

static void Send_433_task()
{
	while(1)
	{
		crcode_433_1527();
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
	vTaskDelete(NULL);
}

static void Recv_433_task()
{
	uint32_t decode_data = 0;
	uint32_t addr = 0;
	uint32_t key_code = 0;
	
	while(1)
	{
		if(xQueueReceive(evt_queue, &decode_data, portMAX_DELAY)) 
		{
			addr = decode_data>>4;
			key_code = decode_data&0x0F;
			//printf("433 recv:0x%x\r\n",decode_data);
            printf("433 recv addr:0x%x key_code:0x%x\r\n",addr,key_code);
            if (addr==0x95e01 && key_code==0x01)
            {
                 gpio_set_level(GPIO_YELLOW_LED, 1);
                 vTaskDelay(500 / portTICK_PERIOD_MS);
                 gpio_set_level(GPIO_YELLOW_LED, 0);
                 vTaskDelay(500 / portTICK_PERIOD_MS);
            }
		}
        else
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
	}
}

void debounce_handle(void *arg)
{	
	while(1)
	{
		if(READ_KEY_PIN_VAL==0)
		{
			key_down_time++;
		}
		else{
			key_up_time++;
		}
		
		if(key_down_time>=30)
		{
			key_down_time = 0;
			key_status = 0;
		}
		else if(key_up_time>=30)
		{
			key_up_time = 0;
			key_status = 1;
		}
		vTaskDelay(1 / portTICK_RATE_MS);
	}
}

int Check_wifi_isok(uint8_t *ssid, uint8_t *bssid,int8_t rssi)
{
	uint16_t ret = 0;
	uint8_t rssi_s[4] = {0};
	wifi_config_t sta_config = {
        .sta = {
            .ssid = "C_TEST",
            .password = "094FAFE8",
            .bssid_set = false
        }
	};
	
	if(!strcmp((const char*)ssid,"C_TEST"))
	{
		ret = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
		if(ret == ESP_OK)
		{
			ret = esp_wifi_connect();
			if(ret == ESP_OK)
			{
				uart_write_bytes(UART_NUM_0,(const char*)"connect to c_test succeed\r\n",27);
				uart_write_bytes(UART_NUM_0,(const char*)"wifi rssi:-",11);
				if(rssi < 0)
					rssi = -rssi;
				rssi_s[0] = (rssi&0x7F)/10+'0';
				rssi_s[1] = (rssi&0x7F)%10+'0';
				rssi_s[2] = '\r';
				rssi_s[3] = '\n';
				uart_write_bytes(UART_NUM_0,(const char*)rssi_s,4);
				ret = esp_wifi_disconnect();
				if(ret == ESP_OK)
				{
					uart_write_bytes(UART_NUM_0,(const char*)"close connect to c_test succeed\r\n",33);
					return true;
				}
			}
		}
        else
        {
            uart_write_bytes(UART_NUM_0,(const char*)"connect to c_test failure\r\n",27);
        }
    }
	else if(!strcmp((const char*)ssid,"YangHuaiMi"))
	{
		strcpy((char *)&sta_config.sta.ssid,(const char*)ssid);
		strcpy((char *)&sta_config.sta.ssid,(const char*)"TianShuiTeChan");
		ret = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
		if(ret == ESP_OK)
		{
			ret = esp_wifi_connect();
			if(ret == ESP_OK)
			{
				uart_write_bytes(UART_NUM_0,(const char*)"connect to YangHuaiMi succeed\r\n",31);
				uart_write_bytes(UART_NUM_0,(const char*)"wifi rssi:-",11);
				if(rssi < 0)
					rssi = -rssi;
				rssi_s[0] = (rssi&0x7F)/10+'0';
				rssi_s[1] = (rssi&0x7F)%10+'0';
				rssi_s[2] = '\r';
				rssi_s[3] = '\n';
				uart_write_bytes(UART_NUM_0,(const char*)rssi_s,4);
				ret = esp_wifi_disconnect();
				if(ret == ESP_OK)
				{
					uart_write_bytes(UART_NUM_0,(const char*)"close connect to YangHuaiMi succeed\r\n",38);
					return true;
				}
			}
		}
        else
        {
            uart_write_bytes(UART_NUM_0,(const char*)"connect to YangHuaiMi failure\r\n",31);
        }
    }
    else if(!strcmp((const char*)ssid,"8DB0839D"))
	{
		strcpy((char *)&sta_config.sta.ssid,(const char*)ssid);
		strcpy((char *)&sta_config.sta.ssid,(const char*)"094FAFE8");
		ret = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
		if(ret == ESP_OK)
		{
			ret = esp_wifi_connect();
			if(ret == ESP_OK)
			{
				uart_write_bytes(UART_NUM_0,(const char*)"connect to 8DB0839D succeed\r\n",31);
				uart_write_bytes(UART_NUM_0,(const char*)"wifi rssi:-",11);
				if(rssi < 0)
					rssi = -rssi;
				rssi_s[0] = (rssi&0x7F)/10+'0';
				rssi_s[1] = (rssi&0x7F)%10+'0';
				rssi_s[2] = '\r';
				rssi_s[3] = '\n';
				uart_write_bytes(UART_NUM_0,(const char*)rssi_s,4);
				ret = esp_wifi_disconnect();
				if(ret == ESP_OK)
				{
					uart_write_bytes(UART_NUM_0,(const char*)"close connect to 8DB0839D succeed\r\n",35);
					return true;
				}
			}
		}
        else
        {
            uart_write_bytes(UART_NUM_0,(const char*)"connect to 8DB0839D failure\r\n",29);
        }
    }
	else{
		return false;
	}
	
	return false;
}

int wifi_run(void)
{
	uint16_t ret = 0;
	uint16_t i = AP_READ_CNT;
	uint16_t j = 0;
	uint8_t  cnt[4] = {0};

	wifi_scan_config_t scan_config = {
		.show_hidden = false,
		.scan_type = WIFI_SCAN_TYPE_ACTIVE,
		.scan_time = {.active = {.min=500}}
	};
	
	if(fun_choice!=WIFI_TEST)
	{
		return -1;
	}
	
	while(1)
	{
		if(scan_flag==0)
		{
			//uart_write_bytes(UART_NUM_0,(const char*)"WIFI_STA_STATUS_START\r\n",23);
			ret = esp_wifi_scan_start(&scan_config,true);
			if(ret == ESP_OK)
			{
				scan_flag = 1;
			}
		}
		else 
		{
			ret = esp_wifi_scan_get_ap_records(&i,ap_records);
			uart_write_bytes(UART_NUM_0,(const char*)"get_ap_cnt:",11);
			if(i < 10)
			{
				cnt[0] = '0';
				cnt[1] = i + '0';
				cnt[2] = '\r';
				cnt[3] = '\n';
			}
			else{
				cnt[0] = (i/10)+'0';
				cnt[1] = (i%10)+'0';
				cnt[2] = '\r';
				cnt[3] = '\n';
			}
			uart_write_bytes(UART_NUM_0,(const char*)cnt,4);
			if(ret == ESP_OK && i > 0)
			{
				for(j=0;j<i;j++)
				{
					//printf_info_to_uart(ap_records[j].ssid,ap_records[j].bssid,ap_records[j].rssi);
					ret = Check_wifi_isok(ap_records[j].ssid,ap_records[j].bssid,ap_records[j].rssi);
					if(ret)
					{
						wifi_ok_flag = 1;
					}
				}
				ret = esp_wifi_scan_stop();
				if(ret == ESP_OK)
				{
					scan_flag = 0;
					memset(&ap_records,0,sizeof(wifi_ap_record_t));
					if(wifi_ok_flag==1)
					{
						return true;
					}
					else{
						return false;
					}
				}
			}
			else{
				return false;
			}
		}
	}
}

static void wifi_fun_test()
{
	int ret = 0;

	wifi_sta_mode_init();
	
	while(1)
	{
		ret = wifi_run();
		if(ret>0)
		{
			gpio_set_level(GPIO_YELLOW_LED, 1);
			vTaskDelay(500 / portTICK_PERIOD_MS);
			gpio_set_level(GPIO_YELLOW_LED, 0);
            vTaskDelay(500 / portTICK_PERIOD_MS);
		}
        else
        {
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
	}
	vTaskDelete(NULL);
}

void app_main(void)
{
	uart0_init();
    nvs_flash_init();
	Recv_433_gpio_init();
	Send_433_gpio_init();
    yellow_led_gpio_init();
	key_gpio_init();

	timer0_init();
	timer1_init();

	evt_queue = xQueueCreate(10, sizeof(uint32_t));
	
	xTaskCreate(RGB_flashing_task,"RGB_flashing_task",512,NULL,10,NULL);
	xTaskCreate(Send_433_task,"Send_433_task",2048,NULL,7,NULL);
	xTaskCreate(Recv_433_task,"Recv_433_task",2048,NULL,8,NULL);	
	xTaskCreate(&RMT_nec_recv_task,"RMT_nec_recv_task",2048,NULL,10,NULL);
	xTaskCreate(&RMT_nec_send_task,"RMT_nec_send_task",2048,NULL,9,NULL);
	xTaskCreate(&wifi_fun_test,"wifi_fun_test",4096,NULL,10,NULL);

	xTaskCreate(&debounce_handle, "debounce_handle", 1024, NULL, 1, NULL);
	printf("fun_choice:RGB test\r\n");

	while(1)
	{
        #if 1
		if(key_status != key_old)
		{
			if(key_status == 1)
			{
				if(fun_choice >= 3)
				{
					fun_choice = 0;
				}
				else{
					fun_choice++;
				}
                if (fun_choice==1)
                {
                    printf("fun_choice:433 test\r\n");
                    RGB_light_open(GPIO_RGB_RED_IO); 
                }            
                else if (fun_choice==2)
                {
                    printf("fun_choice:IR test\r\n");
                    RGB_light_open(GPIO_RGB_GREEN_IO); 
                }                    
                else if (fun_choice==3)
                {
                    printf("fun_choice:WIFI test\r\n");
                    RGB_light_open(GPIO_RGB_BLUE_IO); 
                }
                else if (fun_choice==0)
                {
                   printf("fun_choice:RGB test\r\n");
                }
            }
			key_old = key_status;
		}
        #endif
	}
}

