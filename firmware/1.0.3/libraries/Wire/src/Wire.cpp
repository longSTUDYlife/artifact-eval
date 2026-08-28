/*
  TwoWire.cpp - TWI/I2C library for Wiring & Arduino
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

extern "C" {
//    #include <stdlib.h>
//   #include <string.h>
//    #include <inttypes.h>
//   #include "twi.h"
}

#include "Wire.h"

// Initialize Class Variables //////////////////////////////////////////////////

uint8_t TwoWire::rxBuffer[BUFFER_LENGTH];
volatile uint8_t TwoWire::rxBufferIndex = 0;
volatile uint8_t TwoWire::rxBufferLength = 0;

volatile uint8_t TwoWire::txAddress = 0;
uint8_t TwoWire::txBuffer[BUFFER_LENGTH];
volatile uint8_t TwoWire::txBufferIndex = 0;
volatile uint8_t TwoWire::txBufferLength = 0;

volatile i2c_t TwoWire::hardwareChannel; // I2C1 / I2C2

volatile uint8_t TwoWire::transmitting = 0;      // flag only relevant for slave mode (?)
void (*TwoWire::user_onRequest)(void);
void (*TwoWire::user_onReceive)(int);

// Constructors ////////////////////////////////////////////////////////////////

TwoWire::TwoWire()
{
    // Use I2C1 by default
    hardwareChannel =  I2C1;
}

// Public Methods //////////////////////////////////////////////////////////////

void TwoWire::begin(void)
{
  rxBufferIndex = 0;
  rxBufferLength = 0;

  txBufferIndex = 0;
  txBufferLength = 0;

  //////////////////////////////////////////////////////////////////////////////

  /* Init I2C */

  /* enable APB1 peripheral clock for I2C1*/
  rcc_enable_clock(RCC_GPIOB);

  switch(hardwareChannel)
  {
    case I2C1:
        /* enable APB1 peripheral clock for I2C1*/
        rcc_enable_clock(RCC_I2C1);

        /* De-Init / Disable I2C1 peripheral */
        rcc_enable_reset(RCC_I2C1);

        /* config pins for alternate function */
        gpio_config_altfn(GPIO_I2C1_2, GPIO_OPENDRAIN, GPIO_2MHZ, GPIO_PULLUP, GPIO_PB_I2C1_SCL);
        gpio_config_altfn(GPIO_I2C1_2, GPIO_OPENDRAIN, GPIO_2MHZ, GPIO_PULLUP, GPIO_PB_I2C1_SDA);

        /* Enable I2C1 peripheral */
        rcc_disable_reset(RCC_I2C1);
        break;

    case I2C2:
        /* enable APB1 peripheral clock for I2C2*/
        rcc_enable_clock(RCC_I2C2);

        /* De-Init / Disable I2C2 peripheral */
        rcc_enable_reset(RCC_I2C2);

        /* config pins for alternate function */
        gpio_config_altfn(GPIO_I2C1_2, GPIO_OPENDRAIN, GPIO_2MHZ, GPIO_PULLUP, GPIO_PB_I2C2_SCL);
        gpio_config_altfn(GPIO_I2C1_2, GPIO_OPENDRAIN, GPIO_2MHZ, GPIO_PULLUP, GPIO_PB_I2C2_SDA);

        /* Enable I2C2 peripheral */
        rcc_disable_reset(RCC_I2C2);
        break;
  }

  /* configure clocks */
  setClock(I2C_CLOCK_SPEED);
  
  /* Enable the I2C peripheral */
  i2c_set_bus_mode(hardwareChannel, I2C_ENABLE);

  //////////////////////////////////////////////////////////////////////////////  

}

void TwoWire::begin(uint8_t address)
{
// TODO: implement slave mode (?)
// -> i2c_set_own_address() etc.
//   twi_setAddress(address);
//   twi_attachSlaveTxEvent(onRequestService);
//   twi_attachSlaveRxEvent(onReceiveService);
  begin();
}

void TwoWire::begin(int address)
{
  begin((uint8_t)address);
}

void TwoWire::end(void)
{
  /* De-Init I2C */
  switch(hardwareChannel)
  {
    case I2C1:
        /* disable APB1 peripheral clock for I2C1*/
        rcc_disable_clock(RCC_I2C1);
        break;

    case I2C2:
        /* disable APB1 peripheral clock for I2C2*/
        rcc_disable_clock(RCC_I2C2);
        break;
  }    
}


/* Use this function to switch between I2C hardware channels I2C1 / I2C2 */
void TwoWire::setHardwareChannel(i2c_t i2cPort)
{
    switch(i2cPort)
    {
        case I2C1:
            hardwareChannel = I2C1;
            break;
        
        case I2C2:
            hardwareChannel = I2C2;
            break;
        
        default:
            hardwareChannel = I2C1;
            break;
    }
}

void TwoWire::setClock(uint32_t frequency)
{
  /* configure clocks & I2C-Mode */
  /* I2C-Mode can be: 
     I2C_STANDARD - SCL duty cycle: 1:1
     I2C_FAST, I2C_FAST_DUTY: SCL duty cycle: 16:9
     -> shorter "high"-state of SCL
     -> does this improve stability?
  */
  i2c_set_clock(hardwareChannel, (int) STM32_PCLK1, I2C_STANDARD, (int) frequency, (int) I2C_TIMEOUT_MAX);
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop)
{
  // clamp to buffer length
  if(quantity > BUFFER_LENGTH){
    quantity = BUFFER_LENGTH;
  }
  // perform blocking read into buffer
//   uint8_t read = twi_readFrom(address, rxBuffer, quantity, sendStop);

  /* Start condition */
  i2c_start(hardwareChannel, I2C_TIMEOUT_MAX);

  /* Send address with READ enabled */
  i2c_addr(hardwareChannel, (address<<1) | I2C_READ);
  
  /*  RM page 666
   *  read I2C_SR1 followed by I2C_SR2 to to toggle EV6
   *  this action is performed at the end of i2c_addr()
   *  so we don't need to do this right now */
//  i2c_get_interrupt_status(hardwareChannel, I2C_RXE); // read I2C_SR1
//  i2c_get_status(hardwareChannel, I2C_MASTER); // read I2C_SR2
  
  /* Use i2c_read to read several registers */
  uint8_t read = i2c_read(hardwareChannel, rxBuffer, quantity);  
  
  /* Stop condition */
  i2c_stop(hardwareChannel, I2C_TIMEOUT_MAX);

  /*  set rx buffer iterator vars */
  rxBufferIndex = 0;
  rxBufferLength = read;

  return read;
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity)
{
  return requestFrom((uint8_t)address, (uint8_t)quantity, (uint8_t)true);
}

uint8_t TwoWire::requestFrom(int address, int quantity)
{
  return requestFrom((uint8_t)address, (uint8_t)quantity, (uint8_t)true);
}

uint8_t TwoWire::requestFrom(int address, int quantity, int sendStop)
{
  return requestFrom((uint8_t)address, (uint8_t)quantity, (uint8_t)sendStop);
}

void TwoWire::beginTransmission(uint8_t address)
{
  /* indicate that we are transmitting */
  transmitting = 1;
  
  /* set address of targeted slave */
  txAddress = address;
  
  /* reset tx buffer iterator vars */
  txBufferIndex = 0;
  txBufferLength = 0;
  
  begin();
}

void TwoWire::beginTransmission(int address)
{
  beginTransmission((uint8_t)address);
}

//
//	Originally, 'endTransmission' was an f(void) function.
//	It has been modified to take one parameter indicating
//	whether or not a STOP should be performed on the bus.
//	Calling endTransmission(false) allows a sketch to 
//	perform a repeated start. 
//
//	WARNING: Nothing in the library keeps track of whether
//	the bus tenure has been properly ended with a STOP. It
//	is very possible to leave the bus in a hung state if
//	no call to endTransmission(true) is made. Some I2C
//	devices will behave oddly if they do not see a STOP.
//
uint8_t TwoWire::endTransmission(uint8_t sendStop)
{
  // transmit buffer (blocking)
  //int8_t ret = twi_writeTo(txAddress, txBuffer, txBufferLength, 1, sendStop);
  //////////////////////////////////////////////////////////////////////////////
  int8_t ret = 0;

  // Serial1.println(txBufferLength);

  /* Check if data has to be written */
  if (txBufferLength > 0)
  {

    /* Start condition */
    if (0 == i2c_start(hardwareChannel, I2C_TIMEOUT_MAX))
    {

      /* Send address with WRITE enabled */
      if (0 == i2c_addr(hardwareChannel, (txAddress<<1) & (~I2C_READ)))
      {

        /*  RM page 666
         *  read I2C_SR1 followed by I2C_SR2 to to toggle EV6
         *  this action is performed at the end of i2c_addr()
         *  so we don't need to do this right now */
      //  i2c_get_interrupt_status(hardwareChannel, I2C_RXE); // read I2C_SR1
      //  i2c_get_status(hardwareChannel, I2C_MASTER); // read I2C_SR2

        /* Send data using i2c_write() */
        ret = i2c_write(hardwareChannel, txBuffer, txBufferLength);

        /* Stop condition */
        if((ret != 0) && sendStop) i2c_stop(hardwareChannel, I2C_TIMEOUT_MAX);
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////

  /* reset tx buffer iterator vars */
  txBufferIndex = 0;
  txBufferLength = 0;

  /* indicate that we are done transmitting */
  transmitting = 0;

  return ret;
}

//	This provides backwards compatibility with the original
//	definition, and expected behaviour, of endTransmission
//
uint8_t TwoWire::endTransmission(void)
{
  return endTransmission(true);
}

// must be called in:
// slave tx event callback
// or after beginTransmission(address)
size_t TwoWire::write(uint8_t data)
{
  if(transmitting){
  // in master transmitter mode
    // don't bother if buffer is full
    if(txBufferLength >= BUFFER_LENGTH){
        // TODO: implement setWriteError (?)    
//       setWriteError();
      return 0;
    }
    // put byte in tx buffer
    txBuffer[txBufferIndex] = data;
    ++txBufferIndex;
    // update amount in buffer   
    txBufferLength = txBufferIndex;
  }else{
  // in slave send mode
    // reply to master
    // TODO: implement slave mode (?)    
//     twi_transmit(&data, 1);
  }
  return 1;
}

// must be called in:
// slave tx event callback
// or after beginTransmission(address)
size_t TwoWire::write(const uint8_t *data, size_t quantity)
{
  if(transmitting){
  // in master transmitter mode
    for(size_t i = 0; i < quantity; ++i){
      write(data[i]);
    }
  }else{
  // in slave send mode
    // reply to master
    // TODO: implement slave mode (?)
//     twi_transmit(data, quantity);
  }
  return quantity;
}

// must be called in:
// slave rx event callback
// or after requestFrom(address, numBytes)
int TwoWire::available(void)
{
  return rxBufferLength - rxBufferIndex;
}

// must be called in:
// slave rx event callback
// or after requestFrom(address, numBytes)
int TwoWire::read(void)
{
  int value = -1;
  
  // get each successive byte on each call
  if(rxBufferIndex < rxBufferLength){
    value = rxBuffer[rxBufferIndex];
    ++rxBufferIndex;
  }

  return value;
}

// must be called in:
// slave rx event callback
// or after requestFrom(address, numBytes)
int TwoWire::peek(void)
{
  int value = -1;
  
  if(rxBufferIndex < rxBufferLength){
    value = rxBuffer[rxBufferIndex];
  }

  return value;
}

void TwoWire::flush(void)
{
  // XXX: to be implemented.
}

// behind the scenes function that is called when data is received
void TwoWire::onReceiveService(uint8_t* inBytes, int numBytes)
{
  // don't bother if user hasn't registered a callback
  if(!user_onReceive){
    return;
  }
  // don't bother if rx buffer is in use by a master requestFrom() op
  // i know this drops data, but it allows for slight stupidity
  // meaning, they may not have read all the master requestFrom() data yet
  if(rxBufferIndex < rxBufferLength){
    return;
  }
  // copy twi rx buffer into local read buffer
  // this enables new reads to happen in parallel
  for(uint8_t i = 0; i < numBytes; ++i){
    rxBuffer[i] = inBytes[i];    
  }
  // set rx iterator vars
  rxBufferIndex = 0;
  rxBufferLength = numBytes;
  // alert user program
  user_onReceive(numBytes);
}

// behind the scenes function that is called when data is requested
void TwoWire::onRequestService(void)
{
  // don't bother if user hasn't registered a callback
  if(!user_onRequest){
    return;
  }
  // reset tx buffer iterator vars
  // !!! this will kill any pending pre-master sendTo() activity
  txBufferIndex = 0;
  txBufferLength = 0;
  // alert user program
  user_onRequest();
}

// sets function called on slave write
void TwoWire::onReceive( void (*function)(int) )
{
  user_onReceive = function;
}

// sets function called on slave read
void TwoWire::onRequest( void (*function)(void) )
{
  user_onRequest = function;
}

// Preinstantiate Objects //////////////////////////////////////////////////////

TwoWire Wire = TwoWire();

