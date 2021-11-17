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

static inline void ModemReset(void)
{
    /*Turn on Reset MOSFET*/
	HAL_GPIO_WritePin(RESET_GPIO_Port, RESET_Pin, GPIO_PIN_SET);

    /*Keep it on for a 200ms as required*/
	osDelay(200);

    /*Turn off Reset MOSFET*/
	HAL_GPIO_WritePin(RESET_GPIO_Port, RESET_Pin, GPIO_PIN_RESET);

    /*wait for a 1000ms as required*/
	osDelay(1000);
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
    result = SCP_SendCommandWaitAnswer("AT+CREG?\r\n", "OK", 500, 5);

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

/*Returns Context status*/
static int32_t ContextStatusCheck(void)
{
    char *result = NULL;
    int32_t context = 0;

    /*Request network status info*/
    result = SCP_SendCommandWaitAnswer("AT+CGACT?\r\n", "OK", 1000, 1);

    if(result)
    {
        result = NULL;
        result = memmem((char*)SCPHandler.RxBuffer, CMD_BUFF_LENGTH, "+CGACT: 1", sizeof("+CGACT: 1")-1);
        if(result)
        {
            result += 11;
            context = atoi(result);
        }
    }
    else
    {
        return 0;
    }

    return context;
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

/*Sets LED Output*/
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

/*Enable data context, returns true and IP address if succeeded, must have 15 bytes allocated*/
static _Bool ContextConnect(char *ip_address)
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
      
    /*Check if the module is GPRS attached*/
    result = SCP_SendCommandWaitAnswer("AT+CGATT?\r\n", "+CGATT: 1", 1000, 1);
    if(!result) return false;

    /*Activate PDP context identified by <cid>=1.*/
    result = NULL;
    result = SCP_SendCommandWaitAnswer("AT+CGACT=1,1\r\n", "OK", 1000, 1);
    
    /* Wait a few seconds */
    osDelay(2000);

    if(result)
    {
        /*Get the IP address assigned to the module by the network*/
        result = SCP_SendCommandWaitAnswer("AT+CGPADDR=1\r\n", "OK", 1000, 1);

        if(result)
        {
            result = NULL;
            result = memmem((char*)SCPHandler.RxBuffer, CMD_BUFF_LENGTH, "+CGPADDR: 1, ", sizeof("+CGPADDR: 1, ")-1);
            result += 13;

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
    }

    return false;
}

static _Bool ContextDisconnect(void)
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
static _Bool ModemOpenTcpSocket(char *pAddress, uint32_t port, int *socket_id)
{
    char *result = NULL;
    char buff[35];

    memset(buff, 0, sizeof(buff));

    /* Create a socket and return socket id */
    sprintf(buff, "AT+ETL=1,0,0,\"%s\",%d\r\n", pAddress, (int)port);
    result = SCP_SendCommandWaitAnswer(buff, "OK", 10000, 1);

    if(result)
    {
        result = NULL;
        result = memmem((char*)SCPHandler.RxBuffer, CMD_BUFF_LENGTH, "+ETL:", sizeof("+ETL:")-1);
        if(result)
        {
            result += 6;
            *socket_id = atoi(result);
            return true;
        }
    }

    return false;
}

/* Close socket */
static _Bool ModemCloseTcpSocket(int socket_id)
{
    char *result = NULL;
    char buff[15];

    memset(buff, 0, sizeof(buff));

    /* Close selected socket */
    sprintf(buff, "AT+ETL=0,%d\r\n", socket_id);
    result = SCP_SendCommandWaitAnswer(buff, "OK", 2000, 1);

    if(result)
    {
      return true;
    }

     return false;
}

/*GL865 V4 Socket send procedure*/
static char* SocketSend(int socket_id, char *string, unsigned int length)
{
    char *result = NULL;
    unsigned int i;
    char symbol[3];
    static char buffer[1024];
    static char cmd_buffer[1048];

    /*GL865 V4 Limitation*/
    if(strlen(string) > 512)
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
    sprintf(cmd_buffer, "AT+EIPSEND=%d,\"%s\"\r\n", socket_id, buffer);

    /*attempt to send*/
    result = SCP_SendCommandWaitAnswer(cmd_buffer, "OK", 60000, 1);

    return result;
}

/*GL865 V4 Socket receive procedure*/
static char* SocketReceive(int socket_id)
{
    char *result = NULL;
    unsigned int i;
    long number;
    char symbol[3];

    memset(post_buff, 0, sizeof(post_buff));
    memset(symbol, 0, sizeof(symbol));

    /*Create command to for socket reception*/
    sprintf(post_buff, "AT+EIPRECV=%d\r\n", socket_id);

    /*attempt to receive*/
    result = SCP_SendCommandWaitAnswer(post_buff, "OK", 60000, 1);

    /*Data received, lets convert to ASCII*/
    if(result)
    {
        memset(post_buff, 0, sizeof(post_buff));
        result = NULL;
        result = memmem((char*)SCPHandler.RxBuffer, CMD_BUFF_LENGTH, "+EIPRECV: ", sizeof("+EIPRECV: ")-1);
        if(result)
        {
            result += 13;
            i = 0;
            while(*result != '"')
            {
                symbol[0] = *result;
                result++;
                symbol[1] = *result;
                result++;

                number = strtol(symbol, (char **)NULL,16);
                post_buff[i] = (char)number;

                if(i > 1023)
                {
                    return NULL;
                }
                i++;

            }
        }

        return post_buff;
    }

    return NULL;
}

/*Telit portal authentication procedure */
static _Bool TelitPortalAuthenticate(int socket_id)
{
    char *result = NULL;
    memset(post_buff, 0, sizeof(post_buff));
    memset(telit_sessionId, 0, sizeof(telit_sessionId));
    memset(post_length, 0, sizeof(post_length));
    int i;

    /*Reset rx buffer for data reception*/
    SCP_InitRx();

    // form data to be posted
    sprintf(post_buff, fcmd_dW_auth, telit_appToken, telit_appID, modem_data.imei);

    // form data length
    sprintf(post_length, "%d\r\n\r\n", strlen(post_buff)-2);

    // send http post header
    result = SocketSend(socket_id, (char *)fcmd_HTTPPOST, strlen(fcmd_HTTPPOST));

    // send data length
    if(result) result = SocketSend(socket_id, post_length, strlen(post_length));

    // send post data
    if(result) result = SocketSend(socket_id, post_buff, strlen(post_buff));

    if(!result)
    {return false;}

    /* Wait for URC */
    result = SCP_WaitForAnswer("READY RECV\r\n", 20000);
    if (result)
    {
        result = SocketReceive(modem_data.socket_id);
        if(result)
        {
            result = strstr(result, "sessionId\":\"");
            if(result)
            {
                result += strlen("sessionId\":\"");

                i=0;
                while ((*result != '\"')&& (*result != 0))
                {
                    telit_sessionId[i++]=*(result++);

                }

                memset(post_buff, 0, sizeof(post_buff));
                result = NULL;
                sprintf(post_buff, "+ESOCK: %d CLOSE", socket_id);
                for(i = 5; i > 0; i--)
                {
                  osDelay(1000);
                  result = strstr((char*)SCPHandler.RxBuffer, post_buff);
                  if(result)
                  {
                    return true;
                  }
                }
                ModemCloseTcpSocket(modem_data.socket_id);
                return true;
            }
        }
    }

    ModemCloseTcpSocket(modem_data.socket_id);
    return false;
}

/*Post*/
static _Bool TelitPortalPostData(int socket_id)
{
  char *result = NULL;
  uint32_t len;
  
  memset(post_buff, 0, sizeof(post_buff));
  
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
    
    // form the buffer with post header
    result = SocketSend(socket_id, (char *)fcmd_HTTPPOST, strlen(fcmd_HTTPPOST));

    sprintf((char *)post_buff,"%d\r\n\r\n", (int)len);
    if(result) result = SocketSend(socket_id, post_buff, strlen(post_buff));

    sprintf((char *)post_buff, fcmd_dw_post_auth, telit_sessionId);
    if(result) result = SocketSend(socket_id, post_buff, strlen(post_buff));

    // post first line
    sprintf((char *)post_buff, fcmd_dw_post_p1, modem_data.imei, (int)storage.energy_stored_mah);
    if(result) result = SocketSend(socket_id, post_buff, strlen(post_buff));

    // post second line
    sprintf((char *)post_buff, fcmd_dw_post_p2, modem_data.imei, (int)storage.energy_released_mah);
    if(result) result = SocketSend(socket_id, post_buff, strlen(post_buff));

    // post third line
    sprintf((char *)post_buff, fcmd_dw_post_p3, modem_data.imei, (int)modem_data.day_lenght_store);
    if(result) result = SocketSend(socket_id, post_buff, strlen(post_buff));

    // post fourth line
    sprintf((char *)post_buff, fcmd_dw_post_p4, modem_data.imei, (int)storage.total_batt_ouput_ah);
    if(result) result = SocketSend(socket_id, post_buff, strlen(post_buff));

    // post fifth line
    sprintf((char *)post_buff, fcmd_dw_post_p5, modem_data.imei, (int)storage.vbatt_mv);
    if(result) result = SocketSend(socket_id, post_buff, strlen(post_buff));

    if(result) result = SocketSend(socket_id, "\r\n", strlen("\r\n"));

    if(!result)
    {return false;}
    
    /* Wait for URC */
    result = SCP_WaitForAnswer("READY RECV\r\n", 20000);
    
    if (result)
    {
        result = SocketReceive(modem_data.socket_id);
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
  
  /*Timeout. In case of error, no }} received*/
  ModemCloseTcpSocket(modem_data.socket_id);
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
  
  /*Reset the Modem*/
  ModemReset();

  /*Wait for modem to indicate power on*/
  scp_result =SCP_WaitForAnswer("+EIND: 1", 10000);

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
  
  /*Exit sleep mode if no hardware flow control is used*/
  if (scp_result)  scp_result = SCP_SendCommandWaitAnswer("AT+ESLP=0\r\n", "OK", 2000, 1);

  osDelay(5000);

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
  //if (scp_result) scp_result = SCP_SendCommandWaitAnswer("AT+EGDCONT=0,\"IP\",\"lpwa.telia.iot\"\r", "OK", 2000, 1);
  if (scp_result) scp_result = SCP_SendCommandWaitAnswer("AT+EGDCONT=0,\"IP\",\"omnitel\"\r", "OK", 2000, 1);

  
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
      modem_data.context = ContextConnect(modem_data.ip_address);
      if(!modem_data.context)
      {
        ContextDisconnect();
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
  result = ModemOpenTcpSocket("54.93.92.219", 80, &modem_data.socket_id);
  osDelay(1000);
  if(result)
  {
	result = TelitPortalAuthenticate(modem_data.socket_id);
    if(result)
    {
  	  result = ModemOpenTcpSocket("54.93.92.219", 80, &modem_data.socket_id);
  	  osDelay(1000);
      if (result)
      {
    	result = TelitPortalPostData(modem_data.socket_id);
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
