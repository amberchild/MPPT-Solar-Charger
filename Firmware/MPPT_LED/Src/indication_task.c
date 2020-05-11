/*
 * indication_task.c
 *
 *  Created on: May 11, 2020
 *      Author: Gintaras
 */

#include "indication_task.h"
#include "main.h"

osThreadId IndicationTaskHandle;
osMessageQId ind_msg;
const osMessageQDef_t ind_msg_def =
		{
				.queue_sz = 1,
				.item_sz = 1
		};

void IndicationTask(void const * argument)
{
	osEvent  evt;

	for(;;)
	{
		  /*Always wait for a message with intensity value*/
		  evt = osMessageGet (ind_msg,  osWaitForever);
		  if (evt.status == osEventMessage)
		  {
			  switch(evt.value.v)
			  {
			  	  case IND_OFF:
			  	  {
			  		  ind_off();
			  		  break;
			  	  }
			  	  case IND_GREEN:
			  	  {
			  		  ind_green();
			  		  break;
			  	  }
			  	  case IND_RED:
			  	  {
			  		  ind_red();
			  		  break;
			  	  }
			  	  default:
			  	  {
			  		  ind_off();
			  	  }
			  }
		  }
	}

}

void ind_green(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);

	/*Configure GPIO pin:*/
	GPIO_InitStruct.Pin = LED_IND_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void ind_red(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);

	/*Configure GPIO pin:*/
	GPIO_InitStruct.Pin = LED_IND_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void ind_off(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/*Configure GPIO pin:*/
	GPIO_InitStruct.Pin = LED_IND_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}
