/*
 * eeprom.c
 *
 *  Created on: May 15, 2020
 *      Author: GDR
 */

#include "eeprom.h"

/**
 * writeEEPROMByte allows to write a byte(uint8_t) to the internal eeprom
 * @param   address  starts at 0, the max size depends on the uc type
 * @param   data     byte (uint8_t)
 * @return  status   internal HAL_Status
 */
HAL_StatusTypeDef writeEEPROMByte(uint32_t address, uint8_t data) {
  HAL_StatusTypeDef  status;
  address = address + EEPROM_BASE_ADDRESS;
  HAL_FLASHEx_DATAEEPROM_Unlock();  //Unprotect the EEPROM to allow writing
  status = HAL_FLASHEx_DATAEEPROM_Program(TYPEPROGRAMDATA_BYTE, address, data);
  HAL_FLASHEx_DATAEEPROM_Lock();  // Reprotect the EEPROM
  return status;
  }

HAL_StatusTypeDef writeEEPROMData(uint32_t address, uint8_t* data, uint16_t len)
  {
	HAL_StatusTypeDef  status;
	uint16_t i;

	address = address + EEPROM_BASE_ADDRESS;

	if(address+len > EEPROM_LAST_ADDR)
	{return HAL_ERROR;}

	HAL_FLASHEx_DATAEEPROM_Unlock();  //Unprotect the EEPROM to allow writing
	for(i = 0; i < len; i++)
	{
		status = HAL_FLASHEx_DATAEEPROM_Program(TYPEPROGRAMDATA_BYTE, address, *data);
		data++;
		address++;
		if(status != HAL_OK )
		{
			HAL_FLASHEx_DATAEEPROM_Lock();  // Reprotect the EEPROM
			return status;
		}
	}

	HAL_FLASHEx_DATAEEPROM_Lock();  // Reprotect the EEPROM
	return HAL_OK;
  }
