/*
 * modem.h
 *
 *  Created on: 13 Nov 2018
 *      Author: GDR
 */

#include "stdlib.h"
#include "stdint.h"

#ifndef INC_MODEM_H_
#define INC_MODEM_H_

#define true				1
#define false				0
#define CMD_BUFF_LENGTH                 1024
#define SESSIONID_LENGTH                24
#define PWRONLVL                        3500    //power on indication ADC value
#define WAIT_FOR_NETWORK_S              900     //wait for network in seconds
#define ADC_VOLTAGE_COEFF               1.611328

typedef struct
{
    _Bool   modem_power_en;
    int network_status;
    int context;
    int socket_id;
    int signal;
    char operator[17];
    char imei[16];
    char ip_address[16];
    char device_name[21];
    char fw_version[16];
    uint32_t day_lenght_store;
} modem_data_storage_t;

typedef enum upload_errors
{
  UPLOAD_OK=0, 
  MODEM_POWER_ON_FAIL,
  MODEM_POWER_OFF_FAIL,
  MODEM_CMD_NO_RESPONSE,
  CLOUD_AUTH_ERROR,
  CLOUD_POST_ERROR,
  CLOUD_OPEN_SOCKET_ERROR,
  MODEM_NET_SELECT_FAIL,
  MODEM_NO_OPERATOR_PRESENT,
  MODEM_NO_NETWORK,
  MODEM_NOT_REGISTERED,
  MODEM_NO_DATA_SERVICE,
    
} upload_error_t;

void SCP_Tick_Callback(void);
void *memmem(const void *l, size_t l_len, const void *s, size_t s_len);
upload_error_t TelitCloudUpload(void);
#endif /* INC_MODEM_H_ */
