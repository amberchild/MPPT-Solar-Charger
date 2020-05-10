/*
 * led_control_task.h
 *
 *  Created on: May 10, 2020
 *      Author: Gintaras
 */

#ifndef LED_CONTROL_TASK_H_
#define LED_CONTROL_TASK_H_

#include "cmsis_os.h"

extern osThreadId LEDControlTaskHandle;
extern osMessageQId led_msg;
extern const osMessageQDef_t led_msg_def;

void delay_us (uint16_t us);
void set_intensity(uint32_t ref);
void LEDControlTask(void const * argument);


#endif /* LED_CONTROL_TASK_H_ */
