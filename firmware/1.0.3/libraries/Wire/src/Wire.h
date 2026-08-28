/*
  TwoWire.h - TWI/I2C library for Arduino & Wiring
  Copyright (c) 2006 Nicholas Zambetti.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Modified 2012 by Todd Krein (todd@krein.org) to implement repeated starts
  Modified 2016 by In-Circuit GmbH for radino32
*/

#ifndef TwoWire_h
#define TwoWire_h

// #include <inttypes.h>
// #include "Stream.h"

#include <Arduino.h>
#include <stm32/l1/gpio.h>
#include <stm32/l1/rcc.h>
#include <stm32/l1/i2c.h>

#define I2C_TIMEOUT_MAX 4723	//Fixme ... implement something time based

#define BUFFER_LENGTH 32

#define I2C_CLOCK_SPEED 400000

class TwoWire : public Stream
{
  private:
    static uint8_t rxBuffer[];
    static volatile uint8_t rxBufferIndex;
    static volatile uint8_t rxBufferLength;

    static volatile uint8_t txAddress;
    static uint8_t txBuffer[];
    static volatile uint8_t txBufferIndex;
    static volatile uint8_t txBufferLength;
    
    static volatile i2c_t hardwareChannel; // I2C1 / I2C2

    static volatile uint8_t transmitting;
    static void (*user_onRequest)(void);
    static void (*user_onReceive)(int);
    static void onRequestService(void);
    static void onReceiveService(uint8_t*, int);
  public:
    TwoWire();
    void begin();
    void begin(uint8_t);
    void begin(int);
    void end();
    void setHardwareChannel(i2c_t i2cPort);
    void setClock(uint32_t);
    void beginTransmission(uint8_t);
    void beginTransmission(int);
    uint8_t endTransmission(void);
    uint8_t endTransmission(uint8_t);
    uint8_t requestFrom(uint8_t, uint8_t);
    uint8_t requestFrom(uint8_t, uint8_t, uint8_t);
    uint8_t requestFrom(int, int);
    uint8_t requestFrom(int, int, int);
    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t *, size_t);
    virtual int available(void);
    virtual int read(void);
    virtual int peek(void);
    virtual void flush(void);
    void onReceive( void (*)(int) );
    void onRequest( void (*)(void) );

    inline size_t write(unsigned long n) { return write((uint8_t)n); }
    inline size_t write(long n) { return write((uint8_t)n); }
    inline size_t write(unsigned int n) { return write((uint8_t)n); }
    inline size_t write(int n) { return write((uint8_t)n); }
    using Print::write;
};

extern TwoWire Wire;

#endif

