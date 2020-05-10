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

osThreadId MonitorTaskHandle;
DevStorageTypDef storage;

void MonitorTask(void const * argument)
{
  osEvent evt;
  uint16_t  local_adc_data[5];
  static uint32_t mon_dayticks = 0;

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
		  HAL_GPIO_TogglePin(LED_IND_GPIO_Port, LED_IND_Pin);

		  /*Copy all ADC data measured with DMA*/
		  memcpy(local_adc_data, storage.adc_data, sizeof(storage.adc_data));

		  /*Convert&Store Input Voltage*/
	  }
  }
}
