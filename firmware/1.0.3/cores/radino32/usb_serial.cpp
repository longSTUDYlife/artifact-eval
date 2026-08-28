#include "usb_serial.h"

#if BOARD_HAVE_SERIALUSB
  USBSerial Serial;
#endif

void USBSerial::begin(void)
{
  Set_System();
  Set_USBClock();
  USB_Interrupts_Config();
  USB_Init();
  
  use_tx_LED(this->_tx_LED_enabled);
  use_rx_LED(this->_rx_LED_enabled);
}

void USBSerial::use_tx_LED(bool val) {
  _tx_LED_enabled = val;
  if (val) {
    pinMode(BOARD_TX_LED, OUTPUT);
    digitalWrite(BOARD_TX_LED, LOW);
  }
}

void USBSerial::use_rx_LED(bool val) {
  _rx_LED_enabled = val;
  if (val) {
    pinMode(BOARD_RX_LED, OUTPUT);
    digitalWrite(BOARD_RX_LED, LOW);
  }
}

void USBSerial::end(void)
{
  PowerOff();
  rcc_disable_clock(RCC_USB);
  bDeviceState = UNCONNECTED;
}

bool USBSerial::isConfigured()
{
  return (bDeviceState==CONFIGURED);
}

int USBSerial::peek(void)
{
  if (!isConfigured()) return 0;
  return upeekc();
}

void USBSerial::flush(void)
{
  uflush();
  return;
}

int USBSerial::read(void)
{
  if (!isConfigured()) return 0;
  return ugetc();
}

size_t USBSerial::write(uint8 ch)
{
  if (!isConfigured()) return 0;
  if (EOF!=uputc(ch)) return 1;
  return 0;
}

size_t USBSerial::write(const char *str)
{
  if (!isConfigured()) return 0;
  int result = uwrite(str, strlen(str));
  if (0>result) return 0;
  return result;
}

size_t USBSerial::write(const void* data, uint32 len)
{
  if (!isConfigured()) return 0;
  int result = uwrite(data, len);
  if (0>result) return 0;
  return result;
}

int USBSerial::available(void)
{
  if (!isConfigured()) return 0;
  return uavailable();
}
