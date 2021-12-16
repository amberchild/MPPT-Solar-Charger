/*
 * management_task.h
 *
 *  Created on: May 12, 2020
 *      Author: Gintaras
 */

#ifndef MANAGEMENT_TASK_H_
#define MANAGEMENT_TASK_H_

#include "main.h"
#include "cmsis_os.h"
#include "modem.h"

#define COEFF_K			1.16
#define COEFF_B			4000

extern EEPROMStorageTypDef eeprom_info;
extern osThreadId ManagementTaskHandle;
extern modem_data_storage_t modem_data;

typedef enum
{
	INACTIVE = -1,
	COMPLETED = 0,
	IN_PROGRESS,
	UNKNOWN
}ch_state_t;

void charger_enable(void);
void charger_disable(void);
ch_state_t charger_status(void);
uint32_t load_setup(uint32_t capacity, uint32_t nightitme);
void ManagementTask(void const * argument);

#endif /* MANAGEMENT_TASK_H_ */
