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
const char fcmd_dw_post_p5[]  = "\"5\":{\"command\":\"property.publish\",\"params\":{\"thingKey\":\"%s\",\"key\":\"batt_mv\",\"value\":%d}},";

/*App ID and tokens*/
const char telit_appID[] = "5b542754447cfb36414b9b26";
const char telit_appToken[] = "SFBhqftn43LdjcrF";

/*Global variables for Telit cloud uploads*/
char telit_sessionId[25];
char post_buff[CMD_BUFF_LENGTH];
char post_length[16];
static char local_buff[1024];

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
    HAL_GPIO_WritePin(ON_OFF_GPIO_Port, ON_OFF_Pin, GPIO_PIN_SET);
    osDelay(3000);
    HAL_GPIO_WritePin(ON_OFF_GPIO_Port, ON_OFF_Pin, GPIO_PIN_RESET);
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
    HAL_GPIO_WritePin(ON_OFF_GPIO_Port, ON_OFF_Pin, GPIO_PIN_SET);
    osDelay(4500);
    HAL_GPIO_WritePin(ON_OFF_GPIO_Port, ON_OFF_Pin, GPIO_PIN_RESET);
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

    /*Request network status info*/
    result = SCP_SendCommandWaitAnswer("AT+CEREG?\r\n", "OK", 500, 5);

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

    return ntwrk_stat;
}

/*Returns LTE Context status*/
static int32_t LTE_StatusCheck(void)
{
    char *result = NULL;
    int32_t lte_stat = 0;

    /*Request network status info*/
    result = SCP_SendCommandWaitAnswer("AT+CGACT?\r\n", "OK", 1000, 1);

    if(result)
    {
        result = NULL;
        result = memmem((char*)SCPHandler.RxBuffer, CMD_BUFF_LENGTH, "+CGACT: 1", sizeof("+CGACT: 1")-1);
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

/*Waits for network available, suspends actual task*/
static _Bool WaitForNetwork(void)
{
    int32_t test = 0;
    int signal = 0;

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
        
        /*Indicate network search with LED2*/

    }
    
    /*Turn off LED2*/

    return false;
}

/*Connect LTE, returns true and IP address if succeeded, must have 15 bytes allocated*/
static _Bool LTE_Connect(char *ip_address)
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
      
    /*Check if the module is registered*/
    result = SCP_SendCommandWaitAnswer("AT+CGCONTRDP\r\n", "+CGCONTRDP: 1", 1000, 1);
    if(!result) return false;

    /*Context Activation*/
    result = NULL;
    result = SCP_SendCommandWaitAnswer("AT+CGACT?\r\n", "+CGACT: 1,1", 1000, 1);
    if (!result) result = SCP_SendCommandWaitAnswer("AT+CGACT=1,1\r\n", "OK", 1000, 1);
    osDelay(1000);
    if (!result) return false;
    
    /*IP Address*/
    result = NULL;
    result = SCP_SendCommandWaitAnswer("AT+IPCONFIG\r\n", "+IPCONFIG:", 1000, 1);
    if(result)
    {
            result = NULL;
            result = memmem((char*)SCPHandler.RxBuffer, CMD_BUFF_LENGTH, "+IPCONFIG: ", sizeof("+IPCONFIG: ")-1);
            result += 11;

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

static _Bool LTE_Disconnect(void)
{
  char *result = NULL;
  result = SCP_SendCommandWaitAnswer("AT+CGACT=1,0\r\n", "OK", 1000, 1);
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
    result = SCP_SendCommandWaitAnswer("AT+CGSN=1\r\n", "OK", 100, 1);

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
static _Bool ModemOpenTcpSocket(char *pAddress, uint32_t port, int *socket_id)
{
  char *result = NULL;
  memset(post_buff, 0, sizeof(post_buff));
  
  /*CreateTCP/UDP socket*/
  result = SCP_SendCommandWaitAnswer("AT+ESOC=1,1,1\r\n", "OK", 1000, 1);
  
  /*Read Socket ID*/
  if(result)
  {
    result = NULL;
    result = memmem((char*)SCPHandler.RxBuffer, CMD_BUFF_LENGTH, "+ESOC=", sizeof("+ESOC=")-1);
    if(result)
    {
      result += 6;
      *socket_id = atoi(result);
    }
    else return false;
  }
  else return false;
  
  /* Form "Connect socket to remote address and port" command */
  sprintf(post_buff, "AT+ESOCON=%d,%d,\"%s\"\r\n", *socket_id, (int)port, pAddress);
  result = NULL;
  result = SCP_SendCommandWaitAnswer(post_buff, "OK", 10000, 1);
  if(result)
  {
    return true;
  }
  
  return false;
}

/* Close socket */
static _Bool ModemCloseTcpSocket(int socket_id)
{
    char *result = NULL;
    char buff[15];
    
    /* Close selected socket */
    sprintf(buff, "AT+ESOCL=%d\r\n", socket_id);
    result = SCP_SendCommandWaitAnswer(buff, "OK", 1000, 1);
    if(result)
    {
        return true;
    }

    return false;
}

/*NE310H2 Socket send procedure*/
static char* SocketSend(int socket_id, char *string, unsigned int length)
{
    char *result = NULL;
    unsigned int i;
    char symbol[3];
    static char buffer[2048];
    static char cmd_buffer[2048];

    /*Limitation*/
    if(length > 512)
    {
        return NULL;
    }

    /*Clean buffers to be used*/
    memset(symbol, 0, sizeof(symbol));
    memset(buffer, 0, sizeof(buffer));
    memset(cmd_buffer, 0, sizeof(cmd_buffer));

    /*Convert data to Hex format string*/
    for(i = 0; i < length; i++)
    {
        sprintf(symbol, "%02X", *string);
        strcat(buffer,symbol);
        string++;
    }

    /*Create command to be sent*/
    sprintf(cmd_buffer, "AT+ESOSEND=%d,%d,%s\r\n", socket_id, length, buffer);

    /*attempt to send*/
    result = SCP_SendCommandWaitAnswer(cmd_buffer, "OK", 60000, 1);

    return result;

}

/*NE310H2 Socket receive procedure*/
static char* SocketReceive(int socket_id, int *data_lenght)
{
  char *result = NULL;
  unsigned int i;
  long number;
  char symbol[3];
  
  memset(post_buff, 0, sizeof(post_buff));
  memset(symbol, 0, sizeof(symbol));
  *data_lenght = 0;
  
  /*Wait for Socket message arrived indicator*/
  result = SCP_WaitForAnswer("+ESONMI=", 60000);
  osDelay(1000);
  if(result)
  {
    result += 8;
    if(atoi(result) == socket_id)
    {
      result = strchr(result, ',');
      if(result)
      {
        result++;
        *data_lenght = atoi(result);
        
        result = strchr(result, ',');
        if(result)
        {
          result++;
          
          i = 0;
          while(*result != '\0')
          {
            symbol[0] = *result;
            result++;
            symbol[1] = *result;
            result++;
            
            number = strtol(symbol, (char **)NULL,16);
            post_buff[i] = (char)number;
            
            if(i > CMD_BUFF_LENGTH)
            {
              return NULL;
            }
            i++;
          }
          return post_buff;
        }
        else return NULL;
        
      }
      else return NULL;
      
    }
    else return NULL;
  }
  return NULL;
}

/* Authentication */
static _Bool TelitPortalAuthenticate(void)
{
  char *result = NULL;
  int i = 0;
  int data_length = 0;
  
  memset(post_buff, 0, sizeof(post_buff));
  memset(local_buff, 0, sizeof(local_buff));
  memset(telit_sessionId, 0, sizeof(telit_sessionId));
  
  /*Reset rx buffer for data reception*/
  SCP_InitRx();
  
  /* Form data for lenght calculation*/
  sprintf(local_buff, fcmd_dW_auth, telit_appToken, telit_appID, modem_data.imei);
  
  /* Get data length */
  sprintf(post_length, "%d\r\n\r\n", strlen(local_buff));
  
  /*Generate full HTTP post*/
  sprintf(post_buff, (char *)fcmd_HTTPPOST, strlen(fcmd_HTTPPOST));
  strcat(post_buff,post_length);
  strcat(post_buff,local_buff);
  
  /* Send HTTP POST data */
  result = SocketSend(modem_data.socket_id, post_buff, strlen(post_buff));
  
  /*Receive data and extract the Session ID*/
  if(result)
  {
    result = NULL;
    result = SocketReceive(modem_data.socket_id, &data_length);
    if(result)
    {
      result = strstr(result, "sessionId\":\"");
      if(result)
      {
        result += strlen("sessionId\":\"");
        
        i=0;
        while ((*result != '\"')&& (i < SESSIONID_LENGTH))
        {
          telit_sessionId[i++]=*(result++);
          
        }
        if (i <= SESSIONID_LENGTH)
        {
          ModemCloseTcpSocket(modem_data.socket_id);
          return true;
        }
      }
    }
  }
  return false;
}

/*Post*/
static _Bool TelitPortalPostData(void)
{
  char *result = NULL;
  uint32_t len;
  int data_length = 0;
  
  memset(post_buff, 0, sizeof(post_buff));
  memset(local_buff, 0, sizeof(local_buff));
  
  /*Reset rx buffer for data reception*/
  SCP_InitRx();
  
    /* Calculate post length */
    len =  strlen(fcmd_dw_post_auth) - 2;
    len += strlen(fcmd_dw_post_p1) - 4;
    len += strlen(fcmd_dw_post_p2) - 4;
    len += strlen(fcmd_dw_post_p3) - 4;
    len += strlen(fcmd_dw_post_p4) - 4;
    len += strlen(fcmd_dw_post_p5) - 4;
    
    /* Calculate parameters length */
    sprintf(
            (char *)post_buff,"%s%s%s%s%s%s%d%d%d%d%d",
            telit_sessionId,
            modem_data.imei,
            modem_data.imei,
            modem_data.imei,
            modem_data.imei,
            modem_data.imei,
			(int)storage.energy_stored_mah,
			(int)storage.energy_released_mah,
			(int)modem_data.day_lenght_store,
			(int)storage.total_batt_ouput_ah,
			(int)storage.vbatt_mv
              );

    len += strlen((char *)post_buff);
    
    /*Generate full HTTP post*/
    memset(post_buff, 0, sizeof(post_buff));
    sprintf(post_buff, (char *)fcmd_HTTPPOST, strlen(fcmd_HTTPPOST));
    sprintf((char *)local_buff,"%d\r\n\r\n", (int)len);
    strcat(post_buff,local_buff);
    
    memset(local_buff, 0, sizeof(local_buff));
    sprintf((char *)local_buff, fcmd_dw_post_auth, telit_sessionId);
    strcat(post_buff,local_buff);
    
    memset(local_buff, 0, sizeof(local_buff));
    sprintf((char *)local_buff, fcmd_dw_post_p1, modem_data.imei, (int)storage.energy_stored_mah);
    strcat(post_buff,local_buff);
    
    memset(local_buff, 0, sizeof(local_buff));
    sprintf((char *)local_buff, fcmd_dw_post_p2, modem_data.imei, (int)storage.energy_released_mah);
    strcat(post_buff,local_buff);
    
    memset(local_buff, 0, sizeof(local_buff));
    sprintf((char *)local_buff, fcmd_dw_post_p3, modem_data.imei, (int)modem_data.day_lenght_store);
    strcat(post_buff,local_buff);
    
    memset(local_buff, 0, sizeof(local_buff));
    sprintf((char *)local_buff, fcmd_dw_post_p4, modem_data.imei, (int)storage.total_batt_ouput_ah);
    strcat(post_buff,local_buff);
    
    memset(local_buff, 0, sizeof(local_buff));
    sprintf((char *)local_buff, fcmd_dw_post_p5, modem_data.imei, (int)storage.vbatt_mv);
    strcat(post_buff,local_buff);
    
    /* Send HTTP POST data in two peaces*/
    len = strlen(post_buff)/2;
    result = SocketSend(modem_data.socket_id, post_buff, len);
    if(result)
    {
      result = SocketSend(modem_data.socket_id, &post_buff[len], strlen(post_buff) - len);
    }
    
    /*Receive data*/
    if(result)
    {
      result = NULL;
      result = SocketReceive(modem_data.socket_id, &data_length);
      if(result)
      {
        result = strstr(result, "}}");
        if(result)
        {
          ModemCloseTcpSocket(modem_data.socket_id);
          return true;
        }
      }
    }
    return false;
  
  /*Timeout. In case of error, no }} received*/
  ModemCloseTcpSocket(modem_data.socket_id);
  return false;
}

upload_error_t TelitCloudUpload(void)
{
  static _Bool authenticated = false;
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
  modem_data.lte_pwr_status = true;
  
  /*Check PIN*/
  if (scp_result) scp_result = SCP_SendCommandWaitAnswer("AT+CPIN?\r\n", "+CPIN: READY", 2000, 1);
  
  /*Echo commands turn off*/
  if (scp_result) scp_result = SCP_SendCommandWaitAnswer("ATE0\r\n", "OK", 2000, 1);
  
  /*Sets LED Output mode*/
  if (scp_result) scp_result = SCP_SendCommandWaitAnswer("AT#TCONTLED?\r\n", "#TCONTLED: 1", 1000, 1);
  if (!scp_result) scp_result = SCP_SendCommandWaitAnswer("AT#TCONTLED=1\r\n", "OK", 2000, 1);
  
  /*Sets Error Report*/
  if (scp_result) scp_result = SCP_SendCommandWaitAnswer("AT+CMEE=2\r\n", "OK", 2000, 1);
  
  osDelay(1000);
  
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
  if (scp_result) scp_result = SCP_SendCommandWaitAnswer("AT*MCGDEFCONT?\r\n", "lpwa.telia.iot", 1000, 1);
  if (!scp_result) scp_result = SCP_SendCommandWaitAnswer("AT*MCGDEFCONT=,\"IP\",\"lpwa.telia.iot\"\r\n", "OK", 2000, 1);
  
  if (scp_result)
  {
    /*Check if we are connected to LTE network*/
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
    
    /*Check LTE status*/
    modem_data.lte_status = LTE_StatusCheck();
    
    /*Try to connect if not connected*/
    if(!modem_data.lte_status)
    {
      modem_data.lte_status = LTE_Connect(modem_data.ip_address);
      if(!modem_data.lte_status)
      {
        LTE_Disconnect();
        return_error = MODEM_NO_DATA_SERVICE;
        goto error_exit;
      }
    } 
  }
  else
  {
    modem_data.lte_status = 0;
    authenticated = false;
    return_error = CLOUD_AUTH_ERROR;
    goto error_exit;
  }
  
  /*Start uploading*/
  
  /*Authenticate only once per session*/
  if(!authenticated)
  {
    /*Authenticate */
  Authentication:
    result = ModemOpenTcpSocket("54.93.92.219", 80, &modem_data.socket_id);
    if(result)
    {
      authenticated = TelitPortalAuthenticate();
    }
    /*Post*/
    if(authenticated)
    {
      result = ModemOpenTcpSocket("54.93.92.219", 80, &modem_data.socket_id);
      if (result)
      {
        authenticated = TelitPortalPostData();
      }
    }
    if(!result)
    {
      LTE_Disconnect();
      goto error_exit;
    }
  }
  else
  {
    /*Post*/
    result = ModemOpenTcpSocket("54.93.92.219", 80, &modem_data.socket_id);
    if (result)
    {
      authenticated = TelitPortalPostData();
    }
    /*If post fails try to authenticate*/
    else
    {
      authenticated = false;
      goto Authentication;
    }
  }
  
  /*Turn off LED1*/

  /*Turn off Modem*/

  /*Stop Timer*/
  HAL_TIM_Base_Stop_IT(&htim7);

  if(!ModemOff(&storage.adc_data[4]))
  {
    return_error = MODEM_POWER_OFF_FAIL;
    modem_data.lte_pwr_status = false;
    return return_error;
  }
  modem_data.lte_pwr_status = false;
  return return_error;
  
error_exit:
  /*Turn off LED1*/

  /*Turn off Modem*/
/*Stop Timer*/
HAL_TIM_Base_Stop_IT(&htim7);
  if(!ModemOff(&storage.adc_data[4]))
  {
    return_error = MODEM_POWER_OFF_FAIL;
    modem_data.lte_pwr_status = false;
    return return_error;
  }
  modem_data.lte_pwr_status = false;
  return return_error;
}
