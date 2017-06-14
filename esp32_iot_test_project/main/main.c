#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"
#include "driver/uart.h"
#include "freertos/task.h"
#include <string.h>

#define LED_CNT 		18
#define AP_READ_CNT		30


#define ECHO_TEST_RXD 	3
#define ECHO_TEST_TXD 	1
#define ECHO_TEST_RTS  	23
#define ECHO_TEST_CTS  	19
#define BUF_SIZE 		100

uint8_t scan_flag = 0;
bool wifi_ok_flag = 0;
wifi_ap_record_t ap_records[AP_READ_CNT];

void printf_info_to_uart(uint8_t *ssid,uint8_t *bssid,int8_t rssi)
{
	int8_t rssi_buf[2] = {0};
	if(rssi < 0)
	{
		rssi = -rssi;
	}
	
	rssi_buf[0] = (rssi&0x7F)/10+'0';
	rssi_buf[1] = (rssi&0x7F)%10+'0';
	
	uart_write_bytes(UART_NUM_0,(const char*)"ssid:",5);
	uart_write_bytes(UART_NUM_0,(const char*)ssid,33);
	//uart_write_bytes(UART_NUM_0,(const char*)"\r\nbssid:",8);
	//uart_write_bytes(UART_NUM_0,(const char*)bssid,6);
	uart_write_bytes(UART_NUM_0,(const char*)"\r\nrssi:-",8);
	uart_write_bytes(UART_NUM_0,(const char*)rssi_buf,2);
	uart_write_bytes(UART_NUM_0,(const char*)"\r\n\r\n",4);
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
					uart_write_bytes(UART_NUM_0,(const char*)"close connect to YangHuaiMi succeed\r\n",37);
					return true;
				}
			}
		}
    }
	else if(rssi >= -90)
	{
		uart_write_bytes(UART_NUM_0,(const char*)"wifi signal strength > -90,ok\r\n",31);
		return true;
	}
	else{
		return false;
	}
	
	return false;
}

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

void gpio_test_config(uint8_t *pin)
{
	uint8_t i = 0;
	
	for(i=0;i<LED_CNT;i++)
	{
		gpio_pad_select_gpio(pin[i]);
		gpio_set_direction(pin[i], GPIO_MODE_OUTPUT);
	}
}

void wifi_sta_mode_init(void)
{
	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler,NULL));
	wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_deinit());
	ESP_ERROR_CHECK(esp_wifi_init(&config));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());
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
	uart_driver_install(UART_NUM_0,BUF_SIZE*2,0,0,NULL,0);
	
}

void gpio_test(void)
{
	uint8_t i = 0;
	uint8_t level;
	uint8_t gpio_pin_array[20] = {
		GPIO_NUM_23,GPIO_NUM_22,
		GPIO_NUM_27,GPIO_NUM_14,
		GPIO_NUM_5, GPIO_NUM_13,
		GPIO_NUM_25,GPIO_NUM_16,
		GPIO_NUM_18,GPIO_NUM_0,
		GPIO_NUM_26,GPIO_NUM_4,
		GPIO_NUM_19,GPIO_NUM_2,
		GPIO_NUM_21,GPIO_NUM_17,
		GPIO_NUM_12,GPIO_NUM_15,
	};

	
	gpio_test_config(gpio_pin_array);
	
	level = 0;
	while(1)
	{
		for(i=0;i<LED_CNT;i++)
		{
			gpio_set_level(gpio_pin_array[i], level);
		}
		level = ~level;
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
}

int wifi_fun_test(void)
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
	
	wifi_sta_mode_init();
	
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
		}
	}
}

void app_main(void)
{
	uint8_t ret = 0;

	uart0_init();
    nvs_flash_init();

    while (true) 
	{
		#if 1
		if(wifi_ok_flag==0)
		{
			ret = wifi_fun_test();
			if(ret)
			{
				uart_write_bytes(UART_NUM_0,(const char*)"wifi_test_ok\r\n",14);
			}
			else{
				uart_write_bytes(UART_NUM_0,(const char*)"wifi_test_nok\r\n",15);
			}
		}
		else if(wifi_ok_flag==1)
		{
			uart_write_bytes(UART_NUM_0,(const char*)"gpio_test_start\r\n",17);
			gpio_test();
		}
		#endif
    }
}

