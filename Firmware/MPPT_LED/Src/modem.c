/*
 * modem.c
 *
 *  Created on: 13 Nov 2018
 *      Author: GDR
 */

#include "modem.h"
#include "main.h"
#include "cmsis_os.h"
#include "StringCommandParser.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "math.h"

/*Modem global variables storage*/
modem_data_storage_t modem_data;

/*AT parser variables storage*/
extern TSCPHandler   SCPHandler;

extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim7;

extern uint8_t aRxBuffer;

extern DevStorageTypDef storage;

/*Telit Portal Authentication Templates*/
const char fcmd_HTTPPOST[] ="POST /api HTTP/1.0\r\nHost: api-de.devicewise.com\r\nContent-Type: application/json\r\nContent-Length:";
const char fcmd_dW_auth[]  = "{\"auth\":{\"command\":\"api.authenticate\",\"params\":{\"appToken\":\"%s\",\"appId\":\"%s\",\"thingKey\":\"%s\"}}}\r\n";
const char fcmd_dw_post_auth[]  = "{\"auth\":{\"sessionId\":\"%s\"},";

/*Telit Portal Post Templates*/
const char fcmd_dw_post_p1[]  = "\"1\":{\"command\":\"property.publish\",\"params\":{\"thingKey\":\"%s\",\"key\":\"e_stored\",\"value\":%d}},";
const char fcmd_dw_post_p2[]  = "\"2\":{\"command\":\"property.publish\",\"params\":{\"thingKey\":\"%s\",\"key\":\"e_consumed\",\"value\":%d}},";
const char fcmd_dw_post_p3[]  = "\"3\":{\"command\":\"property.publish\",\"params\":{\"thingKey\":\"%s\",\"key\":\"d_len\",\"value\":%d}},";
const char fcmd_dw_post_p4[]  = "\"4\":{\"command\":\"property.publish\",\"params\":{\"thingKey\":\"%s\",\"key\":\"t_batt_out\",\"value\":%d}},";
const char fcmd_dw_post_p5[]  = "\"5\":{\"command\":\"property.publish\",\"params\":{\"thingKey\":\"%s\",\"key\":\"batt_mv\",\"value\":%d}}}";

/*App ID and tokens*/
const char telit_appID[] = "5b542754447cfb36414b9b26";
const char telit_appToken[] = "SFBhqftn43LdjcrF";

/*Global variables for Telit cloud uploads*/
char telit_sessionId[48];
char post_buff[CMD_BUFF_LENGTH];
char post_length[16];

void *memmem(const void *l, size_t l_len, const void *s, size_t s_len)
{
	const char *cur, *last;
	const char *cl = l;
	const char *cs = s;

	/* a zero length needle should just return the haystack */
	if (s_len == 0)
		return (void *)cl;

	/* "s" must be smaller or equal to "l" */
	if (l_len < s_len)
		return NULL;

	/* special case where s_len == 1 */
	if (s_len == 1)
		return memchr(l, *cs, l_len);

	/* the last position where its possible to find "s" in "l" */
	last = cl + l_len - s_len;

	for (cur = cl; cur <= last; cur++)
		if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
			return (void *)cur;

	return NULL;
}

void SCP_Tick_Callback(void)
{
	SCP_Tick(10);
}

static inline _Bool ModemOn(uint16_t *pwr_level)
{
  /*Turn on power for LTE Modem*/
  if(*pwr_level < PWRONLVL)
  {
	HAL_GPIO_WritePin(LDO_OFF_GPIO_Port, LDO_OFF_Pin, GPIO_PIN_RESET);
	osDelay(3000);
    if(*pwr_level >= PWRONLVL)
    {
      osDelay(1000);
      return true;
    }
    else
    {
      return false;
    }
  }
  else
  {
    return true;
  }
}

static inline _Bool ModemOff(uint16_t *pwr_level)
{
  /*Turn off power for LTE Modem*/
  if(*pwr_level >= PWRONLVL)
  {
    HAL_GPIO_WritePin(LDO_OFF_GPIO_Port, LDO_OFF_Pin, GPIO_PIN_SET);
    osDelay(3000);
    if(*pwr_level < PWRONLVL)
    {
      return true;
    }
    else
    {
      return false;
    }
  }
  else
  {
    return true;
  }
}

uint32_t uart_send_buff(uint8_t *data_out, uint32_t size)
{
	return HAL_UART_Transmit_DMA(&huart1, data_out, (uint16_t)size);
}

uint32_t uart_read_byte(uint8_t *pData)
{
	return HAL_UART_Receive_DMA(&huart1, pData, 1);
}

/*Returns network registration status*/
static int32_t NetworkRegistrationCheck(void)
{
    char *result = NULL;
    int32_t ntwrk_stat = 0;
    uint8_t i;

    /*Check what type of network is currently in use*/
    result = SCP_SendCommandWaitAnswer("AT+COPS?\r\n", "OK", 500, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    /*We have response, lets look for the info in the receiver buffer*/
    if(result)
    {
        result = NULL;
        result = memmem((char*)SCPHandler.RxBuffer, CMD_BUFF_LENGTH, "+COPS:", sizeof("+COPS:")-1);

        /*Response found, lets look for the third (,) */
        for(i = 0; i < 3; i++)
        {
        	result = strchr(result, ',');
            if(!result)
            {
            	/*Something is wrong with a data?*/
            	return 0;
            }

            result++;
        }

        /*Read the result*/
        ntwrk_stat = atoi(result);
    }

    /*GSM Network*/
    if(ntwrk_stat == 0)
    {
    	result = NULL;

        /*Request network status info*/
        result = SCP_SendCommandWaitAnswer("AT+CREG?\r\n", "OK", 500, 5);
        vTaskDelay(pdMS_TO_TICKS(100));

        if(result)
        {
            result = NULL;
            result = memmem((char*)SCPHandler.RxBuffer, CMD_BUFF_LENGTH, "+CREG: ", sizeof("+CREG: ")-1);
            if(result)
            {
                result += 9;
                ntwrk_stat = atoi(result);
            }
        }
        else
        {
            return 0;
        }

        return ntwrk_stat;
    }

    /*LTE Cat-M / NB-IoT*/
    else if(ntwrk_stat == 8 || ntwrk_stat == 9)
    {
    	/*Request network status info*/
        result = SCP_SendCommandWaitAnswer("AT+CEREG?\r\n", "OK", 500, 5);
        vTaskDelay(pdMS_TO_TICKS(100));

        if(result)
        {
            result = NULL;
            result = memmem((char*)SCPHandler.RxBuffer, CMD_BUFF_LENGTH, "+CEREG: ", sizeof("+CEREG: ")-1);
            if(result)
            {
                result += 10;
                ntwrk_stat = atoi(result);
            }
        }
        else
        {
            return 0;
        }
    }

    return ntwrk_stat;
}

/*Returns Context status*/
static int32_t ContextStatusCheck(void)
{
    char *result = NULL;
    int32_t lte_stat = 0;

    /*Request network status info*/
    result = SCP_SendCommandWaitAnswer("AT#SGACT?\r\n", "OK", 1000, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    if(result)
    {
        result = NULL;
        result = memmem((char*)SCPHandler.RxBuffer, CMD_BUFF_LENGTH, "#SGACT: 1", sizeof("#SGACT: 1")-1);
        if(result)
        {
            result += 10;
            lte_stat = atoi(result);
        }
    }
    else
    {
        return 0;
    }

    return lte_stat;
}

/*Returns received signal quality */
static int32_t SignalQuality(void)
{
    int32_t signal_level = 0;
    char *result = NULL;

    /*Request RSSI*/
    result = SCP_SendCommandWaitAnswer("AT+CSQ\r\n", "OK", 2000, 1);

    if(result)
    {
        result = NULL;
        result = memmem((char*)SCPHandler.RxBuffer, CMD_BUFF_LENGTH, "+CSQ:", sizeof("+CSQ:")-1);
        if(result)
        {
            result += 6;
            signal_level = atoi(result);
        }
    }
    else
    {
        return 0;
    }

    return signal_level;
}

/*Sets "STATUS" LED Output*/
static char* SetLED(_Bool state)
{
    char *result = NULL;
    char led_buff[16];

    memset(led_buff, 0, sizeof(led_buff));
    sprintf(led_buff, "AT#GPIO=8,%d,1\r\n",state);

    /*Send LED Set command*/
    result = SCP_SendCommandWaitAnswer(led_buff, "OK", 2000, 1);

    return result;
}

/*Waits for network available, suspends actual task*/
static _Bool WaitForNetwork(void)
{
    int32_t test = 0;
    int signal = 0;
    _Bool led = false;

    /*Wait for network available*/
    for(uint32_t i=0; i < WAIT_FOR_NETWORK_S; i++)
    {
        /*Get current network state*/
        test = NetworkRegistrationCheck();

        /*registered, home network  or  registered, roaming is acceptable*/
        if((test == 1) || (test == 5))
        {
          /*Check the signal level*/
          signal = SignalQuality();
          if(signal != 99)
          {
        	SetLED(false);
            return true;
          }
          else
          {
            /* Wait 1000ms */
            osDelay(1000);
          }
        }
        else
        {
            /* Wait 1000ms */
            osDelay(1000);
        }
        
        /*Indicate network search with LED*/
        led = !led;
        SetLED(led);
    }
    
    /*Turn off LED*/
    SetLED(false);

    return false;
}

/*Context Activation, returns true and IP address if succeeded, must have 15 bytes allocated*/
static _Bool ContextActivation(char *ip_address)
{
    char *result = NULL;
    char *temp = NULL;
    int check = 0;

    temp = ip_address;

    /*Check if the module is registered.*/
    check = NetworkRegistrationCheck();
    if(!check)
    {
      WaitForNetwork();
    }

    /*Context Activation*/
    result = NULL;
    result = SCP_SendCommandWaitAnswer("AT#SGACT?\r\n", "#SGACT: 1,0", 1000, 1);
    if (!result) result = SCP_SendCommandWaitAnswer("AT#SGACT=1,0\r\n", "OK", 1000, 1);
    if (!result) return false;

    vTaskDelay(pdMS_TO_TICKS(1000));

    result = NULL;
    result = SCP_SendCommandWaitAnswer("AT#SGACT=1,1\r\n", "OK", 60000, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    if(result)
    {
            result = NULL;
            result = memmem((char*)SCPHandler.RxBuffer, CMD_BUFF_LENGTH, "#SGACT: ", sizeof("#SGACT: ")-1);
            result += 8;

            if(result)
            {
                memset(ip_address, 0x00, 15);

                /*Maximum 15 chars for IP address*/
                for(uint8_t i = 0; i < 15; i++)
                {
                    /*Skip the tags*/
                    if(*result == '"')
                    {
                        result++;
                    }

                    /*Not a number or dot in IP address shall be treated as error*/
                    if(!(*result > 47 && *result < 58) && !(*result == '.') && (!(*result == '\r')))
                    {
                        memset(ip_address, 0x00, 15);
                        return false;
                    }

                    /*The end of the string*/
                    if(*result == '\r')
                    {
                        break;
                    }

                    *temp = *result;
                    result++;
                    temp++;
                }
            }
            return true;
    }
    return false;
}

static _Bool ContextDeactivation(void)
{
	char *result = NULL;
	result = SCP_SendCommandWaitAnswer("AT#SGACT=1,0\r\n", "OK", 1000, 1);
	if(result)
	{
		return true;
	}
	else return false;
}

/*Returns pointer to operator string*/
static char* GetOperator(void)
{
    char *result = NULL;
    static char operator[17];

    /*Request operator*/
    result = SCP_SendCommandWaitAnswer("AT+COPS?\r\n", "OK", 30000, 1);

    /*We have response, lets look for the info in the receiver buffer*/
    if(result)
    {
        result = NULL;
        result = memmem((char*)SCPHandler.RxBuffer, CMD_BUFF_LENGTH, "+COPS:", sizeof("+COPS:")-1);

        /*Response found, lets look for operator string, begins with (") */
        if(result)
        {
            result = strchr(result, '"');

            /*Copy operator to the RAM and return*/
            if(result)
            {
                /*Clean static buffer*/
                memset(operator, 0x00, 17);

                /*Maximum 16 chars for operator initials allowed*/
                for(uint8_t i = 0; i < 16; i++)
                {
                    operator[i] = *result;
                    result++;

                    /*Last operator char?*/
                    if(*result == '"')
                    {
                        i++;
                        operator[i] = *result;
                        return operator;
                    }
                }

                return operator;
            }
        }
    }

    return NULL;
}

/*Returns pointer to IMEI string of 15 numbers*/
static char* GetIMEI(void)
{
    char *result = NULL;
    static char imei[16];
    _Bool isDigit = false;
    uint32_t j = 0, i=0;

    /*Request IMEI*/
    result = SCP_SendCommandWaitAnswer("AT+CGSN\r\n", "OK", 100, 1);

    /*We have response, lets look for the info in the receiver buffer*/
    if(result)
    {
        result = NULL;

        /*Lets look for a ASCII number...*/
        while((j < strlen((char*)SCPHandler.RxBuffer)) && (!isDigit))
        {
          if((SCPHandler.RxBuffer[j] > 47) && (SCPHandler.RxBuffer[j] < 58))
          {
              isDigit = true;
              result = (char*)&SCPHandler.RxBuffer[j];
              break;
          }

          j++;
        }

        /*First number of IMEI found, copy the number to the RAM and return */
        if(result)
        {
            memset(imei, 0x00, 16);

            /*Maximum 15 chars for IMEI is allowed*/
            for(i = 0; i < 15; i++)
            {
                /*Not a number in IMEI shall be treated as error*/
                if(!(*result > 47 && *result < 58))
                {
                    return NULL;
                }

                imei[i] = *result;

                result++;
            }

            return imei;
        }
    }

    return NULL;
}

/*Request model identification*/
static char* GetID(void)
{
    char *result = NULL;
    static char device_id[21];

    /*Request model identification*/
    result = SCP_SendCommandWaitAnswer("AT+CGMM\r\n", "OK", 100, 1);

    /*We have response, lets look for the info in the receiver buffer*/
    if(result)
    {
        result = NULL;
        /*Lets look for a (\n) char as the begining of device name*/
        result = strchr((char*)SCPHandler.RxBuffer, '\n');

        if(result)
        {
            result++;

            /*Copy operator to the RAM and return*/
            memset(device_id, 0x00, 21);

            /*Maximum 20 chars for model identification allowed*/
            for(uint8_t i = 0; i < 20; i++)
            {
                device_id[i] = *result;
                result++;

                /*Device_id end*/
                if(*result == '\r')
                {
                    return device_id;
                }
            }
        }

    }

    return NULL;
}

/*Returns pointer to firmware version*/
static char* GetVersion(void)
{
    char *result = NULL;
    static char version[16];
    uint32_t i=0;

    /*Request IMEI*/
    result = SCP_SendCommandWaitAnswer("AT+CGMR\r\n", "OK", 100, 1);

    /*We have response, lets look for the info in the receiver buffer*/
    if(result)
    {
        result = NULL;

        /*Lets look for a (\n) char as the begining of firmware version string*/
        result = strchr((char*)SCPHandler.RxBuffer, '\n');

        /*Copy string to RAM */
        if(result)
        {
            result++;
            memset(version, 0x00, 16);

            /*Maximum 15 chars limit*/
            for(i = 0; i < 15; i++)
            {

                version[i] = *result;
                result++;

                if(*result == '\r')
                {
                    break;
                }
            }

            return version;
        }
    }

    return NULL;
}

/*Open & Connect TCP/IP Socket*/
static _Bool ModemOpenTcpSocket(char *pAddress, uint32_t port)
{
    char *result = NULL;
    memset(post_buff, 0, sizeof(post_buff));
    /* Form open socket command */
    sprintf(post_buff, "AT#SD=1,0,%d,\"%s\"\r", (int)port, pAddress);
    result = SCP_SendCommandWaitAnswer(post_buff, "CONNECT", 15000, 1);

    if(result)
    {
        return true;
    }

    return false;
}

/* Close socket */
static _Bool ModemCloseTcpSocket(void)
{
    char *result = NULL;
    /* Form close socket command */
    (void) SCP_SendCommandWaitAnswer("+++\r\n", "OK", 1000, 1);

    result = SCP_SendCommandWaitAnswer("AT#SH=1\r\n", "OK", 1000, 1);

    if(result)
    {
        return true;
    }

    return false;
}

/*Telit portal authentication procedure */
static _Bool TelitPortalAuthenticate()
{
    char *result = NULL;
    int i = 0;
    char local_buff[1024];

    memset(post_buff, 0, sizeof(post_buff));
    memset(local_buff, 0, sizeof(local_buff));
    memset(post_length, 0, sizeof(post_length));
    memset(telit_sessionId, 0, sizeof(telit_sessionId));

    /* Form data for lenght calculation*/
    sprintf(local_buff, fcmd_dW_auth, telit_appToken, telit_appID, modem_data.imei);

    /* Get data length */
    sprintf(post_length, "%d\r\n\r\n", strlen(local_buff));

    /*Generate full HTTP post*/
    sprintf(post_buff, (char *)fcmd_HTTPPOST);
    strcat(post_buff,post_length);
    strcat(post_buff,local_buff);
    strcat(post_buff,"\r\n");

    /*Reset rx buffer for data reception*/
    SCP_InitRx();

    /* Send HTTP POST data */
    SCP_SendData(post_buff, strlen(post_buff));

    /* Wait for full answer */
    result = SCP_WaitForAnswer("}}}", 60000);
    if (result)
    {
        /* Getting session id */
        result = NULL;
        result = strstr((char*)SCPHandler.RxBuffer, "sessionId\":\"");
        if(result)
        {
            result += strlen("sessionId\":\"");
            while ((*result != '\"')&& (*result != 0))
            {
                telit_sessionId[i++]=*(result++);
            }
            /* We should wait for the server to shut down the connection */
            result = NULL;
            result = SCP_WaitForAnswer("NO CARRIER", 30000);
            if(result)
            {
                ModemCloseTcpSocket();
                return true;
            }
        }
    }
    ModemCloseTcpSocket();
    return false;
}

/*Post data to Telit cloud*/
static _Bool TelitPortalPostData()
{
	char *result = NULL;
	char local_buff[1024];
  
	memset(post_buff, 0, sizeof(post_buff));
	memset(local_buff, 0, sizeof(local_buff));
	memset(post_length, 0, sizeof(post_length));
  
	/*Reset rx buffer for data reception*/
	SCP_InitRx();
  
	/* Generate the JSON Object */
	memset(post_buff, 0, sizeof(post_buff));
	sprintf((char *)post_buff, fcmd_dw_post_auth, telit_sessionId);
	strcat(local_buff,post_buff);

	memset(post_buff, 0, sizeof(post_buff));
	sprintf((char *)post_buff, fcmd_dw_post_p1, modem_data.imei, (int)storage.energy_stored_mah);
	strcat(local_buff,post_buff);

	memset(post_buff, 0, sizeof(post_buff));
	sprintf((char *)post_buff, fcmd_dw_post_p2, modem_data.imei, (int)storage.energy_released_mah);
	strcat(local_buff,post_buff);

	memset(post_buff, 0, sizeof(post_buff));
	sprintf((char *)post_buff, fcmd_dw_post_p3, modem_data.imei, (int)modem_data.day_lenght_store);
	strcat(local_buff,post_buff);

	memset(post_buff, 0, sizeof(post_buff));
	sprintf((char *)post_buff, fcmd_dw_post_p4, modem_data.imei, (int)storage.total_batt_ouput_ah);
	strcat(local_buff,post_buff);

	memset(post_buff, 0, sizeof(post_buff));
	sprintf((char *)post_buff, fcmd_dw_post_p5, modem_data.imei, (int)storage.vbatt_mv);
	strcat(local_buff,post_buff);

	/*Generate HTTP post*/
	memset(post_buff, 0, sizeof(post_buff));
	sprintf(post_buff, (char *)fcmd_HTTPPOST);
	sprintf(post_length, "%d\r\n\r\n", strlen(local_buff));
	strcat(post_buff,post_length);
	strcat(post_buff,local_buff);
	strcat(post_buff,"\r\n");

	/* Send HTTP POST data */
	SCP_SendData((char *)post_buff, strlen(post_buff));
	vTaskDelay(pdMS_TO_TICKS(1000));

	/*Look for the ending of the JSON object*/
	result = SCP_WaitForAnswer("}}", 60000);
	if (result)
	{
		/* We should wait for the server to shut down the connection */
		result = NULL;
		result = SCP_WaitForAnswer("NO CARRIER", 30000);
		if(result)
		{
			ModemCloseTcpSocket();
			return true;
		}
	}

	/*Timeout. In case of error, no }} received*/
	ModemCloseTcpSocket();
	return false;
}

upload_error_t TelitCloudUpload(void)
{
  _Bool result = false;  
  char * scp_result = NULL;
  upload_error_t return_error = UPLOAD_OK;
  
  /*Apply power for IoT LTE module*/
  if(!ModemOn(&storage.adc_data[4]))
  {
    return_error = MODEM_POWER_ON_FAIL;
    goto error_exit;
  }
  
  /*Start Timer and USART Receive with DMA*/
  HAL_TIM_Base_Start_IT(&htim7);

  /*Start UART DMA Receive process everytime to avoid errors on modem start up*/
  HAL_UART_Receive_DMA(&huart1, &aRxBuffer, 1);

  /*Start AT Commands*/
  scp_result = SCP_SendCommandWaitAnswer("AT\r\n", "OK", 200, 1);
  
  /*No response to AT, Retry*/
  if(!scp_result)
  {
    osDelay(5000);
    HAL_UART_Receive_DMA(&huart1, &aRxBuffer, 1);
    scp_result = NULL;
    scp_result = SCP_SendCommandWaitAnswer("AT\r\n", "OK", 200, 1);
    if(!scp_result)
    {
      return_error = MODEM_CMD_NO_RESPONSE;
      goto error_exit;
    }
  }

  /*Modem is ON*/
  if (scp_result) modem_data.modem_power_en = true;
  
  /*Check PIN*/
  if (scp_result) scp_result = SCP_SendCommandWaitAnswer("AT+CPIN?\r\n", "+CPIN: READY", 2000, 1);
  
  /*Echo commands turn off*/
  if (scp_result) scp_result = SCP_SendCommandWaitAnswer("ATE0\r\n", "OK", 2000, 1);
  
  /*Sets Error Report*/
  if (scp_result) scp_result = SCP_SendCommandWaitAnswer("AT+CMEE=2\r\n", "OK", 2000, 1);
  
  osDelay(5000);

  /*Read modem properties*/
  if(scp_result)
  {
    /*Get IMEI*/
    scp_result = NULL;
    scp_result = GetIMEI();
    if(scp_result)
    {
      memset(modem_data.imei, 0x00, 16);
      strcpy(modem_data.imei, scp_result);
    }
    
    /*Get module type*/
    scp_result = NULL;
    scp_result = GetID();
    if(scp_result)
    {
      memset(modem_data.device_name, 0x00, 21);
      strcpy(modem_data.device_name, scp_result);
    }
    
    /*Get firmware version*/
    scp_result = NULL;
    scp_result = GetVersion();
    if(scp_result)
    {
      memset(modem_data.fw_version, 0x00, 16);
      strcpy(modem_data.fw_version, scp_result);
    }
  }
  
  if(!scp_result)
  {
    return_error = MODEM_CMD_NO_RESPONSE;
    goto error_exit;
  }
  
  /*Set APN*/
  if (scp_result) scp_result = SCP_SendCommandWaitAnswer("AT+CGDCONT?\r\n", "omnitel", 1000, 1);
  if (!scp_result) scp_result = SCP_SendCommandWaitAnswer("AT+CGDCONT=1,\"IP\",\"omnitel\"\r\n", "OK", 2000, 1);

  /*Network Support Setup*/
  if (scp_result) scp_result = SCP_SendCommandWaitAnswer("AT+WS46?\r\n", "+WS46: 28", 1000, 1);
  if (!scp_result) scp_result = SCP_SendCommandWaitAnswer("AT+WS46=28\r\n", "OK", 1000, 1);
  if (scp_result)
  {
    scp_result = SCP_SendCommandWaitAnswer("AT#WS46=2\r\n", "OK", 1000, 1);
    if(!scp_result)
    {
      return_error = MODEM_NET_SELECT_FAIL;
      goto error_exit;
    }
  }

  /*Check the network status*/
  if (scp_result)
  {
    /*Check if we detect the network*/
    if(!WaitForNetwork())
    {
      return_error = MODEM_NO_OPERATOR_PRESENT;
      goto error_exit;
    }
  }
  else
  {
    return_error = MODEM_CMD_NO_RESPONSE;
    goto error_exit;
  }
  
  osDelay(5000);

  /*Store signal quality*/
  modem_data.signal = SignalQuality();
  
  /*Network status check*/
  modem_data.network_status =  NetworkRegistrationCheck();
  
  /*registered to home network or registered as roaming is acceptable*/
  if((modem_data.network_status == 1) || (modem_data.network_status == 5))
  {
    /*Store operator*/
    scp_result = NULL;
    scp_result = GetOperator();
    if(scp_result)
    {
      memset(modem_data.operator, 0x00, 17);
      strcpy(modem_data.operator, scp_result);
    }
    else
    {
      return_error = MODEM_NO_OPERATOR_PRESENT;
      goto error_exit;
    }
    
    /*Check Data Context Status*/
    modem_data.context = ContextStatusCheck();
    
    /*Try to connect if not connected*/
    if(!modem_data.context)
    {
      modem_data.context = ContextActivation(modem_data.ip_address);
      if(!modem_data.context)
      {
        ContextDeactivation();
        return_error = MODEM_NO_DATA_SERVICE;
        goto error_exit;
      }
    } 
  }
  else
  {
    modem_data.context = 0;
    return_error = MODEM_NOT_REGISTERED;
    goto error_exit;
  }
  
  /*Start uploading*/
  SetLED(true);
  result = ModemOpenTcpSocket("api-de.devicewise.com", 80);
  osDelay(1000);
  if(result)
  {
	result = TelitPortalAuthenticate();
    if(result)
    {
  	  result = ModemOpenTcpSocket("api-de.devicewise.com", 80);
  	  osDelay(1000);
      if (result)
      {
    	result = TelitPortalPostData();
        if(!result)
        {
          return_error = CLOUD_POST_ERROR;
          goto error_exit;
        }
      }
      else
      {
    	return_error = CLOUD_OPEN_SOCKET_ERROR;
    	goto error_exit;
      }
    }
    else
    {
    	return_error = CLOUD_AUTH_ERROR;
    	goto error_exit;
    }
  }
  else
  {
	return_error = CLOUD_OPEN_SOCKET_ERROR;
	goto error_exit;
  }
  
  /*Turn off LED*/
  SetLED(false);

  /*Stop Timer*/
  HAL_TIM_Base_Stop_IT(&htim7);

  /*Turn off Modem*/
  if(!ModemOff(&storage.adc_data[4]))
  {
    return_error = MODEM_POWER_OFF_FAIL;
    modem_data.modem_power_en = false;
    return return_error;
  }
  modem_data.modem_power_en = false;
  return return_error;
  
error_exit:
  /*Turn off LED*/
  SetLED(false);

  /*Turn off Modem*/
/*Stop Timer*/
HAL_TIM_Base_Stop_IT(&htim7);
  if(!ModemOff(&storage.adc_data[4]))
  {
    return_error = MODEM_POWER_OFF_FAIL;
    modem_data.modem_power_en = false;
    return return_error;
  }
  modem_data.modem_power_en = false;
  return return_error;
}
