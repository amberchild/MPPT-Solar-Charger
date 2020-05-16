/*
 * scp.h
 *
 *  Created on: 2018-10-09
 *      Author: KJU
 */

#include "stdint.h"
#include "stdlib.h"

#ifndef STRINGCOMMANDPARSER_H_
#define STRINGCOMMANDPARSER_H_

#define SCP_CMD_BUFF_LENGTH         64
#define SCP_RX_BUFF_LENGTH          1024
#define SCP_MAX_CALLBACKS           10
#define CMD_PROCCESS_RATE_MS        100

typedef struct
{
    const char* pWaitForString;
    void (*fOnExecute)(const char *pString);
}TSCPCallBack;

typedef struct
{
    int64_t             timer;
    uint32_t            RxIndex;
    uint8_t             RxBuffer[SCP_RX_BUFF_LENGTH];
    TSCPCallBack        scpCallbacks[SCP_CMD_BUFF_LENGTH];
    uint32_t            (*fSendData)(uint8_t *pData, uint32_t lenght);
    uint32_t            (*fReadByte)(uint8_t *pData);
} TSCPHandler;


// function prototypes
#define SCP_OS_DELAY(x)     tx_thread_sleep (x)



// should be periodically called
void     SCP_Tick(uint32_t msecTick);

// exported functions
void     SCP_ByteReceived(uint8_t rxByte);
uint32_t SCP_AddCallback(const char *pCallbackstring, void (*fOnExecute)(const char *pString));
void SCP_Init(uint32_t (*fSendData)(uint8_t *pData, uint32_t lenght), uint32_t (*fReadByte)(uint8_t *pData));
char * SCP_SendCommandWaitAnswer(char *pCmd,  char *pAnswer, uint32_t timeout, uint8_t retry);
char * SCP_SendDoubleCommandWaitAnswer(char *pCmd1, char *pCmd2, char *pAnswer1, char *pAnswer2, uint32_t timeout, uint8_t retry);
void SCP_SendData(char *pData, uint32_t length);
char *SCP_WaitForAnswer(char *pAnswer, uint32_t timeout);
void SCP_Process(void);
void SCP_InitRx(void);
uint32_t uart_send_buff(uint8_t *data_out, uint32_t size);
uint32_t uart_read_byte(uint8_t *pData);

#endif /* STRINGCOMMANDPARSER_H_ */
