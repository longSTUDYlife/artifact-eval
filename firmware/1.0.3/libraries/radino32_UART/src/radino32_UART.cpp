/*
        ESP-UART on In-Circuit radino32 WiFi modules
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
/*
 * UART Class compatible with SPI_UART
 *
 * uses HardwareSerial3 and implements all additional functions from SPI_UART
*/

#include "Arduino.h"
#include "radino32_UART.h"

// Callback pointer for interrupt
static cb_type_name interruptCallbackPointer = NULL;



// Init SC16IS750
uint8_t radino32_UART::init()
{
	return 1;
}

// Not used
void radino32_UART::enableSleeping(void)
{
	return;
}

// tx is not buffered, always return 1
uint8_t radino32_UART::txavailable(void)
{
	return 1;
}

// accesses Radino Pins directly
void radino32_UART::GPIO_pinMode(uint8_t pin, WiringPinMode mode)
{
	pinMode(pin, mode);
}

// accesses Radino Pins directly
void radino32_UART::GPIO_digitalWrite(uint8_t pin, uint8_t value)
{
	digitalWrite(pin, value);
}

// accesses Radino Pins directly
uint8_t radino32_UART::GPIO_digitalRead(uint8_t pin)
{
	digitalRead(pin);
}

// Execute callback function
void radino32_UART::runCallback(uint8_t val)
{
	Serial3.runRxCallback(val);
}

// Interrupt is enabled by default
uint8_t radino32_UART::defineInterrupts(uint8_t type)
{
	return 1;
}

// Attach function which should be called after each interrupt
void radino32_UART::attachInterruptCallback(cb_type_name function)
{
	Serial3.attachRxCallback(function);
}

void radino32_UART::setBaudrate(uint32_t baudrate)
{
	begin(baudrate);
}

// Not used
void radino32_UART::writeRegister(uint8_t thisRegister, uint8_t thisValue) {
	return;
}

// Not used
byte radino32_UART::readRegister(uint8_t thisRegister) {
	return 0;
}
