/*
 * management_task.c
 *
 *  Created on: May 12, 2020
 *      Author: Gintaras
 */

#include "management_task.h"
#include "led_control_task.h"
#include "monitor_task.h"
#include "indication_task.h"

osThreadId ManagementTaskHandle;

void ManagementTask(void const * argument)
{
	ch_state_t ch_status = UNKNOWN;
	_Bool battery_charged = 0;

	for(;;)
	{
		osDelay(1000);

		/*Check if it is a day time*/
		if(storage.vinput_mv+VINPUT_HYS > VINPUT_LIMIT)
		{
			/*If input is more than MPPT, enable charger*/
			if(storage.vinput_mv+VINPUT_HYS > MPPT_MV)
			{
				charger_enable();
				osDelay(1000);
				ch_status = charger_status();

				while(ch_status == IN_PROGRESS)
				{
					osDelay(1000);
					osMessagePut(ind_msg, IND_RED, osWaitForever);
					ch_status = charger_status();
				}

				if(ch_status == COMPLETED)
				{
					battery_charged = 1;
					osMessagePut(ind_msg, IND_GREEN, osWaitForever);
				}
				else
				{
					osMessagePut(ind_msg, IND_OFF, osWaitForever);
					if(storage.vbatt_mv < FULL_BATT_MV)
					{
						battery_charged = 0;
					}
				}

			}
			/*If input is less than MPPT, disable charger*/
			else if(storage.vinput_mv-VINPUT_HYS < MPPT_MV)
			{
				charger_disable();
				osMessagePut(ind_msg, IND_OFF, osWaitForever);
			}

		}
		/*Check if it is a night time*/
		else if(storage.vinput_mv-VINPUT_HYS < VINPUT_LIMIT)
		{
			charger_disable();
			osMessagePut(ind_msg, IND_OFF, osWaitForever);
		}
	}
}

void charger_enable(void)
{
	HAL_GPIO_WritePin(CHR_CTRL_GPIO_Port, CHR_CTRL_Pin, GPIO_PIN_RESET);
}
void charger_disable(void)
{
	HAL_GPIO_WritePin(CHR_CTRL_GPIO_Port, CHR_CTRL_Pin, GPIO_PIN_SET);
}

ch_state_t charger_status(void)
{
	_Bool stat1;
	_Bool stat2;

	/*Read STAT1 pin*/
	if(HAL_GPIO_ReadPin(STAT1_GPIO_Port, STAT1_Pin) == GPIO_PIN_RESET)
		{stat1 = 1;}
	else
		{stat1 = 0;}

	/*Read STAT2 pin*/
	if(HAL_GPIO_ReadPin(STAT2_GPIO_Port, STAT2_Pin) == GPIO_PIN_RESET)
		{stat2 = 1;}
	else
		{stat2 = 0;}

	/*Decode logic*/
	if(stat1 && !stat2)
	{
		return IN_PROGRESS;
	}
	if(!stat1 && stat2)
	{
		return COMPLETED;
	}
	if(!stat1 && !stat2)
	{
		return INACTIVE;
	}

	return UNKNOWN;
}

void load_setup(uint32_t capacity, uint32_t nightitme)
{

}
