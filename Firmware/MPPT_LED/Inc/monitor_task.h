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

#define MPPT_MV			17500
#define VINPUT_LIMIT	8000
#define VINPUT_HYS		1000
#define VINPUT_CONST	7.371
#define FULL_BATT_MV	12600
#define FULL_BATT_MAH	12800
#define BATT_LOW_MV		9000
#define VBATT_CONST		4.000
#define VARD_CONST		0.806
#define CSENSE_CONST	1.943
#define COFFSET_CONST	65
#define ETIME_CONST		0.0000277778
#define DRIVER_EFF		0.85
#define HOURS_24		86400
#define IDLE_CURR_MA	20
#define IDLE_CURR_MAH	120
#define MIN_DAY_DUR		3600

extern osThreadId MonitorTaskHandle;
extern DevStorageTypDef storage;

void MonitorTask(void const * argument);


#endif /* MONITOR_TASK_H_ */
