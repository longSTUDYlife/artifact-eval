/**
  ******************************************************************************
  * @file    usb_endp.c
  * @author  MCD Application Team
  * @version V4.0.0
  * @date    21-January-2013
  * @brief   Endpoint routines
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
  * Modified 2016 for radino32 compatibility by In-Circuit GmbH  
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */


/* Includes ------------------------------------------------------------------*/
#include "usb_lib.h"
#include "usb_desc.h"
#include "usb_mem.h"
#include "hw_config.h"
#include "usb_endp.h"
#include "usb_istr.h"
#include "usb_pwr.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/* Interval between sending IN packets in frame number (1 frame = 1ms) */
#define VCOMPORT_IN_FRAME_INTERVAL             5
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
__IO uint32_t packet_sent = 0;
__IO uint32_t packet_receive = 0;
__IO uint8_t Receive_Buffer[RECEIVE_BUFFER_SIZE];
__IO uint8_t Receive_End = 0;
__IO uint8_t Receive_Start = 0;
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/


uint8_t Receive_Available_For_Reading(void)
{
  uint8_t Available = 0;

  if (Receive_Start <= Receive_End)
  {
    Available = Receive_End - Receive_Start;
  }
  else
  {
    Available += RECEIVE_BUFFER_SIZE - Receive_Start;
    Available += Receive_End;
  }

  return Available;
}


static uint8_t Receive_Free(void)
{
  return RECEIVE_BUFFER_SIZE - Receive_Available_For_Reading() - 1;
}


/*******************************************************************************
* Function Name  : EP1_IN_Callback
* Description    :
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/

void EP1_IN_Callback (void)
{
  packet_sent = 1;
}

/*******************************************************************************
* Function Name  : EP3_OUT_Callback
* Description    :
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void EP3_OUT_Callback(void)
{
  packet_receive = 1;

  uint16_t Received = GetEPRxCount(ENDP3);

  // FIXME: Add handling for excess data. We always request less data than the
  // available buffer space. So just copy blindly for now.
  if (Receive_Start > Receive_End)
  {
    PMAToUserBufferCopy((unsigned char*)(Receive_Buffer + Receive_End),
      ENDP3_RXADDR, Received);
    Receive_End += Received;
  }
  else
  {
    const uint8_t To_End = RECEIVE_BUFFER_SIZE - Receive_End;
    uint8_t To_Copy = Received > To_End ? To_End : Received;
    uintptr_t EP_Read_Address = ENDP3_RXADDR;

    // Copy the first chunk towards the buffer's end.
    PMAToUserBufferCopy((unsigned char*)(Receive_Buffer + Receive_End),
      EP_Read_Address, To_Copy);
    Receive_End = (Receive_End + To_Copy) % RECEIVE_BUFFER_SIZE;
    EP_Read_Address += To_Copy;
    Received -= To_Copy;

    // Copy remaining data from the beginning.
    if (Received > 0)
    {
      PMAToUserBufferCopy((unsigned char*)(Receive_Buffer + Receive_End),
        EP_Read_Address, Received);
      Receive_End += Received;
    }
  }

  // Receive more data if there is sufficient room.
  if (Receive_Free() > VIRTUAL_COM_PORT_DATA_SIZE)
  {
    packet_receive = 0;
    SetEPRxValid(ENDP3);
  }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
