/*
 * StringCommandParser.c
 *
 *  Created on: 2018-10-09
 *      Author: KJU
 */
#include "StringCommandParser.h"
#include "string.h"

/************************************************************************************************
 *  SCP globar variables
************************************************************************************************/
TSCPHandler   SCPHandler;

/************************************************************************************************
 *  Function prototypes
************************************************************************************************/
const char * SCP_CheckCommand(const char * str);
void SCP_Process(void);
char SCP_UpCase(char ch);
void SCP_GetBuff(void);

/************************************************************************************************
************************************************************************************************/
void SCP_ByteReceived(uint8_t rxByte)
{
    /*Put the byte into the buffer */
    if (SCPHandler.RxIndex < SCP_RX_BUFF_LENGTH)
    {
        SCPHandler.RxBuffer[SCPHandler.RxIndex++] = rxByte;
    }

    /*Buffer overflow */
    else
    {
    	SCP_InitRx();

        /*Write first byte at the beginning and exit*/
        SCPHandler.RxBuffer[SCPHandler.RxIndex++] = rxByte;
    }
}
/************************************************************************************************
************************************************************************************************/
void SCP_Tick(uint32_t msecTick)
 {
     if (SCPHandler.timer > 0)
     {
         SCPHandler.timer = SCPHandler.timer - msecTick;
     }
 }
/************************************************************************************************
************************************************************************************************/
char SCP_UpCase(char ch )
{
 if ((ch >= 'a')&&(ch <= 'z')) return (ch & ~0x20);
 return ch;
}

void SCP_Init(uint32_t (*fSendData)(uint8_t *pData, uint32_t lenght), uint32_t (*fReadByte)(uint8_t *pData))
{
    SCPHandler.RxIndex = 0;
    SCPHandler.timer = 0;
    SCPHandler.fSendData = fSendData;
    SCPHandler.fReadByte = fReadByte;

    for (int i = 0; i < SCP_MAX_CALLBACKS; i++)
    {
        SCPHandler.scpCallbacks[i].fOnExecute = 0;
        SCPHandler.scpCallbacks[i].pWaitForString = 0;
    }
}


uint32_t SCP_AddCallback(const char *pCallbackstring, void (*fOnExecute)(const char *pString))
{
    uint32_t i;
    for (i = 0; i < SCP_MAX_CALLBACKS; i++)
    {
        if (!SCPHandler.scpCallbacks[i].pWaitForString)
        {
            SCPHandler.scpCallbacks[i].pWaitForString = pCallbackstring;
            SCPHandler.scpCallbacks[i].fOnExecute = fOnExecute;
            return i;
        }
    }
    return 0;
}

void SCP_Process(void)
{
    const char *pReceivedString = 0;

    for (int i = 0; i < SCP_MAX_CALLBACKS; i++)
    {
        if (SCPHandler.scpCallbacks[i].pWaitForString && SCPHandler.scpCallbacks[i].fOnExecute)
        {
            pReceivedString  = SCP_CheckCommand(SCPHandler.scpCallbacks[i].pWaitForString);

            /*Execute callback if response string is in buffer*/
            if(pReceivedString)
            {
                SCPHandler.scpCallbacks[i].fOnExecute(pReceivedString);
            }
        }
    }

}

void SCP_GetBuff(void)
{
    uint8_t rxByte;
    uint32_t result = SCPHandler.fReadByte(&rxByte);
    while (!result)
    {
        if (SCPHandler.RxIndex < SCP_RX_BUFF_LENGTH)
        {
            SCPHandler.RxBuffer[SCPHandler.RxIndex++] = rxByte;
        }
        else
            return;
        result = SCPHandler.fReadByte(&rxByte);
    }
}

/******************************************************************************************
 * Parameter
 * str - pointer to string to look for
 *
 * Returns
 * If success returns pointer to the last byte of found string in rx buffer
 * if fail returns 0
******************************************************************************************/
const char * SCP_CheckCommand(const char * str)
{

uint32_t i=0;
uint32_t rxInx = 0;

 if (str)
  {

   while (rxInx < SCPHandler.RxIndex)
    {
     /*Low case or Up case characters may occur*/
     if (SCP_UpCase((char)str[i]) == SCP_UpCase((char)SCPHandler.RxBuffer[rxInx++])) // UpCase
      {
       /*Match found*/
       i++;

       /*If the end of the string reached*/
       if (str[i] == 0)
       {
           /*Return the pointer to the first occurrence in buffer*/
           return (const char *)&SCPHandler.RxBuffer[rxInx - i];
       }

      }

     /*No match, reset string pointer*/
     else i=0;
    }
  }
return NULL;
}


/************************************************************************************************
* PARAMETERS:
* pCmd - pointer to command
* pAnswer - pointer to answer that should be return from the device
* timeout - timeout in msecs
* retry - number of retries
*
* RETURN:
* 0 if error
* pointer to answer if success
************************************************************************************************/
 char * SCP_SendCommandWaitAnswer(char *pCmd, char *pAnswer, uint32_t timeout, uint8_t retry)
 {
     int    inx =0;
     char * pResult = NULL;

     /* flush RX */
     SCPHandler.RxIndex = 0;
     memset(SCPHandler.RxBuffer, 0x00, SCP_RX_BUFF_LENGTH);

     while (!pResult && (inx++ < retry))
     {
             SCPHandler.timer = timeout;
             SCPHandler.fSendData((uint8_t *)pCmd, strlen(pCmd));
             while ( (!pResult) && (SCPHandler.timer))
             {
                 pResult = (char *)SCP_CheckCommand(pAnswer);
                 SCP_Process();
             }
     }
     return pResult;

 }

 char * SCP_SendDoubleCommandWaitAnswer(char *pCmd1, char *pCmd2, char *pAnswer1, char *pAnswer2, uint32_t timeout, uint8_t retry)
 {
     int    inx =0;
     char * pResult = NULL;

     /* flush RX */
     SCPHandler.RxIndex = 0;
     memset(SCPHandler.RxBuffer, 0x00, SCP_RX_BUFF_LENGTH);

     /*Send first command, wait for response*/
     while (!pResult && (inx++ < retry))
     {
             SCPHandler.timer = timeout;
             SCPHandler.fSendData((uint8_t *)pCmd1, strlen(pCmd1));
             while ( (!pResult) && (SCPHandler.timer))
             {
                 pResult = (char *)SCP_CheckCommand(pAnswer1);
                 SCP_Process();
             }
     }

     /*Send second command, wait for response*/
     if(pResult)
     {
         /* flush RX */
         SCPHandler.RxIndex = 0;
         memset(SCPHandler.RxBuffer, 0x00, SCP_RX_BUFF_LENGTH);
         inx =0;
         pResult = NULL;

         while (!pResult && (inx++ < retry))
         {
                 SCPHandler.timer = timeout;
                 SCPHandler.fSendData((uint8_t *)pCmd2, strlen(pCmd2));
                 while ( (!pResult) && (SCPHandler.timer))
                 {
                     pResult = (char *)SCP_CheckCommand(pAnswer2);
                     SCP_Process();
                 }
         }

     }

     return pResult;
 }

 void SCP_InitRx(void)
 {
     SCPHandler.RxIndex = 0;
     memset(SCPHandler.RxBuffer, 0x00, SCP_RX_BUFF_LENGTH);
 }

 void SCP_SendData(char *pData, uint32_t length)
 {
     SCPHandler.fSendData((uint8_t *)pData, length);
 }

 char *SCP_WaitForAnswer(char *pAnswer, uint32_t timeout)
 {
     char * pResult = NULL;

     SCPHandler.timer = timeout;
     while ( (!pResult) && (SCPHandler.timer))
      {
          pResult = (char *)SCP_CheckCommand(pAnswer);
          SCP_Process();
      }
     return pResult;
 }

