#ifndef USB_ENDP_H
#define USB_ENDP_H


#include "usb_desc.h"


#define RECEIVE_BUFFER_SIZE (3 * VIRTUAL_COM_PORT_DATA_SIZE)


extern __IO uint32_t packet_sent;
extern __IO uint32_t packet_receive;

extern __IO uint8_t Receive_Buffer[RECEIVE_BUFFER_SIZE];
extern __IO uint8_t Receive_End;
extern __IO uint8_t Receive_Start;


uint8_t Receive_Available_For_Reading(void);


#endif
