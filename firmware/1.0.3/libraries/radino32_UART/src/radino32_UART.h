/*
        Pin, register and other definitions for ESP-UART chip on In-Circuit radino WiFi modules
        for more information: www.in-circuit.de or www.radino.cc

	Copyright (c) 2015 In-Circuit GmbH

	Permission is hereby granted, free of charge, to any person obtaining a copy of this software
	and associated documentation files (the "Software"), to deal in the Software without restriction,
	including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
	and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
	LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
	WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef radino32_UART_h
#define radino32_UART_h

#include "Arduino.h"
#include <HardwareSerial.h>

#define A4 29
#define MCUCR 0
#define EICRB 0
#define EIMSK 0
#define SREG 0

typedef uint8_t (*cb_type_name)(uint8_t);

#define setINT_Overflow() defineInterrupts(IER_RXLINE) // Must check for overflow manually from LSR or check interrupt status from IIR
#define setINT_RXlvl() defineInterrupts(IER_RXLVL)
#define setINT_None() defineInterrupts(0)

class radino32_UART : public HardwareSerial
{
	public:
		radino32_UART(uint8_t chipselect) :
			HardwareSerial(USART3, GPIO_PB_USART3_TX, GPIO_PB_USART3_RX)
		{
			_chipselect = chipselect;	// Save ChipSelect pin number
		};

		radino32_UART() :
			HardwareSerial(USART3, GPIO_PB_USART3_TX, GPIO_PB_USART3_RX)
		{
			_chipselect = 36; //RF_nRST
		};


		uint8_t init(); // Init SC16IS750

		void enableSleeping(void); // Enable sleep mode globally when idle

		uint8_t txavailable(void);// Get number of available bytes within the UART's TX-Buffer. 0 = full

		// Set GPIO-Pin (0..7) direction/mode
		// 1 = Output == SC16_GPIO_INPUT
		// 0 = Input == SC16_GPIO_OUTPUT
		void GPIO_pinMode(uint8_t pin, WiringPinMode mode);

		// Set value of GPIO pin (0..7)
		// 0 = LOW
		// 1 = HIGH
		void GPIO_digitalWrite(uint8_t pin, uint8_t value);

		// Read value of GPIO pin (0..7)
		// 0 = LOW
		// 1 = HIGH
		uint8_t GPIO_digitalRead(uint8_t pin);

		uint8_t defineInterrupts(uint8_t type);
		void attachInterruptCallback(cb_type_name function);

		void setBaudrate(uint32_t baudrate); // Set Baudrate of SC16IS750
		void writeRegister(uint8_t thisRegister, uint8_t thisValue); // Writes a value to a specified register over SPI
		byte readRegister(uint8_t thisRegister); // Reads a value from a specified register over SPI


		//const cb_type_name interruptCallbackPointer = NULL;
		//static uint32_t interruptCallbackPointer = 0;

		void runCallback(uint8_t val);

	private:
		uint8_t _chipselect;

	};



// Pin definitions
//#define  SC16_CS    8  // CS connected to Arduino D8
//#define  SC16_MOSI  MOSI
//#define  SC16_MISO  MISO
//#define  SC16_SCK   SCK

#define  SC16_IRQ   7  // IRQ at D7 / INT6

// Defines
#define  WRITE  0x00  // SPI 'write'-mask
#define  READ   0x80  // SPI 'read'-mask

//define as Arduino-API WiringPinMode values
#define SC16_GPIO_INPUT   INPUT
#define SC16_GPIO_OUTPUT  OUTPUT

// Register definitions

#define  RHR  0x00  // Receive Holding Register; Read only
#define  THR  0x00  // Transmit Holding Register; Write only

#define  IER  0x01  // Interrupt Enable Register; R/W
  #define SLEEP_MODE  4  // Bit to enable sleep mode

#define	 IER_RXLVL	(1<<0)	// Receive holding register interrupt
#define	 IER_TXLVL	(1<<1)	// Transmit holding register interrupt
#define	 IER_RXLINE	(1<<2)	// Receive line status interrupt
#define	 IER_MODEMSTATUS	(1<<3)	// Modem status interrupt
//#define	 IER_RXLVL	(1<<4)	// RX lvl INT
#define	 IER_XOFF	(1<<5)	// Xoff interrupt
#define	 IER_RTS	(1<<6)	// RTS interrupt
#define	 IER_CTS	(1<<7)	// CTS interrupt

#define  FCR  0x02  // FIFO Control Register; Write only
#define  IIR  0x02  // Interrupt Identification Register; Read only

#define  LCR  0x03  // Line Control Register; R/W

#define  LCR_DIVISOR        7  // Define for each bit of LCR register
#define  LCR_BREAK          6
#define  LCR_PARITY_FORCED  5
#define  LCR_PARITY_TYPE    4
#define  LCR_PARITY_EN      3
#define  LCR_STOP           2
#define  LCR_WORD_LENGTH    0  // 2 Bits!

#define  LSR  0x05  // Line Status Register; Read only

#define  MCR  0x04  // Modem Control Register; R/W
#define  MSR  0x06  // Modem Status Register; Read only

#define  SPR  0x07  // Scratch Pad Register; Can be used to store data - no effect to the device; R/W

// Only accessible when MCR[2]=1 and EFR[4]=1
#define  TCR  0x06  // R/W
#define  TLR  0x07  // Trigger Level Register; R/W

#define  TXLVL  0x08  // Transmit FIFO level; Read only
#define  RXLVL  0x09  // Receive FIFO level; Read only

#define  IODir      0x0A  // I/O dicrection; R/W
#define  IOState    0x0B  // R/W
#define  IOIntEna   0x0C  // I/O Interrupt Enable; R/W
#define  IOControl  0x0E  // I/O control; R/W

#define  EFCR  0x0F  // Extra Features Control; R/W

// Special register set
// Only accessible when LCR[7]=1
#define  DLL  0x00  // Baud rate divisor; Low byte; R/W
#define  DLH  0x01  // Baud rate divisor; High byte, R/W

// Enhanced register set
// Only accessible when LCR=0xBF
#define  EFR   0x02  // Extra feature register; R/W
  #define  ENABLE_ENHANCED_FUNCTIONS  4
#define  XON1  0x04  // R/W
#define  XON2  0x05  // R/W
#define  XOFF1 0x06  // R/W
#define  XOFF2 0x07  // R/W

#endif
