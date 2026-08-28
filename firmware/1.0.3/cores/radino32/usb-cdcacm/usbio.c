#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "platform_config.h"
#include "hw_config.h"
//#include "stm32l1xx.h"
#include "usb_core.h"
#include "usb_endp.h"
#include "usb_pwr.h"
#include "usb_regs.h"
#include "usbio.h"


// TODO: Provide an I/O interface suitable for libc file I/O which would allow
// using higher-level functions like fgetline.


// FIXME: This chunk size is just a wild guess from the former echo service. It
// passed its received data directly to CDC_Send_DATA and the Receive_Buffer is
// declared in hw_config.c to be 64 bytes. But 64 bytes does not seem to work.
#define CHUNK_SIZE 32


size_t uavailable(void)
{
  size_t available = Receive_Available_For_Reading();

  // Try to receive more data in case there is nothing left in the buffer.
  if (0 == available)
  {
    CDC_Receive_DATA();
    available = Receive_Available_For_Reading();
  }

  return available;
}


int uflush(void)
{
    // The uwrite function just prepares the USB transfer. Wait for in-flight
    // transfer to complete.
    //
    // TODO: Introduce a define for the actually used endpoints and clean up
    // the USB CDC ACM code.
    while (EP_TX_VALID == GetEPTxStatus(ENDP1))
    {
    }

    return 0;
}


int upeekc(void)
{
  int c = EOF;

  if (uavailable() > 0)
  {
    c = Receive_Buffer[Receive_Start];
  }

  return c;
}


int ugetc(void)
{
  int c = EOF;

  if (uavailable() > 0)
  {
    c = Receive_Buffer[Receive_Start];
    Receive_Start = (Receive_Start + 1) % RECEIVE_BUFFER_SIZE;
  }

  return c;
}


int uputc(int c)
{
  if (1 == uwrite(&c, 1))
  {
    return c;
  }

  return EOF;
}


int uputs(const char *str)
{
  int result = uwrite(str, strlen(str));

  if (-1 == result)
  {
    result = EOF;
  }

  return result;
}


int uwrite(const void *buffer, size_t length)
{
  int result = -1;

  if (CONFIGURED != bDeviceState)
  {
      errno = ENOSPC;
      result = -1;
  }
  else
  {
    const char *bytes = (const char *)buffer;
    size_t remaining = length;

    // FIXME: CDC_Send_DATA has a remaininggth argument but id does not pay attention
    // to it. Fix CDC_Send_DATA so that we can use this function right here.
    while (remaining > CHUNK_SIZE)
    {
       if (1 != CDC_Send_DATA((uint8_t *)bytes, CHUNK_SIZE))
       {
         errno = EIO;
         return -1;
       }
       bytes += CHUNK_SIZE;
       remaining -= CHUNK_SIZE;
    }

    if (1 == CDC_Send_DATA((uint8_t *)bytes, remaining))
    {
      result = length;
    }
  }

  return result;
}
