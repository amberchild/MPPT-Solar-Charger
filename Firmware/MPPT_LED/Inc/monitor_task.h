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

#define VINPUT_LIMIT	8000
#define VINPUT_CONST	7.371
#define VBATT_CONST		4.000
#define VARD_CONST		0.806
#define CSENSE_CONST	1.943
#define COFFSET_CONST	65
#define ETIME_CONST		0.0000277778

extern osThreadId MonitorTaskHandle;
extern osSemaphoreId MonSemaphore;
extern const osSemaphoreDef_t MonSemaphoreDef;
extern DevStorageTypDef storage;

void MonitorTask(void const * argument);


#endif /* MONITOR_TASK_H_ */
