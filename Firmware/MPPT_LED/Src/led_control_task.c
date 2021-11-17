/*
 * led_control_task.c
 *
 *  Created on: May 10, 2020
 *      Author: Gintaras
 */
#include "led_control_task.h"
#include "main.h"

osThreadId LEDControlTaskHandle;
osMessageQId led_msg;
const osMessageQDef_t led_msg_def =
		{
				.queue_sz = 1,
				.item_sz = 1
		};


void LEDControlTask(void const * argument)
{
  osEvent  evt;

  for(;;)
  {
	  /*Always wait for a message with intensity value*/
	  evt = osMessageGet (led_msg,  osWaitForever);
	  if (evt.status == osEventMessage)
	  {
		  set_intensity(evt.value.v);
	  }
  }
}

/*Hardware Timer Microsecond Delay*/
void delay_us (uint16_t us)
{
	/*Set the counter value a 0*/
	__HAL_TIM_SET_COUNTER(&htim22,0);

	/*Wait for the counter to reach the us input in the parameter*/
	while (__HAL_TIM_GET_COUNTER(&htim22) < us);
}

/*Sets driver current using EasyScale Protocol*/
void set_intensity(uint32_t ref)
{
  unsigned char byte = 0, j = 0, k = 0;
  byte = 0x72;       //device address byte

  /*Shut down driver and enter into EasyScale control mode*/
  HAL_GPIO_WritePin(LED_CTRL_GPIO_Port, LED_CTRL_Pin, GPIO_PIN_SET);
  delay_us(110);
  HAL_GPIO_WritePin(LED_CTRL_GPIO_Port, LED_CTRL_Pin, GPIO_PIN_RESET);
  delay_us(1100);

  /*Send address and data*/
  for(k=2; k>0; k--)
  {
	HAL_GPIO_WritePin(LED_CTRL_GPIO_Port, LED_CTRL_Pin, GPIO_PIN_SET);
	delay_us(48);

    for(j=8; j>0; j--)
    {
      if(byte & 0x80)
      {
    	HAL_GPIO_WritePin(LED_CTRL_GPIO_Port, LED_CTRL_Pin, GPIO_PIN_RESET); //encoding bit 1
    	delay_us(24);
        HAL_GPIO_WritePin(LED_CTRL_GPIO_Port, LED_CTRL_Pin, GPIO_PIN_SET);
        delay_us(48);
      }
      else
      {
    	HAL_GPIO_WritePin(LED_CTRL_GPIO_Port, LED_CTRL_Pin, GPIO_PIN_RESET); //encoding bit 0
    	delay_us(48);
        HAL_GPIO_WritePin(LED_CTRL_GPIO_Port, LED_CTRL_Pin, GPIO_PIN_SET);
        delay_us(24);
      }
      byte += byte; //left shift
    }

    HAL_GPIO_WritePin(LED_CTRL_GPIO_Port, LED_CTRL_Pin, GPIO_PIN_RESET); //End of stream delay
    delay_us(48);

    byte = ref; //read data byte
  }
  HAL_GPIO_WritePin(LED_CTRL_GPIO_Port, LED_CTRL_Pin, GPIO_PIN_SET);
}
