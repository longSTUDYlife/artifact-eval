/******************************************************************************
 * The MIT License
 *
 * Copyright (c) 2010 Perry Hung.
 * Modified 2016 for radino32 compatibility by In-Circuit GmbH
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *****************************************************************************/

/**
 * @brief Wirish USB virtual serial port (SerialUSB).
 */

#ifndef _WIRISH_USB_SERIAL_H_
#define _WIRISH_USB_SERIAL_H_

extern "C" {
#include "usb-cdcacm/hw_config.h"
#include "usb-cdcacm/usb_init.h"
#include "usb-cdcacm/usb_pwr.h"
#include "usb-cdcacm/usbio.h"
}

#include <Arduino.h>
#include "Print.h"
#include "boards.h"
#include "Stream.h"

/**
 * @brief Virtual serial terminal.
 */
class USBSerial : public Stream
{
  public:
    USBSerial() {};
    
    void begin(void);
    
    void begin(unsigned long){begin();};
    void begin(unsigned long, uint8_t){begin();};
    
    void end(void);
    
    bool isConfigured();
    operator bool() {return isConfigured();};
    
    virtual int available(void);
    
    virtual int peek(void);
    virtual int read(void);
    
    //int availableForWrite(void);
    virtual void flush(void);
    
    virtual size_t write(uint8);
    size_t write(const char *str);
    size_t write(const void* data, uint32 len);
    using Print::write;
    
    //uint8 getRTS();
    //uint8 getDTR();
    //uint8 isConnected();
    //uint8 pending();
    
    void use_tx_LED(bool);
    void use_rx_LED(bool);
    bool tx_LED_enabled(void) {return this->_tx_LED_enabled; }
    bool rx_LED_enabled(void) {return this->_rx_LED_enabled; }
    
  private:
    bool _tx_LED_enabled = false;
    bool _rx_LED_enabled = false;
};

#if BOARD_HAVE_SERIALUSB 
  extern USBSerial Serial;
#else
  #define Serial Serial1
#endif

#endif

