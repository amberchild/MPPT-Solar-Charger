/*
 * indication_task.h
 *
 *  Created on: May 11, 2020
 *      Author: Gintaras
 */

#ifndef INDICATION_TASK_H_
#define INDICATION_TASK_H_

#include "main.h"
#include "cmsis_os.h"

#define IND_OFF		0
#define IND_GREEN	1
#define IND_RED		2

extern osThreadId IndicationTaskHandle;
extern osMessageQId ind_msg;
extern const osMessageQDef_t ind_msg_def;

void ind_green(void);
void ind_red(void);
void ind_off(void);
void IndicationTask(void const * argument);

#endif /* INDICATION_TASK_H_ */
