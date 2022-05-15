/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
extern ADC_HandleTypeDef hadc;
extern TIM_HandleTypeDef htim21;
extern TIM_HandleTypeDef htim22;

typedef struct
{
	uint16_t  adc_data[5];
	uint32_t vinput_mv;
	uint32_t vbatt_mv;
	uint32_t vard_input_mv;
	int32_t cinput_ma;
	int32_t coutput_ma;
	double energy_stored_mah;
	double energy_released_mah;
	double total_batt_ouput_ah;
	uint32_t daylength_s;
	uint8_t led_level;
	_Bool daytime_flag;

}DevStorageTypDef;

typedef struct
{
	double total_batt_ouput_ah;
	uint16_t vin_limit_mv;
	uint16_t vin_hys_mv;
	uint16_t batt_full_mv;
	uint16_t batt_full_mah;
	uint16_t batt_low_mv;
	uint32_t crc;

}EEPROMStorageTypDef;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void eeprom_ram_init(EEPROMStorageTypDef *eeprom);
void eeprom_save(EEPROMStorageTypDef *eeprom);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ISENS1_Pin GPIO_PIN_0
#define ISENS1_GPIO_Port GPIOA
#define ISENS2_Pin GPIO_PIN_1
#define ISENS2_GPIO_Port GPIOA
#define VINPUT_Pin GPIO_PIN_2
#define VINPUT_GPIO_Port GPIOA
#define VBAT_Pin GPIO_PIN_3
#define VBAT_GPIO_Port GPIOA
#define VAUX_Pin GPIO_PIN_4
#define VAUX_GPIO_Port GPIOA
#define WAKE_Pin GPIO_PIN_5
#define WAKE_GPIO_Port GPIOA
#define LDO_OFF_Pin GPIO_PIN_6
#define LDO_OFF_GPIO_Port GPIOA
#define RESET_Pin GPIO_PIN_7
#define RESET_GPIO_Port GPIOA
#define LDO_OK_Pin GPIO_PIN_0
#define LDO_OK_GPIO_Port GPIOB
#define LED_CTRL_Pin GPIO_PIN_1
#define LED_CTRL_GPIO_Port GPIOB
#define LED_IND_Pin GPIO_PIN_8
#define LED_IND_GPIO_Port GPIOA
#define VMON_CLK_Pin GPIO_PIN_15
#define VMON_CLK_GPIO_Port GPIOA
#define CHR_CTRL_Pin GPIO_PIN_3
#define CHR_CTRL_GPIO_Port GPIOB
#define STAT2_Pin GPIO_PIN_4
#define STAT2_GPIO_Port GPIOB
#define STAT1_Pin GPIO_PIN_5
#define STAT1_GPIO_Port GPIOB
void   MX_GPIO_Init(void);
void   MX_DMA_Init(void);
void   MX_ADC_Init(void);
void   MX_TIM22_Init(void);
void   MX_TIM21_Init(void);
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
