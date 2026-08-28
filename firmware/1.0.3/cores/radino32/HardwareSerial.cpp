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
 * @file wirish/HardwareSerial.cpp
 * @brief Wirish serial port implementation.
 */

#include "HardwareSerial.h"

#include <stm32/l1/gpio.h>
#include <stm32/l1/usart.h>
#include <io.h>
#include "usart_irq.h"

HardwareSerial Serial1(USART1, GPIO_PA_USART1_TX, GPIO_PA_USART1_RX);
HardwareSerial Serial2(USART2, GPIO_PA_USART2_TX, GPIO_PA_USART2_RX);
HardwareSerial Serial3(USART3, GPIO_PB_USART3_TX, GPIO_PB_USART3_RX);
static unsigned char _rx_buffer[3][SERIAL_RX_BUFFER_SIZE];
static ring_buffer _rx_rb[3];
static unsigned char _tx_buffer[3][SERIAL_TX_BUFFER_SIZE];
static ring_buffer _tx_rb[3];

HardwareSerial::HardwareSerial(usart_t usart_device,
                               int tx_pin,
                               int rx_pin) {
    this->usart_device = usart_device;
    this->tx_pin = tx_pin;
    this->rx_pin = rx_pin;

    switch(usart_device) {
    case USART1:
        this->rx_buffer = _rx_buffer[0];
        this->rx_rb = &_rx_rb[0];
        this->tx_buffer = _tx_buffer[0];
        this->tx_rb = &_tx_rb[0];
        break;
    case USART2:
        this->rx_buffer = _rx_buffer[1];
        this->rx_rb = &_rx_rb[1];
        this->tx_buffer = _tx_buffer[1];
        this->tx_rb = &_tx_rb[1];
        break;
    case USART3:
        this->rx_buffer = _rx_buffer[2];
        this->rx_rb = &_rx_rb[2];
        this->tx_buffer = _tx_buffer[2];
        this->tx_rb = &_tx_rb[2];
        break;
    }

    rb_init(this->rx_rb, SERIAL_RX_BUFFER_SIZE, this->rx_buffer);
    rb_init(this->tx_rb, SERIAL_TX_BUFFER_SIZE, this->tx_buffer);
}

void HardwareSerial::setTxBuf(unsigned char *buf, unsigned long len)
{
  this->tx_buffer = buf;
  rb_init(this->tx_rb, len, this->tx_buffer);
}

/*
 * Set up/tear down
 */


void HardwareSerial::begin(uint32 baud)
{
	begin(baud,SERIAL_8N1);
}
/*
 * Roger Clark.
 * Note. The config parameter is not currently used. This is a work in progress.
 * Code needs to be written to set the config of the hardware serial control register in question.
 *
*/

void HardwareSerial::begin(uint32 baud, uint8_t config)
{

    /* Setup GPIO pin as alternate function. */
	gpio_config_altfn(GPIO_USART1_3, GPIO_PUSHPULL, GPIO_2MHZ,
                      GPIO_NOPUPD,  this->tx_pin);

    gpio_config_altfn(GPIO_USART1_3, GPIO_PUSHPULL, GPIO_2MHZ,
                      GPIO_NOPUPD,  this->rx_pin);

    /* Enable USART Receive interrupt. */
	usart_enable_interrupt(this->usart_device, USART_RXNE);
  
  // Setup USART
  switch (config)
  {
    default:
    case SERIAL_8N1:
      usart_init(this->usart_device, PCLK1, baud, 8, USART_STOP_1, USART_PARITY_NONE, USART_FLOW_NONE, USART_TX_RX);
      break;
    case SERIAL_8N2:
      usart_init(this->usart_device, PCLK1, baud, 8, USART_STOP_2, USART_PARITY_NONE, USART_FLOW_NONE, USART_TX_RX);
      break;
    case SERIAL_9N1:
      usart_init(this->usart_device, PCLK1, baud, 9, USART_STOP_1, USART_PARITY_NONE, USART_FLOW_NONE, USART_TX_RX);
      break;
    case SERIAL_9N2:
      usart_init(this->usart_device, PCLK1, baud, 9, USART_STOP_2, USART_PARITY_NONE, USART_FLOW_NONE, USART_TX_RX);
      break;
    case SERIAL_8E1:
      usart_init(this->usart_device, PCLK1, baud, 8, USART_STOP_1, USART_EVEN, USART_FLOW_NONE, USART_TX_RX);
      break;
    case SERIAL_8E2:
      usart_init(this->usart_device, PCLK1, baud, 8, USART_STOP_2, USART_EVEN, USART_FLOW_NONE, USART_TX_RX);
      break;
    case SERIAL_9E1:
      usart_init(this->usart_device, PCLK1, baud, 9, USART_STOP_1, USART_EVEN, USART_FLOW_NONE, USART_TX_RX);
      break;
    case SERIAL_9E2:
      usart_init(this->usart_device, PCLK1, baud, 9, USART_STOP_2, USART_EVEN, USART_FLOW_NONE, USART_TX_RX);
      break;
    case SERIAL_8O1:
      usart_init(this->usart_device, PCLK1, baud, 8, USART_STOP_1, USART_ODD, USART_FLOW_NONE, USART_TX_RX);
      break;
    case SERIAL_8O2:
      usart_init(this->usart_device, PCLK1, baud, 8, USART_STOP_2, USART_ODD, USART_FLOW_NONE, USART_TX_RX);
      break;
    case SERIAL_9O1:
      usart_init(this->usart_device, PCLK1, baud, 9, USART_STOP_1, USART_ODD, USART_FLOW_NONE, USART_TX_RX);
      break;
    case SERIAL_9O2:
      usart_init(this->usart_device, PCLK1, baud, 9, USART_STOP_2, USART_ODD, USART_FLOW_NONE, USART_TX_RX);
      break;
  }
  use_tx_LED(this->_tx_LED_enabled);
  use_rx_LED(this->_rx_LED_enabled);
}

void HardwareSerial::end(void) {
    usart_disable(this->usart_device);
}

/*
 * I/O
 */

int HardwareSerial::read(void) {
    while (!this->available());
    return rb_remove(this->rx_rb);
}

int HardwareSerial::available(void) {
    return rb_full_count(this->rx_rb);
}

/* Roger Clark. Added function missing from LibMaple code */

int HardwareSerial::peek(void)
{
    return rb_peek(this->rx_rb);
}

int HardwareSerial::availableForWrite(void)
{
/* Roger Clark.
 * Currently there isn't an output ring buffer, chars are sent straight to the hardware.
 * so just return 1, meaning that 1 char can be written
 * This will be slower than a ring buffer implementation, but it should at least work !
 */
  //IC todo implement faster routine?
  return (this->tx_rb->size-rb_full_count(this->tx_rb));
  //return 1;
}

int HardwareSerial::queuedTx(void)
{
  return rb_full_count(this->tx_rb);
}

size_t HardwareSerial::write(uint8_t ch)
{
#if 1
	while(rb_is_full(this->tx_rb))
	{ }
	rb_insert(this->tx_rb, ch);
	usart_enable_interrupt(this->usart_device, USART_TXE);
	return 1;
#else
	if (this->tx_LED_enabled())
		digitalWrite(BOARD_TX_LED, HIGH);

	usart_send_blocking(this->usart_device, ch);

	if (this->tx_LED_enabled())
		digitalWrite(BOARD_TX_LED, LOW);
	return 1;
#endif
}

void HardwareSerial::flush(void) {
    //rb_reset(this->rx_rb);
    while(!rb_is_empty(this->tx_rb) || !usart_get_interrupt_status(this->usart_device, USART_TC))
    { }
    //usart_reset_rx(this->usart_device);
}

void HardwareSerial::use_tx_LED(bool val) {
    _tx_LED_enabled = val;
    if (val) {
        pinMode(BOARD_TX_LED, OUTPUT);
        digitalWrite(BOARD_TX_LED, LOW);
    }
}

void HardwareSerial::use_rx_LED(bool val) {
    _rx_LED_enabled = val;
    if (val) {
        pinMode(BOARD_RX_LED, OUTPUT);
        digitalWrite(BOARD_RX_LED, LOW);
    }
}

void HardwareSerial::attachRxCallback(cb_type_name function) {
    rx_callback = function;
}

void HardwareSerial::runRxCallback(uint8_t val) {
    if (this->rx_callback)
        this->rx_callback(val);
}

void usart_rx_interrupt(int serial) {
    usart_t usart_dev;
    HardwareSerial *s;
    uint8_t c;

    switch (serial) {
    case 1:
        s = &Serial1;
        usart_dev = USART1;
        break;
    case 2:
        s = &Serial2;
        usart_dev = USART2;
        break;
    case 3:
        s = &Serial3;
        usart_dev = USART3;
        break;
    default:
        return;
    }
    
		if ( (usart_get_interrupt_status(usart_dev, USART_RXNE)) || (usart_get_interrupt_status(usart_dev, USART_ORE)) )
		{
			if (s->rx_LED_enabled())
				digitalWrite(BOARD_RX_LED, HIGH);
			c = usart_recv(usart_dev);
			rb_insert(s->get_rx_rb(), c);
			s->runRxCallback(c);
			if (s->rx_LED_enabled())
				digitalWrite(BOARD_RX_LED, LOW);
		}
		if(usart_get_interrupt_status(usart_dev, USART_TXE))
		{
			if (rb_is_empty(s->get_tx_rb()))
			{
				usart_disable_interrupt(usart_dev, USART_TXE);
				if (s->tx_LED_enabled())
					digitalWrite(BOARD_TX_LED, LOW);
			} else {
				if (s->tx_LED_enabled())
					digitalWrite(BOARD_TX_LED, HIGH);
				usart_send(usart_dev, rb_remove(s->get_tx_rb()));
			}
		}
}
