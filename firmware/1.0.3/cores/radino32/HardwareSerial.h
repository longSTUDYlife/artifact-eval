/******************************************************************************
 * The MIT License
 *
 * Copyright (c) 2010 Perry Hung.
 * Copyright (c) 2011, 2012 LeafLabs, LLC.
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
 * @file wirish/include/wirish/HardwareSerial.h
 * @brief Wirish serial port interface.
 */

#ifndef _WIRISH_HARDWARESERIAL_H_
#define _WIRISH_HARDWARESERIAL_H_

#include <stm32/l1/rcc.h>
#include <stm32/l1/usart.h>
#include "Print.h"
#include "boards.h"
#include "Stream.h"
#include "ring_buffer.h"

/*
 * IMPORTANT:
 *
 * This class documented "by hand" (i.e., not using Doxygen) in the
 * leaflabs-docs/ repository.
 *
 * If you alter the public HardwareSerial interface, you MUST update
 * the documentation accordingly.
 */



// Define constants and variables for buffering incoming serial data.  We're
// using a ring buffer (I think), in which head is the index of the location
// to which to write the next incoming character and tail is the index of the
// location from which to read.
#if !(defined(SERIAL_RX_BUFFER_SIZE))
#define SERIAL_TX_BUFFER_SIZE 256
#define SERIAL_RX_BUFFER_SIZE 256
#endif

/* Roger Clark
 *
 * Added config defines from AVR
 * Note. The values will need to be changed to match STM32 USART config register values, these are just place holders.
 */
// Define config for Serial.begin(baud, config);
// Note. STM32 doesn't support as many different Serial modes as AVR or SAM cores.

#define SERIAL_8N1	0B00000000
#define SERIAL_8N2	0B00100000
#define SERIAL_9N1	0B00001000
#define SERIAL_9N2	0B00101000

#define SERIAL_8E1	0B00000010
#define SERIAL_8E2	0B00100010
#define SERIAL_9E1	0B00001010
#define SERIAL_9E2	0B00101010

#define SERIAL_8O1	0B00000011
#define SERIAL_8O2	0B00100011
#define SERIAL_9O1	0B00001011
#define SERIAL_9O2	0B00101011

typedef uint8_t (*cb_type_name)(uint8_t);

/* Roger clark. Changed class inheritance from Print to Stream.
 * Also added new functions for peek() and availableForWrite()
 * Note. AvailableForWrite is only a stub function in the cpp
 */
class HardwareSerial : public Stream {

public:
    HardwareSerial(usart_t usart_device,
                   int tx_pin,
                   int rx_pin);
    void setTxBuf(unsigned char *buf, unsigned long len);
    /* Set up/tear down */
    void begin(uint32 baud);
    void begin(uint32 baud,uint8_t config);
    void end();
    virtual int available(void);
    virtual int peek(void);
    virtual int read(void);
    int availableForWrite(void);
    int queuedTx(void);
    virtual void flush(void);
    virtual size_t write(uint8_t);
    inline size_t write(unsigned long n) { return write((uint8_t)n); }
    inline size_t write(long n) { return write((uint8_t)n); }
    inline size_t write(unsigned int n) { return write((uint8_t)n); }
    inline size_t write(int n) { return write((uint8_t)n); }
    using Print::write;

    /* Pin accessors */
    int txPin(void) { return this->tx_pin; }
    int rxPin(void) { return this->rx_pin; }

    void use_tx_LED(bool);
    void use_rx_LED(bool);
	operator bool() { return true; }
    bool tx_LED_enabled(void) {return this->_tx_LED_enabled; }
    bool rx_LED_enabled(void) {return this->_rx_LED_enabled; }

    /* Escape hatch into libmaple */
    /* FIXME [0.0.13] documentation */
    usart_t c_dev(void) { return this->usart_device; }
    ring_buffer *get_rx_rb(void) { return this->rx_rb; }
    ring_buffer *get_tx_rb(void) { return this->tx_rb; }
    void attachRxCallback(cb_type_name function);
    void runRxCallback(uint8_t val);


private:
    usart_t usart_device;
    int tx_pin;
    int rx_pin;
    unsigned char *rx_buffer;
    ring_buffer *rx_rb;
    unsigned char *tx_buffer;
    ring_buffer *tx_rb;
    bool _tx_LED_enabled = false;
    bool _rx_LED_enabled = false;
    cb_type_name rx_callback = NULL;
  protected:
#if 0
    volatile uint8_t * const _ubrrh;
    volatile uint8_t * const _ubrrl;
    volatile uint8_t * const _ucsra;
    volatile uint8_t * const _ucsrb;
    volatile uint8_t * const _ucsrc;
    volatile uint8_t * const _udr;
    // Has any byte been written to the UART since begin()
    bool _written;

    volatile rx_buffer_index_t _rx_buffer_head;
    volatile rx_buffer_index_t _rx_buffer_tail;
    volatile tx_buffer_index_t _tx_buffer_head;
    volatile tx_buffer_index_t _tx_buffer_tail;
    // Don't put any members after these buffers, since only the first
    // 32 bytes of this struct can be accessed quickly using the ldd
    // instruction.
    unsigned char _rx_buffer[SERIAL_RX_BUFFER_SIZE];
    unsigned char _tx_buffer[SERIAL_TX_BUFFER_SIZE];
#endif
};

extern HardwareSerial Serial1;
extern HardwareSerial Serial2;
extern HardwareSerial Serial3;

#endif
