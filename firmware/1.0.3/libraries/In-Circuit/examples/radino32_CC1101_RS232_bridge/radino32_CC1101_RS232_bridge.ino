/*
  Copyright (c) 2017 In-Circuit GmbH

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

// libraries for radino32
#include <SPI.h>
#include <EEPROM.h>
#include <stm32/l1/iwdg.h>

// libraries for CC1101
#include <IC_CC1101.h>
#include <IC_CC1101_Bridge.h>

#define CC1101_FREQUENCY    868.3    // 868.3 for radino32 CC1101 868MHz (901.356A001)
                                     // 433.5 for radino32 CC1101 433MHz (901.356)

IC_CC1101_Bridge wirelessBridge(CC1101_FREQUENCY, 38400, 0xC0);	// set wireless interface

#define bridge           Serial1				// Serial for USB-UART, Serial1 for HW-UART
#define bridgeSpeed      9600					// Baudrate
#define bridgeBurst      10  					// Chars written out per call
#define bridgeDelay      ((bridgeBurst*1000)/(bridgeSpeed/8))   // Time between calls - Let hardware serial write out async

#define DEFAULT_NWID   0               // Network ID. Only messages from our network are accepted
#define DEFAULT_MYID   0xFF	       // Our ID. Messages targeted at our ID are received. Is used as srcId in messages. 0xFF:BroadcastAdress->not ACKed
#define DEFAULT_TGTID  0xFF            // Targets ID. Messages are sent to this device. 0xFF:Broadcast
#define DEFAULT_ACCBC  1	       // Are we accepting messages targeted at the Broadcast ID(0xFF)?

struct networkSettingsStruct {
  unsigned char nwid = DEFAULT_NWID;
  unsigned char myid = DEFAULT_MYID;
  unsigned char tgtid = DEFAULT_TGTID;
  unsigned char accBC = DEFAULT_ACCBC;
} mySettings;

void setup() {
  bridge.begin(bridgeSpeed);
  
  //Initialize wireless chip
  wirelessBridge.init(mySettings.nwid, mySettings.myid, mySettings.tgtid, mySettings.accBC);
}

void loop() {
  wirelessBridge.handle();

  static unsigned long uart_lastCharIn = 0;  	// Timestamp when the last char was received on serial
  static unsigned long uart_lastCharOut = 0;    // Timestamp when of last write on serial
  unsigned char cnt;
  unsigned short i;

  uint8_t tmpChar = 0;

  /************** radio to UART ************************/
  if (wirelessBridge.available() && (millis() - uart_lastCharOut) > bridgeDelay)
  {
    uart_lastCharOut = millis();
    for ( i = 0 ; i < bridgeBurst && wirelessBridge.available() > 1 ; i++ )
    {
      tmpChar = wirelessBridge.read();
      if (bridge) bridge.write(tmpChar);
    }
    if (i == 0 && wirelessBridge.available() == 1)   // This is the last char
    {
      tmpChar = wirelessBridge.read();
      if (bridge) bridge.write(tmpChar);
    }
  }

  /************** UART to radio ************************/
  if (bridge)
  {
    for (cnt = bridge.available() ; wirelessBridge.txBufFree() && ( cnt  || (cnt = bridge.available())) ; cnt--)
    {
      tmpChar = bridge.read();
      wirelessBridge.write(tmpChar);
      uart_lastCharIn = millis();
    }
  }
}