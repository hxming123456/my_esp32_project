/*
 * wireless_logic.c
 *
 *  Created on: 2017-3-29
 *      Author: Administrator
 */
#include "wireless_logic.h"
#include <SI_EFM8BB1_Register_Enums.h>
#include "wireless_global.h"
#include "EFM8BB1_FlashPrimitives.h"
#include "wireless_led.h"

sbit WIRELESS_PIN = P0^0;
sbit TEST_KEY     = P0^3;

static uint32_t  wireless_keyval = 0;
//static uint8_t index_num = 4;
static uint8_t count = 0;
DevDef  data dev = {0};

static void setDataPin(uint8_t index,bool value)
{
	if(index == 0)
	{
		DATA0 = value;
	}
	else if(index == 1)
	{
		DATA1 = value;
	}
	else if(index == 2)
	{
		DATA2 = value;
	}
	else if(index == 3)
	{
		DATA3 = value;
	}
}
static void wirelessStudy(void)
{
	static bool study_flag = false;
	static uint32_t last_val = 0;
	static uint8_t index = 0;
	uint8_t i = 0;
	if(!study_flag)
	{
		study_flag = true;
		last_val = wireless_keyval;
		wireless_keyval = 0;
	}
	else
	{
		if(study_flag)
		{
			if(last_val == wireless_keyval)
			{
				for(i = 0; i < 4; i++)
				{
					if(dev.channel_value[i] == wireless_keyval)
					{
						setDataPin(i,true);
						goto _ret;
					}
				}
				/*Ð´½øflash*/
				dev.channel_value[index] = wireless_keyval;
				setDataPin(index,true);
				index++;
				if(index >= 4)
				{
					index = 0;
				}
				FLASH_PageErase(FLASH_LAST);
				for(i = 0; i < 4; i++)
				{
					FLASH_ByteWrite(FLASH_LAST + i,dev.channel_value[i]);
				}
				ledTog300ms();
			}
		}
	}
_ret:
	count = 0;
	study_flag = false;
	last_val = 0;
	wireless_keyval = 0;
	dev.work_mode = DEVICE_WORK_NORMAL;
}

static void wirelessAction(void)
{
	uint8_t i = 0;
	for(i = 0; i < 4;i++)
	{
		dev.cnt[i]++;
		if((dev.channel_value[i] == wireless_keyval ||  TEST_KEY == 1 )
					&& wireless_keyval != 0)
		{
			dev.up_flag[i] = true;
			setDataPin(i,true);
			dev.cnt[i] = 0;
			ledTog10ms();
		}
		else
		{
			if(dev.up_flag[i] == true)
			{
				if(dev.cnt[i] == 20)
				{
					dev.cnt[i] = 0;
					dev.up_flag[i] = false;
					setDataPin(i,false);
				}
			}
		}
	}
}
void wirelessScan(void)
{
	if(dev.work_mode == DEVICE_WORK_STUDY && wireless_keyval != 0)
	{
		wirelessStudy();
		return;
	}
	wirelessAction();
}
void wirelessInit(void)
{
	uint8_t i = 0;
	for(i = 0; i < 4; i++)
	{
		dev.channel_value[i] = FLASH_ByteRead(FLASH_LAST + i);
	}
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
        if(WIRELESS_PIN == 0)
        {
            low_cnt++;
        }
        if(WIRELESS_PIN == 1)
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

        if(WIRELESS_PIN == 1)
        {
            high_cnt++;
        }
        else
        {

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
void decode(void)
{
    uint32_t decode_data = 0;
    decode_data = decodeWireless();
    if(decode_data == 0)
	{
		return;
	}
	wireless_keyval = decode_data;
}
