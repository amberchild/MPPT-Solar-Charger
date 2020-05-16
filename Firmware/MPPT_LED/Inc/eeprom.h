/*
 * eeprom.h
 *
 *  Created on: May 15, 2020
 *      Author: GDR
 */

#ifndef EEPROM_H_
#define EEPROM_H_

#include "main.h"

#define EEPROM_BASE_ADDRESS 0x08080000UL
#define EEPROM_SIZE			6144
#define EEPROM_LAST_ADDR	((EEPROM_BASE_ADDRESS + EEPROM_SIZE) - 1)

HAL_StatusTypeDef writeEEPROMByte(uint32_t address, uint8_t data);
HAL_StatusTypeDef writeEEPROMData(uint32_t address, uint8_t* data, uint16_t len);

#endif /* EEPROM_H_ */
