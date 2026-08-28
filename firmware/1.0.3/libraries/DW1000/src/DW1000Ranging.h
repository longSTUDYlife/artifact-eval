/*
 * Copyright (c) 2015 by Thomas Trojer <thomas@trojer.net> and Leopold Sayous <leosayous@gmail.com>
 * Modified 2016 for radino32 compatibility by In-Circuit GmbH
 * 
 * Decawave DW1000 library for arduino.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file DW1000Ranging.h
 * Arduino global library (header file) working with the DW1000 library
 * for the Decawave DW1000 UWB transceiver IC.
 */

#include "DW1000.h"
#include "DW1000Time.h"
#include "DW1000Device.h" 
#include "DW1000Mac.h"

// Factor for still unresolved calculation flaws
//leonardo
//#define IC_ANTENNA_RANGE_CORNEAR(x) ( x += (x<2) ? (2-x)*(0.167f) : 0 )
//spider
#define IC_ANTENNA_RANGE_CORNEAR(x) (x)
//#define IC_ANTENNA_RANGE_CORFACT (0.953f)
#define IC_ANTENNA_RANGE_CORFACT (1.0f)
// Ranging error correction for antenna layout on in-circuit boards
// leaonardo to leonardo: -0.61m
// spider to spider: -0.15m
//#define IC_ANTENNA_RANGE_OFFSET (-0.61f)
// #define IC_ANTENNA_RANGE_OFFSET (0.047f)
#define IC_ANTENNA_RANGE_OFFSET (0.0f)
//#define IC_ANTENNA_RANGE_OFFSET (-19.75f)

// messages used in the ranging protocol
#define POLL 0
#define POLL_ACK 1
#define RANGE 2
#define RANGE_REPORT 3
#define RANGE_FAILED 255
#define BLINK 4
#define RANGING_INIT 5
#define NULL_PACKET 6

#define LEN_DATA 116

//Max devices we put in the networkDevices array ! Each DW1000Device is 74 Bytes in SRAM memory for now.
#define MAX_DEVICES 6

//Default Pin for module:
#define DEFAULT_RST_PIN 28
#define DEFAULT_SPI_SS_PIN 29
#define DEFAULT_SPI_IRQ_PIN 33

//Default value
//in ms
#define DEFAULT_RESET_PERIOD 200
//in us
#define DEFAULT_REPLY_DELAY_TIME 3000UL
#define DEFAULT_REPLY_DELAY_TIME_normal 3000UL

//sketch type (anchor or tag)
#define TAG 0
#define ANCHOR 1
#define LISTENER 2

//default timer delay
#define DEFAULT_TIMER_DELAY 14
 

//debug mode
#ifndef DEBUG
#define DEBUG false
#endif


class DW1000RangingClass {
  public:
    //variables 
    // data buffer
    static byte data[LEN_DATA];
    
    
    //initialisation
    static void initCommunication(unsigned int mySS=DEFAULT_SPI_SS_PIN, unsigned int myIRQ=DEFAULT_SPI_IRQ_PIN, unsigned int myRST=DEFAULT_RST_PIN );
    static void configureNetwork(unsigned int deviceAddress, unsigned int networkId, const byte mode[]); 
    static void generalStart();
    static void startAsAnchor(char address[], const byte mode[], unsigned short myShortAddress);
    static void startAsTag(char address[], const byte mode[], unsigned short myShortAddress);
    static void startAsListener(char address[], const byte mode[], unsigned short myShortAddress);
    static boolean addNetworkDevices(DW1000Device *device, boolean shortAddress);
    static boolean addNetworkDevices(DW1000Device *device);
    static void removeNetworkDevices(short index);
    
    //setters
    static void setReplyTime(unsigned long replyDelayTimeUs);
    static void setResetPeriod(unsigned long resetPeriod);
    
    //getters 
    static byte* getCurrentAddress(){return _currentAddress; };
    static byte* getCurrentShortAddress(){return _currentShortAddress; };
    static short getNetworkDevicesNumber(){return _networkDevicesNumber; };
    
    //ranging functions
    static short detectMessageType(byte datas[]);
    static void loop();
    
    
    //Handlers:
    static void attachNewRange(void (*handleNewRange)(void)) {_handleNewRange = handleNewRange; };
    
    float getNearestRange();
    DW1000Device* getDeviceAtIdx(unsigned int idx);
    static DW1000Device* getDistantDevice();
    static DW1000Device* searchDistantDevice(byte shortAddress[]);
    
    //FOR DEBUGGING
    static void visualizeDatas(byte datas[]);
    
        
  private: 
    //other devices in the network
    static DW1000Device _networkDevices[MAX_DEVICES];
    static short _networkDevicesNumber;
    static short _lastDistantDevice;
    static byte _currentAddress[8];
    static byte _currentShortAddress[2];
    static byte _lastSentToShortAddress[2]; 
    static DW1000Mac _globalMac;
    static unsigned long timer;
    static short counterForBlink;
    static uint16_t _blinkSeq;  
    
    //Handlers:
    static void (*_handleNewRange)(void);
    
    //sketch type (tag or anchor)
    static int _type; //0 for tag and 1 for anchor
    // message flow state
    static volatile byte _expectedMsgId;
    // message sent/received state
    static volatile boolean _sentAck;
    static volatile boolean _receivedAck;
    // protocol error state
    static boolean _protocolFailed; 
    // reset line to the chip
    static unsigned int _RST;
    static unsigned int _SS;
    static unsigned int _IRQ;
    // watchdog and reset period
    static unsigned long _lastActivity;
    static unsigned long _resetPeriod;
    // reply times (same on both sides for symm. ranging)
    static unsigned long _replyDelayTimeUS;
    //timer Tick delay
    static unsigned int _timerDelay;
    // ranging counter (per second)
    static unsigned int _successRangingCount;
    static unsigned long _rangingCountPeriod;
    
    //_bias correction
    static char _bias_RSL[17];
    //17*2=34 bytes in SRAM
    static short _bias_PRF_16[17];
    //17 bytes in SRAM
    static char _bias_PRF_64[17];

    static uint8_t _pollSeq;
    
    //methods
    static void handleSent();
    static void handleReceived();
    static void noteActivity();
    static void resetInactive();
    
    //global functions:
    static void checkForReset();
    static void checkForInactiveDevices();
    static void copyShortAddress(byte address1[],byte address2[]);
    
    //for ranging protocole (ANCHOR)
    static void transmitInit();
    static void transmit(byte datas[]);
    static void transmit(byte datas[], DW1000Time time);
    static void transmitBlink();
    static void transmitRangingInit(DW1000Device *myDistantDevice);
    static void transmitPollAck(DW1000Device *myDistantDevice);
    static void transmitRangeReport(DW1000Device *myDistantDevice);
    static void transmitRangeFailed(DW1000Device *myDistantDevice);
    static void receiver();
    
    //for ranging protocole (TAG)
    static void transmitPoll(DW1000Device *myDistantDevice);
    static void transmitRange(DW1000Device *myDistantDevice);
    static void transmitNullPacket(DW1000Time delayTime);
    
    //methods for range computation
    static void computeRangeAsymmetric(DW1000Device *myDistantDevice, DW1000Time *myTOF);
    
    static void timerTick();
     

};

extern DW1000RangingClass DW1000Ranging;
