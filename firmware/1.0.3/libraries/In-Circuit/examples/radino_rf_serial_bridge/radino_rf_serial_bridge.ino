/*
  radino RF Serial Bridge
  for more information: www.in-circuit.de or www.radino.cc

  Copyright (c) 2015 In-Circuit GmbH

  v0.2 - 2015.12.04
    Library based implementation

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

/**********************************************************************************/
/******************************* Main configuration *******************************/
/**********************************************************************************/

/** Select which serial to bridge - possible options listet below **/
#define BRIDGED_SERIAL_SELECTION BRIDGE_HWSERIAL
#define BRIDGED_SERIAL_SPEED 9600

/** Select wireless hardware - possible options listet below **/
#define WIRELESS_HARDWARE CC1101_868_38400_p100

/** Select pin for command mode activation **/
//Internal Pullup is enabled on this pin. Pull low externally to enter command mode
#define COMMAND_MODE_PIN 5  //Comment to disable command mode

/** Signal serial activity on TX/RX LEDs **/
#define USE_SERIALLEDS true

/** Use modified buffer size **/
//#define MY_RADIO_BUFSIZE  4096  //Overide Buffer for data received from radio
//#define MY_UART_BUFSIZE   4096  //Overide Buffer for data received from uart

/******************************* possible options for bridged serial *******************************/
#define BRIDGE_HWSERIAL             1    //Use Serial1
#define BRIDGE_HWSERIAL_RS485PIN2   2    //Use Serial1 and utilize Pin D2 as TX_Enable for RS485 functionality
#define BRIDGE_USBSERIAL            3    //Use USB Serial
#define BRIDGE_SWSERIAL_10_11_12    4    //Softwareserial with RX on D10, TX on D11, TXEN on D12 (e.g. DinRailAdapter)
#define BRIDGE_SWSERIAL_10_A5_12    5    //Softwareserial with RX on D10, TX on DA5, TXEN on D12
#define BRIDGE_HWSERIAL2            6    //Use Serial2
#define BRIDGE_HWSERIAL2_RS485PIN12 7    //Use Serial2 and utilize Pin D12 as TX_Enable for RS485 functionality (e.g. radino RS485 Funkbridge)

/******************************* possible wireless hardware options *******************************/
#define CC1101_868_38400_p100   1    //CC1101 with 868.3 mHz, 38400 baud, +10dBm
#define CC1101_868_38400_m005   2    //CC1101 with 868.3 mHz, 38400 baud, -0.5dBm
#define CC1101_922_38400        3    //CC1101 with 922.5 mHz, 38400 baud, +10dBm
#define CC1101_433_38400        4    //CC1101 with 433.5 mHz, 38400 baud, +10dBm

#define HW_RF233                5    //RF233 basic
#define HW_RF69_443             6    //RF69 with 433 mHz

/****************************************************************************************/
/******************************* Set up hardware specific *******************************/
/****************************************************************************************/
#include <SPI.h>
#include <EEPROM.h>
#if defined ARDUINO_ARCH_AVR
#include <avr/wdt.h>
#elif defined ARDUINO_RADINO32
#include <stm32/l1/iwdg.h>
#else
#error Select a supported board
#endif

/******************************* set serial bridge interface *******************************/
#undef TXEN_PIN
//HardwareSerial 1
#if BRIDGED_SERIAL_SELECTION==BRIDGE_HWSERIAL
#define bridge Serial1
#define bridgeSpeed BRIDGED_SERIAL_SPEED
#define bridgeBurst 10  //Chars written out per call
#define bridgeDelay ((bridgeBurst*1000)/(bridgeSpeed/8))  //Time between calls - Let hardware serial write out async

//RS485 HardwareSerial 1
#elif BRIDGED_SERIAL_SELECTION==BRIDGE_HWSERIAL_RS485PIN2
#define bridge Serial1
#define bridgeSpeed BRIDGED_SERIAL_SPEED
#define bridgeBurst 10  //Chars written out per call
#define bridgeDelay ((bridgeBurst*1000)/(bridgeSpeed/8))  //Time between calls - Let hardware serial write out async
#define TXEN_PIN 2  //Which pin to use for RS485 functionality - comment to disable

//HardwareSerial 2
#elif BRIDGED_SERIAL_SELECTION==BRIDGE_HWSERIAL2
#define bridge Serial2
#define bridgeSpeed BRIDGED_SERIAL_SPEED
#define bridgeBurst 10  //Chars written out per call
#define bridgeDelay ((bridgeBurst*1000)/(bridgeSpeed/8))  //Time between calls - Let hardware serial write out async

//RS485 HardwareSerial 2
#elif BRIDGED_SERIAL_SELECTION==BRIDGE_HWSERIAL2_RS485PIN12
#define bridge Serial2
#define bridgeSpeed BRIDGED_SERIAL_SPEED
#define bridgeBurst 10  //Chars written out per call
#define bridgeDelay ((bridgeBurst*1000)/(bridgeSpeed/8))  //Time between calls - Let hardware serial write out async
#define TXEN_PIN 12  //Which pin to use for RS485 functionality - comment to disable

//RS485 SoftwareSerial on DinRailAdapter
#elif BRIDGED_SERIAL_SELECTION==BRIDGE_SWSERIAL_10_11_12
#include <SoftwareSerial.h>
#define RX_PIN 10
#define TX_PIN 11
SoftwareSerial bridge = SoftwareSerial(RX_PIN, TX_PIN);
#define bridgeSpeed BRIDGED_SERIAL_SPEED
#define bridgeBurst 4  //Chars written out per call
#define bridgeDelay 0 //SofwareSerial writes blocking - no wait necessary
#define TXEN_PIN 12  //Which pin to use for RS485 functionality - comment to disable

//RS485 SoftwareSerial on radino RS485 bridge
#elif BRIDGED_SERIAL_SELECTION==BRIDGE_SWSERIAL_10_A5_12
#include <SoftwareSerial.h>
#define RX_PIN 10
#define TX_PIN A5
SoftwareSerial bridge = SoftwareSerial(RX_PIN, TX_PIN);
#define bridgeSpeed 9600
#define bridgeBurst 4  //Chars written out per call
#define bridgeDelay 0 //SofwareSerial writes blocking - no wait necessary
#define TXEN_PIN 12  //Which pin to use for RS485 functionality - comment to disable

//USB Serial
#elif BRIDGED_SERIAL_SELECTION==BRIDGE_USBSERIAL
#define bridge Serial
#define bridgeSpeed BRIDGED_SERIAL_SPEED
#define bridgeBurst 10  //Chars written out per call
#define bridgeDelay ((bridgeBurst*1000)/(bridgeSpeed/8))  //Time between calls - Let hardware serial write out async

#else
#error You have to select a serial interface

#endif

//Utilize RS485 functionality?
#ifdef TXEN_PIN
#define TXEN_ON() digitalWrite(TXEN_PIN, HIGH)
#define TXEN_OFF() digitalWrite(TXEN_PIN, LOW)
#define TXEN_INIT() pinMode(TXEN_PIN, OUTPUT)
#else
#define TXEN_ON() do{}while(0)
#define TXEN_OFF() do{}while(0)
#define TXEN_INIT() do{}while(0)
#endif

#define UART_TIMEOUT 10  //After this long in ms no chars are received on bridgeSerial it is assumed clear

/******************************* set wireless interface *******************************/
//CC1101 868.3mHz 38400baud +10dB
#if WIRELESS_HARDWARE==CC1101_868_38400_p100
#include <IC_CC1101.h>
#include <IC_CC1101_Bridge.h>
IC_CC1101_Bridge wirelessBridge(868.3, 38400, 0xC0);

//CC1101 868.3mHz 38400baud -0.5dB
#elif WIRELESS_HARDWARE==CC1101_868_38400_m005
#include <IC_CC1101.h>
#include <IC_CC1101_Bridge.h>
IC_CC1101_Bridge wirelessBridge(868.3, 38400, 0x60);

//CC1101 922.5mHz 38400baud +10dB
#elif WIRELESS_HARDWARE==CC1101_922_38400
#include <IC_CC1101.h>
#include <IC_CC1101_Bridge.h>
IC_CC1101_Bridge wirelessBridge(922.5, 38400, 0xC0);

//CC1101 433.5mHz 38400baud +10dB
#elif WIRELESS_HARDWARE==CC1101_433_38400
#include <IC_CC1101.h>
#include <IC_CC1101_Bridge.h>
IC_CC1101_Bridge wirelessBridge(433.5, 38400, 0xC0);

//RF233 868.3mHz
#elif WIRELESS_HARDWARE==HW_RF233
#include <IC_RF233.h>
#include <IC_RF233_Bridge.h>
IC_RF233_Bridge wirelessBridge = IC_RF233_Bridge();

//RF69 433 mHz
#elif WIRELESS_HARDWARE==HW_RF69_443
#include <RFM69.h>
#include <IC_RF69.h>
#include <IC_RF69_Bridge.h>
IC_RF69_Bridge wirelessBridge = IC_RF69_Bridge();

#else
#error You have to select the wireless type

#endif

/******************************* protocol configuration *******************************/
/*
Protocol settings can be modified on the bridged serial after pulling down pin D5
Commands consist of 3 chars at max and are not displayed immediately
Char buffer works as fifo and is interpreted after \r or \n
"n2m55\r" -> command "m55" is executed
"l0\r" -> command "l0" is executed
"sss\n" -> command "s" is executed
"qss\n" -> command "q" is executed
Commands:
"nXX": set network ID to XX (Hex)
"mXX": set my ID to XX (Hex)
"tXX": set target ID to XX (Hex)
"lXX": set broadcast listening (Hex) (0->off, else->on)
"s": store values to eeprom
"r": reload values from eeprom
"p": ping and list devices in range
"q": exit command mode
*/

#define DEFAULT_NWID 0
#define DEFAULT_MYID 0xFF
#define DEFAULT_TGTID 0xFF
#define DEFAULT_ACCBC 1

#define USE_EE_CONFIG 1  //Use EEPROM stored config? Also enables config menu on COMMAND_MODE_PIN low
#define EEPROM_MYSETTINGS_ADDRESS 128 //Starting byte of settings struct in EEPROM
#define EEPROM_CHECKPATTERN 0xA5

struct networkSettingsStruct {
  unsigned char nwid = DEFAULT_NWID;      // Network ID. Only messages from our network are accepted
  unsigned char myid = DEFAULT_MYID;   // Our ID. Messages targeted at our ID are received. Is used as srcId in messages. 0xFF:BroadcastAdress->not ACKed
  unsigned char tgtid = DEFAULT_TGTID;  // Targets ID. Messages are sent to this device. 0xFF:Broadcast
  unsigned char accBC = DEFAULT_ACCBC;     // Are we accepting messages targeted at the Broadcast ID(0xFF)?
  unsigned char chk = EEPROM_CHECKPATTERN;  //let it be the last value -> for checking if ever written settings
} mySettings;

#ifdef MY_UART_BUFSIZE
  char myUartBuf[MY_UART_BUFSIZE];
#endif
#ifdef MY_RADIO_BUFSIZE
  char myRadioBuf[MY_RADIO_BUFSIZE];
#endif

//On witch serial to output the configuration menu
#define cmdSerial bridge
#define cmdSerialSpeed bridgeSpeed

/******************************* helper functions *******************************/
//check if character is valid hex digit
bool ishexch(unsigned char ch)
{
  if (ch >= '0' && ch <= '9')  return true;
  if (ch >= 'a' && ch <= 'f')  return true;
  if (ch >= 'A' && ch <= 'F')  return true;
  return false;
}

//Convert hex char to uint
unsigned char fromhexch(unsigned char ch)
{
  if (ch >= '0' && ch <= '9')  return (ch - '0');
  if (ch >= 'a' && ch <= 'f')  return (ch - 'a' + 10);
  if (ch >= 'A' && ch <= 'F')  return (ch - 'A' + 10);
  return 0;
}

/******************************* config functions *******************************/
//Reload configuration from EEPROM / to default values
bool execReload()
{
#if USE_EE_CONFIG
  if(cmdSerial) cmdSerial.print("Reloading values from EEPROM");
  for (unsigned char i = 0; i < sizeof(struct networkSettingsStruct); i++)
  {
    ((unsigned char *)&mySettings)[i] = EEPROM.read(EEPROM_MYSETTINGS_ADDRESS + i);
  }
  return true;
#else
  if(cmdSerial) cmdSerial.print("Reloading values with defaults");
  mySettings.nwid = DEFAULT_NWID;      // Network ID. Only messages from our network are accepted
  mySettings.myid = DEFAULT_MYID;   // Our ID. Messages targeted at our ID are received. Is used as srcId in messages. 0xFF:BroadcastAdress->not ACKed
  mySettings.tgtid = DEFAULT_TGTID;  // Targets ID. Messages are sent to this device. 0xFF:Broadcast
  mySettings.accBC = DEFAULT_ACCBC;     // Are we accepting messages targeted at the Broadcast ID(0xFF)?
  return true;
#endif
}

//Store configuration to EEPROM
bool execStore()
{
#if USE_EE_CONFIG
  if(cmdSerial) cmdSerial.print("Storing settings to EEPROM");
  //Save to EEPROM
  for (unsigned char i = 0; i < sizeof(struct networkSettingsStruct); i++)
  {
    if (EEPROM.read(EEPROM_MYSETTINGS_ADDRESS + i) != (((unsigned char *)&mySettings)[i]))
      EEPROM.write(EEPROM_MYSETTINGS_ADDRESS + i, ((unsigned char *)&mySettings)[i]);
  }
  return true;
#else
  if(cmdSerial) cmdSerial.print("EEPROM disabled");
  return false;
#endif
}

//Search for other devices
bool execPing()
{
  int8_t pingRes = 0;
  uint8_t otherID;
  float otherRSSI, ourRSSI;

  wirelessBridge.start();
  if(cmdSerial) cmdSerial.print("   \r\nID\tMyRSSI\tTgtRSSI");

  //Ping modules which accept broadcasts
  if (!wirelessBridge.startPing(mySettings.nwid, 0xFF, 0xFF))
  {
    if(cmdSerial) cmdSerial.print("   \r\nError on send (channel jammed?)");
    return false;
  }
  //Wait for answers from other devices
  pingRes = wirelessBridge.checkPingResult(mySettings.nwid, 0xFF, 0xFF, &otherID, &otherRSSI, &ourRSSI);
  while (pingRes != 0)
  {
#if defined ARDUINO_ARCH_AVR
    wdt_enable(WDTO_4S);  //Watchdog 4s
#elif defined ARDUINO_RADINO32
    iwdg_reset();
#endif
    if (pingRes > 0)  //Some device answered
    {
      if(cmdSerial) cmdSerial.print("   \r\n");
      if(cmdSerial) cmdSerial.print((otherID >> 4) & 0xF, HEX);
      if(cmdSerial) cmdSerial.print((otherID >> 0) & 0xF, HEX);
      if(cmdSerial) cmdSerial.print("\t");
      if(cmdSerial) cmdSerial.print(ourRSSI);
      if(cmdSerial) cmdSerial.print("\t");
      if(cmdSerial) cmdSerial.print(otherRSSI);
    }
    pingRes = wirelessBridge.checkPingResult(mySettings.nwid, 0xFF, 0xFF, &otherID, &otherRSSI, &ourRSSI);
  }
  wirelessBridge.stop();
  return true;
}

/******************************* main functions *******************************/
//Initialize the system
void setup()
{
#if defined ARDUINO_RADINO32
  iwdg_set_timeout(256, 578);  // Freq 37000/256: 6.918ms/clock, reload 578 -> 6.918ms*578 = ~4s
  iwdg_start();
  iwdg_reset();
#endif

  //Init bridged serial and TXE for RS485 if required
  bridge.begin(bridgeSpeed);
  if (USE_SERIALLEDS) TX_RX_LED_INIT;
  TXEN_INIT();

  //Should we check for command mode activation
#ifdef COMMAND_MODE_PIN
  pinMode(COMMAND_MODE_PIN, INPUT_PULLUP);
#endif

#if USE_EE_CONFIG
  //Load settings from EEPROM. If invalid -> set defaults
  if ( EEPROM_CHECKPATTERN != EEPROM.read(EEPROM_MYSETTINGS_ADDRESS + (sizeof(struct networkSettingsStruct)) - 1) )
  {
    for (unsigned char i = 0; i < sizeof(struct networkSettingsStruct); i++)
    {
      if (EEPROM.read(EEPROM_MYSETTINGS_ADDRESS + i) != (((unsigned char *)&mySettings)[i]))
      {
        EEPROM.write(EEPROM_MYSETTINGS_ADDRESS + i, ((unsigned char *)&mySettings)[i]);
      }
    }
  } else {
    for (unsigned char i = 0; i < sizeof(struct networkSettingsStruct); i++)
    {
      ((unsigned char *)&mySettings)[i] = EEPROM.read(EEPROM_MYSETTINGS_ADDRESS + i);
    }
  }
#endif

  //Initialize wireless chip
  #ifdef MY_UART_BUFSIZE
    wirelessBridge.setUartBuf(myUartBuf, MY_UART_BUFSIZE);
  #endif
  #ifdef MY_RADIO_BUFSIZE
    wirelessBridge.setRadioBuf(myRadioBuf, MY_RADIO_BUFSIZE);
  #endif
  wirelessBridge.init(mySettings.nwid, mySettings.myid, mySettings.tgtid, mySettings.accBC);
}

//Main loop is called over and over again
void loop()
{
  static unsigned long uart_lastCharIn = 0;  //timestamp when the last char was received on the serial
  static unsigned long uart_lastCharOut = 0;  //timestamp when of last write on serial
  unsigned char crc, cnt;
  unsigned short i;

  //Reset watchdog
#if defined ARDUINO_ARCH_AVR
  wdt_enable(WDTO_4S);  //Watchdog 4s
#elif defined ARDUINO_RADINO32
  iwdg_reset();
#endif

  /************** Command mode - config pin pulled down ****************/
#ifdef COMMAND_MODE_PIN
  static unsigned char cmdMode = 0;  //is command mode active?
  static unsigned long cmdMenuOutput = 0;  //was the information line printed
  static unsigned char cmdBuf[3];  //buffer for incoming chars
  static unsigned char cmdBufPos = 0;  //where in the buffer to store next character
  unsigned char curChar = '\0';
  unsigned char * valToModify = NULL;
  bool cmdOk = false;
  if (cmdMode != 0 || !digitalRead(COMMAND_MODE_PIN))
  {
    if (!cmdMode)  //we just entered command mode
    {
      wirelessBridge.stop();  //shutdown wireless chip as we do not handle incoming data while in command mode
      TXEN_ON();
      if(cmdSerial) cmdSerial.print("\r\nEntering command mode\r\n");
      cmdMenuOutput = cmdBufPos = cmdBuf[0] = cmdBuf[1] = cmdBuf[2] = 0;
      cmdMode = 1;
    }
    //handle incoming chars
    for (cnt = cmdSerial.available(); cmdMenuOutput != 0 && ( cnt || (cnt = cmdSerial.available()) ); cnt--)
    {
      curChar = cmdSerial.read();
      if (curChar != '\r' && curChar != '\n')
      {
        //Shift char into buf
        if (cmdBufPos >= 3)
        {
          cmdBuf[0] = cmdBuf[1];
          cmdBuf[1] = cmdBuf[2];
          cmdBuf[2] = curChar;
        } else {
          cmdBuf[cmdBufPos++] = curChar;
        }
      } else {
        cnt--;
        TXEN_ON();
        //Trash remaining chars (e.g. Windows \r\n)
        do {
          delay(((cmdSerialSpeed / 8) / 1000) + 1);
          while (cnt--) cmdSerial.read();
        } while ( (cnt = cmdSerial.available()) );

        //print out current buf content
        if(cmdSerial) cmdSerial.write(cmdBuf[0]);
        if(cmdSerial) cmdSerial.write(cmdBuf[1]);
        if(cmdSerial) cmdSerial.write(cmdBuf[2]);
        if(cmdSerial) cmdSerial.println("   ");
        if(cmdSerial) cmdSerial.write(cmdBuf[0]);
        if(cmdSerial) cmdSerial.print(": ");
        valToModify = NULL;
        cmdOk = false;
        //Start command handling
        switch (cmdBuf[0])
        {
          case'n': case'N': valToModify = &mySettings.nwid; if(cmdSerial) cmdSerial.print("Setting network ID"); break;
          case'm': case'M': valToModify = &mySettings.myid; if(cmdSerial) cmdSerial.print("Setting my ID"); break;
          case't': case'T': valToModify = &mySettings.tgtid; if(cmdSerial) cmdSerial.print("Setting target ID"); break;
          case'l': case'L': valToModify = &mySettings.accBC; if(cmdSerial) cmdSerial.print("Setting listen broadcast"); break;
          case'p': case'P': if(cmdSerial) cmdSerial.print("Pinging neighbours"); cmdOk = execPing(); break;
          case'r': case'R': cmdOk = execReload(); break;
          case'q': case'Q': if(cmdSerial) cmdSerial.print("Quitting command mode"); cmdOk = true; cmdMode = 0; break;
          case's': case'S': cmdOk = execStore(); break;
          default: if(cmdSerial) cmdSerial.print("Unknown command"); break;
        }
        if(cmdSerial) cmdSerial.println("   ");
        if (valToModify != NULL)
        {
          if (cmdBufPos < 2)  //got only single char command
          {
            if(cmdSerial) cmdSerial.print("No value given");
          } else {
            if(cmdSerial) cmdSerial.write(cmdBuf[1]);
            if (cmdBufPos > 2) if(cmdSerial) cmdSerial.write(cmdBuf[2]);
            if(cmdSerial) cmdSerial.print(": ");
            if ( !ishexch(cmdBuf[1]) || (cmdBufPos > 2 && !ishexch(cmdBuf[2])) )
            {
              if(cmdSerial) cmdSerial.print("Invalid value");  //got non hex param
            } else {
              *valToModify = fromhexch(cmdBuf[1]);
              if (cmdBufPos > 2)
              {
                *valToModify <<= 4;
                *valToModify += fromhexch(cmdBuf[2]);
              }
              if (valToModify == &mySettings.accBC)
                *valToModify = *valToModify ? 1 : 0;
              if(cmdSerial) cmdSerial.print("New value: ");
              if (valToModify != &mySettings.accBC)
                if(cmdSerial) cmdSerial.print(((*valToModify) >> 4) & 0xF, HEX);
              if(cmdSerial) cmdSerial.print(((*valToModify) >> 0) & 0xF, HEX);
              cmdOk = true;
            }
          }
          if(cmdSerial) cmdSerial.println("   ");
        }
        if (cmdOk)  //command handled successfully
          if(cmdSerial) cmdSerial.println("OK");
        else  //error occured during command handling
          if(cmdSerial) cmdSerial.println("NOK");
        cmdMenuOutput = cmdBuf[0] = cmdBuf[1] = cmdBuf[2] = cmdBufPos = 0;
      }
    }  //for cmdSerial available
    if (cmdMode == 0)  //we left command mode
    {
      TXEN_ON();
      //Propagate new configuration to hardware library
      wirelessBridge.setConfig(mySettings.nwid, mySettings.myid, mySettings.tgtid, mySettings.accBC);
      wirelessBridge.start();
      cmdSerial.flush();
      TXEN_OFF();
      return;
    }
    if (cmdMenuOutput == 0)  //a printout of the info line was scheduled
    {
      TXEN_ON();
      cmdMenuOutput = 1;
      if(cmdSerial) cmdSerial.print("N)wID:");
      if(cmdSerial) cmdSerial.print(((mySettings.nwid >> 4) & 0xF), HEX);
      if(cmdSerial) cmdSerial.print(((mySettings.nwid >> 0) & 0xF), HEX);
      if(cmdSerial) cmdSerial.print("  M)yID:");
      if(cmdSerial) cmdSerial.print(((mySettings.myid >> 4) & 0xF), HEX);
      if(cmdSerial) cmdSerial.print(((mySettings.myid >> 0) & 0xF), HEX);
      if(cmdSerial) cmdSerial.print("  T)gtID:");
      if(cmdSerial) cmdSerial.print(((mySettings.tgtid >> 4) & 0xF), HEX);
      if(cmdSerial) cmdSerial.print(((mySettings.tgtid >> 0) & 0xF), HEX);
      if(cmdSerial) cmdSerial.print("  L)istenBC:");
      if(cmdSerial) cmdSerial.print(mySettings.accBC ? 1 : 0);
      if(cmdSerial) cmdSerial.print("  S)tore");
      if(cmdSerial) cmdSerial.print("  P)ing");
      if(cmdSerial) cmdSerial.print("  R)eload");
      if(cmdSerial) cmdSerial.print("  Q)uit");
      if(cmdSerial) cmdSerial.print("\r\n> ");
      if(cmdSerial) cmdSerial.flush();
      TXEN_OFF();
    }
    return;
  }
#endif
  /************** End command mode *********************/

  //let the hardware library transmit/receive data over wireless
  wirelessBridge.handle();


  /************** radio to UART ************************/

  //Buffer based output of incoming packets
  //On RS485 check if no chars were received since UART_TIMEOUT ms (bus should be clear to send)
  //writes bridgeBurst chars every bridgeDelay ms (Let the hardware serial write out async - important for less blocking RS485 functionality)
  uint8_t tmpChar = 0;
#ifdef TXEN_PIN
  if ((millis() - uart_lastCharIn) > UART_TIMEOUT && wirelessBridge.available() && (millis() - uart_lastCharOut) > bridgeDelay)
#else
  if (wirelessBridge.available() && (millis() - uart_lastCharOut) > bridgeDelay)
#endif
  {
    if (USE_SERIALLEDS) TXLED1;
    TXEN_ON();
    uart_lastCharOut = millis();
    for ( i = 0 ; i < bridgeBurst && wirelessBridge.available() > 1 ; i++ )
    {
      tmpChar = wirelessBridge.read();
      if (bridge) bridge.write(tmpChar);
    }
    if (i == 0 && wirelessBridge.available() == 1) //This is the last char
    {
      tmpChar = wirelessBridge.read();
      if (bridge) bridge.write(tmpChar);
#ifdef TXEN_PIN
      if (bridge) bridge.flush();  //wait for tx empty
#endif
      TXEN_OFF();
    }
  }
  if (USE_SERIALLEDS && ((millis() - uart_lastCharOut) > 25)) TXLED0;


  /************** UART to radio ************************/

  //read incoming chars from UART
  if (bridge)
  {
    for (cnt = bridge.available() ; wirelessBridge.txBufFree() && ( cnt  || (cnt = bridge.available())) ; cnt--)
    {
      if (USE_SERIALLEDS) RXLED1;
      wirelessBridge.write(bridge.read());
      uart_lastCharIn = millis();
    }
  }
  if (USE_SERIALLEDS && ((millis() - uart_lastCharIn) > 25)) RXLED0;
}

