/*
 * monitor_task.h
 *
 *  Created on: May 10, 2020
 *      Author: Gintaras
 */

#ifndef MONITOR_TASK_H_
#define MONITOR_TASK_H_

#include "main.h"
#include "cmsis_os.h"

#define VINPUT_CONST

extern osThreadId MonitorTaskHandle;
extern osSemaphoreId MonSemaphore;
extern const osSemaphoreDef_t MonSemaphoreDef;
extern DevStorageTypDef storage;

void MonitorTask(void const * argument);


#endif /* MONITOR_TASK_H_ */
