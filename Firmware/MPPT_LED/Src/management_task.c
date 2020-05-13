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
	static _Bool battery_charged = 0;
	static int32_t max_idle_current = 0;

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
				osDelay(5000);
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

			/*Load the battery with LEDs*/
			if(battery_charged)
			{
				load_setup(FULL_BATT_MAH, HOURS_24 - storage.daylength_s);
				storage.energy_released_mah = 0;
			}
			else
			{
				storage.energy_stored_mah = storage.energy_stored_mah - storage.energy_released_mah;
				storage.energy_released_mah = 0;
				if(storage.energy_stored_mah > 0)
				{
					load_setup(storage.energy_stored_mah, HOURS_24 - storage.daylength_s);
				}
			}

			/*Discharge battery with LEDs*/
			osMessagePut(ind_msg, IND_RED, osWaitForever);
			while(1)
			{
				osDelay(1000);

				/*Day time?*/
				if(storage.vinput_mv+VINPUT_HYS > VINPUT_LIMIT)
				{
					break;
				}

				/*Low battery?*/
				if(storage.vbatt_mv < BATT_LOW_MV)
				{
					break;
				}

				/*Out of energy?*/
				if(storage.energy_stored_mah - storage.energy_released_mah < 0)
				{
					break;
				}
			}

			/*End discharge process*/
			battery_charged = 0;
			storage.daylength_s = 0;
			osMessagePut(led_msg, 0, osWaitForever);
			osMessagePut(ind_msg, IND_OFF, osWaitForever);
		}

		/*Energy bleed check in idle state*/
		if(battery_charged)
		{
			if(storage.coutput_ma > max_idle_current)
			{
				max_idle_current = storage.coutput_ma;
			}
			if(max_idle_current > IDLE_CURR_MA)
			{
				max_idle_current = 0;
				battery_charged = 0;
			}
			if(storage.energy_released_mah > IDLE_CURR_MAH)
			{
				battery_charged = 0;
			}
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

uint32_t load_setup(uint32_t capacity, uint32_t nightitme)
{
	uint32_t intensity;
	uint32_t mAseconds;

	/*Full load if time is too short*/
	if(nightitme < MIN_NIGHT_DUR)
	{
		intensity = 31;
		osMessagePut(led_msg, intensity, osWaitForever);
		return intensity;
	}

	/*Decrease capacity to have more realistic results*/
	capacity = capacity * DRIVER_EFF;

	/*Convert capacity to mAs*/
	mAseconds = capacity*3600;

	/*Look for load to have LEDs operational over night time*/
	for(intensity = 0; intensity < 32; intensity++)
	{
		osMessagePut(led_msg, intensity, osWaitForever);
		osDelay(300);
		if(storage.coutput_ma*nightitme > mAseconds)
		{
			osMessagePut(led_msg, intensity-1, osWaitForever);
			return intensity;
		}
	}

	return intensity;
}
