/*
 * monitor_task.c
 *
 *  Created on: May 10, 2020
 *      Author: Gintaras
 */

#include "monitor_task.h"
#include "main.h"
#include "stdio.h"
#include "string.h"

extern EEPROMStorageTypDef eeprom_info;
osThreadId MonitorTaskHandle;
DevStorageTypDef storage;

void MonitorTask(void const * argument)
{
  osEvent evt;
  uint16_t  local_adc_data[5];
  static uint32_t mon_dayticks = 0;
  static uint32_t daytime_counter= 0;
  static uint32_t nighttime_counter= 0;

  /*Start ADC DMA Process*/
  if(HAL_ADC_Start_DMA(&hadc, (uint32_t *)storage.adc_data, 5) != HAL_OK)
  {
	  Error_Handler();
  }

  /*Start timer*/
  HAL_TIM_Base_Start_IT(&htim21);

  for(;;)
  {
	  /*Wait for signal from timer interrupt*/
	  evt = osSignalWait (0x00000001, osWaitForever);
	  if (evt.status == osEventSignal)
	  {
		  /*Copy all ADC data measured with DMA*/
		  memcpy(local_adc_data, storage.adc_data, sizeof(storage.adc_data));

		  /*Convert&Store Input Voltage*/
		  storage.vinput_mv = (uint32_t)(local_adc_data[2] * VINPUT_CONST);

		  /*Convert&Store Battery Voltage*/
		  storage.vbatt_mv = (uint32_t)(local_adc_data[3] * VBATT_CONST);

		  /*Convert&Store Arduino Input Voltage*/
		  storage.vard_input_mv = (uint32_t)(local_adc_data[4] * VARD_CONST);

		  /*Convert&Store Input Current*/
		  storage.cinput_ma = (int32_t)((local_adc_data[0] - COFFSET_CONST) * CSENSE_CONST);
		  if(storage.cinput_ma < 0)
		  {storage.cinput_ma = 0;}

		  /*Convert&Store Output Current*/
		  storage.coutput_ma = (int32_t)((local_adc_data[1] -COFFSET_CONST) * CSENSE_CONST);
		  if(storage.coutput_ma < 0)
		  {storage.coutput_ma = 0;}

		  /*Convert&Store Energy Accumulated*/
		  storage.energy_stored_mah += (float)(storage.cinput_ma * ETIME_CONST);
		  if(storage.energy_stored_mah > FULL_BATT_MAH)
		  {storage.energy_stored_mah = FULL_BATT_MAH;}

		  /*Convert&Store Energy Released*/
		  storage.energy_released_mah += (float)(storage.coutput_ma * ETIME_CONST);
		  if(storage.energy_released_mah > FULL_BATT_MAH)
		  {storage.energy_released_mah = FULL_BATT_MAH;}

		  /*Convert&Store Total Battery Energy Output*/
		  storage.total_batt_ouput_ah += (float)((storage.coutput_ma * ETIME_CONST)/1000);

		  /*Day time monitoring*/
		  if((int)(storage.vinput_mv+eeprom_info.vin_hys_mv) > eeprom_info.vin_limit_mv)
		  {
			  daytime_counter++;
			  nighttime_counter= 0;
		  }
		  else if((int)(storage.vinput_mv-eeprom_info.vin_hys_mv) < eeprom_info.vin_limit_mv)
		  {
			  daytime_counter= 0;
			  nighttime_counter++;
		  }
		  if(daytime_counter > TIME_INTEG)
		  {
			  storage.daytime_flag = 1;
			  daytime_counter= 0;
		  }
		  if(nighttime_counter > TIME_INTEG)
		  {
			  storage.daytime_flag = 0;
			  nighttime_counter= 0;
		  }

		  /*Do the day length time calculation*/
		  if(storage.daytime_flag)
		  {
			  mon_dayticks++;
			  storage.daylength_s = (uint32_t)(mon_dayticks/10);
			  if(storage.daylength_s > HOURS_24)
			  {storage.daylength_s = HOURS_24;}
		  }
		  else
		  {
			  mon_dayticks = 0;
		  }
	  }
  }
}
