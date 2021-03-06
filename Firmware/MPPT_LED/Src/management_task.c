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
#include "modem.h"
#include "StringCommandParser.h"

osThreadId ManagementTaskHandle;

void ManagementTask(void const * argument)
{
	ch_state_t ch_status = UNKNOWN;
	static _Bool battery_charged = 0;
	static _Bool discharge_lock = 0;
	static int32_t max_idle_current = 0;
	uint8_t i;
	upload_error_t sts = UPLOAD_OK;

	for(;;)
	{
		osDelay(1000);

		/*Check if it is a day time*/
		if(storage.daytime_flag)
		{
			/*If input is more than MPPT, enable charger*/
			if(storage.vinput_mv+100 > MPPT_MV)
			{
				charger_enable();
				osDelay(5000);
				ch_status = charger_status();

				/*Stay in here until fully charged or day is over:*/
				/*the charger should always indicate charge process termination*/
				while(ch_status == IN_PROGRESS)
				{
					osDelay(1000);
					discharge_lock = 0;
					battery_charged = 0;
					osMessagePut(ind_msg, IND_RED, osWaitForever);
					ch_status = charger_status();
					if(!storage.daytime_flag)/*Fail-safe*/
					{break;}
				}

				/*If battery is full, then it must have it's declared capacity (assumption)*/
				if(ch_status == COMPLETED)
				{
					battery_charged = 1;
					discharge_lock = 0;
					osMessagePut(ind_msg, IND_GREEN, osWaitForever);
					storage.energy_stored_mah = FULL_BATT_MAH;
				}
				else
				{
					osMessagePut(ind_msg, IND_OFF, osWaitForever);
				}

			}
			/*If input is less than MPPT, disable charger*/
			else if(storage.vinput_mv-100 < MPPT_MV)
			{
				charger_disable();
				osMessagePut(ind_msg, IND_OFF, osWaitForever);
			}

		}
		/*It must be a night time*/
		else
		{
			charger_disable();
			osMessagePut(ind_msg, IND_OFF, osWaitForever);
			osMessagePut(led_msg, 0, osWaitForever);

			if(!discharge_lock)
			{
				/*Fully load the battery if charging time is too short */
				if(storage.daylength_s < MIN_DAY_DUR)
				{
					storage.led_level = load_setup(FULL_BATT_MAH, 0);
				}
				/*Load the battery with LEDs*/
				else if(battery_charged)
				{
					storage.led_level = load_setup(FULL_BATT_MAH, HOURS_24 - storage.daylength_s);
				}
				else
				{
					storage.energy_stored_mah -= storage.energy_released_mah;
					storage.energy_released_mah = 0;
					if(storage.energy_stored_mah > 0)
					{
						storage.led_level = load_setup(storage.energy_stored_mah, HOURS_24 - storage.daylength_s);
					}
					else
					{
						storage.energy_stored_mah = 0;
					}
				}

				/*Discharge battery with LEDs*/
				osMessagePut(ind_msg, IND_RED, osWaitForever);
				modem_data.day_lenght_store = storage.daylength_s;
				while(1)
				{
					osDelay(1000);

					/*Day time?*/
					if(storage.daytime_flag)
					{
						break;
					}

					/*Low battery?*/
					if(storage.vbatt_mv < BATT_LOW_MV)
					{
						discharge_lock = 1;
						break;
					}

					/*Out of energy check*/
					if(battery_charged)
					{
						if(eeprom_info.batt_full_mah - storage.energy_released_mah <= 0)
						{
							discharge_lock = 1;
							break;
						}
					}
					else
					{
						if(storage.daylength_s > MIN_DAY_DUR)
						{
							if(storage.energy_stored_mah - storage.energy_released_mah <= 0)
							{
								discharge_lock = 1;
								break;
							}
						}
					}
				}

				/*End discharge process*/
				osMessagePut(led_msg, 0, osWaitForever);
				osMessagePut(ind_msg, IND_OFF, osWaitForever);
				eeprom_info.total_batt_ouput_ah = storage.total_batt_ouput_ah;
				eeprom_save(&eeprom_info);
				for(i = 0; i < 5; i++)
				{
					sts = TelitCloudUpload();
					if(sts == UPLOAD_OK)
					{break;}
					if(sts == MODEM_NO_OPERATOR_PRESENT)
					{break;}
				}
				battery_charged = 0;
				storage.daylength_s = 0;
				storage.energy_stored_mah = storage.energy_stored_mah - storage.energy_released_mah;
				if(storage.energy_stored_mah < 0)
				{storage.energy_stored_mah = 0;}
				storage.energy_released_mah = 0;

			}
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
		{stat1 = 0;}
	else
		{stat1 = 1;}

	/*Read STAT2 pin*/
	if(HAL_GPIO_ReadPin(STAT2_GPIO_Port, STAT2_Pin) == GPIO_PIN_RESET)
		{stat2 = 0;}
	else
		{stat2 = 1;}

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

	/*Battery balancer decreases the capacity measurements?*/
	capacity = capacity * BATT_EFF;

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

	return intensity-1;
}
