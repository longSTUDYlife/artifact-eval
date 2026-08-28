/*
 DW1000 ranging demo on radino32 dw1000 modules
 optimized for spiders antenna (DW1000Ranging.h)
 2016 In-Circuit GmbH
 based on the DW1000 library by Thomas Trojer

 for more informations visit:
 http://wiki.in-circuit.de/index.php5?title=dw1000_ranging_demo
*/

#include <stm32/l1/iwdg.h>
#include <SPI.h> 
#include <DW1000Ranging.h>
#include <DW1000Device.h> 

//Set to 1 for the ANCHOR. Else it is a TAG.
//If set to 2 the Pin PIN_READMODE selects mode at bootup. (Pull low for ANCHOR) 
//One ANCHOR can handle up to 6 TAGS. The TAGs need individual short addresses.
#define WE_ARE_ANCHOR      0
#define PIN_READMODE      A1

//Short address used for packet addressing
#define MY_SHORT_ADDRESS   0x1234

//set tx power in dBm, 0 to 33.5, step 0.5
//Respect you local wireless regulations!
//#define DW_TX_POWER      33.5

//If set to 1, the Anchor regularly prints out a table of known devices.
//Else it prints every ranging event seperately
#define PRINT_DEVICE_TABLE  1

//LED toggles on successful measurement. Set to 0xFF to deactivate functionality.
#define PIN_LED 13

//Where to printout information
#define SerialOut if(Serial)Serial
#define SerialOutBegin() Serial.begin()

//Here you can set actions to execute when a Tag gets near an Anchor
//MODULE_NEAR_INIT : Executed during setup()
//MODULE_NEAR_ON : Executed when any tag is nearer than MODULE_NEAR_VALUE meters for at least MODULE_NEAR_TIMEOUT ms
//MODULE_NEAR_OFF : Executed when no tag is nearer than MODULE_NEAR_VALUE meters
#define MODULE_NEAR_VALUE    1.0
#define MODULE_NEAR_TIMEOUT  300
#if 0
  #define MODULE_NEAR_INIT() do{ pinMode(22,OUTPUT); }while(0)
  #define MODULE_NEAR_ON() do{ digitalWrite(22,HIGH); }while(0)
  #define MODULE_NEAR_OFF() do{ digitalWrite(22,LOW); }while(0)
#else
  #define MODULE_NEAR_INIT() do{}while(0)
  #define MODULE_NEAR_ON() do{}while(0)
  #define MODULE_NEAR_OFF() do{}while(0)
#endif

bool weAreAnchor = false;
void setup() {
  pinMode(32, OUTPUT);   //Wakeup=low
  digitalWrite(32, LOW);
  
  if(PIN_LED!=0xFF) pinMode(PIN_LED,OUTPUT);
  if(PIN_LED!=0xFF) digitalWrite(PIN_LED,LOW);
  if (WE_ARE_ANCHOR==2) pinMode(PIN_READMODE, INPUT_PULLUP);
  
  MODULE_NEAR_INIT();
  
  SerialOutBegin();
  delay(1000);
  iwdg_start();
  
  #if WE_ARE_ANCHOR==2
    weAreAnchor = !digitalRead(PIN_READMODE);
  #else
    weAreAnchor = (WE_ARE_ANCHOR==1);
  #endif
  
  //init the configuration
  DW1000Ranging.initCommunication();
  DW1000Ranging.attachNewRange(newRange);
  
  if (weAreAnchor) {
    //Start the module as tag since the library switched naming.
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_IN_CIRCUIT_1, MY_SHORT_ADDRESS);
  } else {
    //Start the module as anchor since the library switched naming.
    DW1000Ranging.startAsAnchor("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_IN_CIRCUIT_1, MY_SHORT_ADDRESS);
  }
  
  #ifdef DW_TX_POWER
    DW1000.setManualTxPower(DW_TX_POWER);
  #endif
}

void loop()
{
  iwdg_reset();
  
#if PRINT_DEVICE_TABLE==1
  static uint32_t lastPrint = millis();
  if (weAreAnchor && (millis()-lastPrint)>500)
  {
    printDeviceTable();
    lastPrint = millis();
  }
#endif
  
  DW1000Ranging.loop();
  
  if (weAreAnchor)
  {
    static uint32_t nearCounter = 0;
    float nearestRange=DW1000Ranging.getNearestRange();
    if ((nearestRange>0) && (nearestRange<MODULE_NEAR_VALUE))
      nearCounter = millis();
    if ((millis()-nearCounter)<MODULE_NEAR_TIMEOUT)
      MODULE_NEAR_ON();
    else
      MODULE_NEAR_OFF();
  }
}

void printDeviceTable()
{
  SerialOut.print("\x1B[2J\x1b[H");
  SerialOut.print("Active");
  SerialOut.print("\t");
  SerialOut.print("Address");
  SerialOut.print("\t");
  SerialOut.print("Range");
  SerialOut.print("\t");
  SerialOut.print("Power");
  SerialOut.print("\r\n");
  
  unsigned int idx=0;
  for( DW1000Device* devicePtr=DW1000Ranging.getDeviceAtIdx(idx++) ; devicePtr!=NULL ; devicePtr=DW1000Ranging.getDeviceAtIdx(idx++) )
  {
    SerialOut.print(!devicePtr->isInactive());
    SerialOut.print("\t");
    SerialOut.print(devicePtr->getShortAddress(), HEX);
    SerialOut.print("\t");
    SerialOut.print(devicePtr->getRange()); SerialOut.print(" m");
    SerialOut.print("\t");
    SerialOut.print(devicePtr->getRXPower()); SerialOut.print(" dBm");
    SerialOut.print("\r\n");
  }
}

void newRange()
{
  if(PIN_LED!=0xFF) digitalWrite(PIN_LED,!digitalRead(PIN_LED));
  if (!weAreAnchor || PRINT_DEVICE_TABLE!=1)
  {
    SerialOut.print("from: "); SerialOut.print(DW1000Ranging.getDistantDevice()->getShortAddress(), HEX);  
    SerialOut.print("\t Range: "); SerialOut.print(DW1000Ranging.getDistantDevice()->getRange()); SerialOut.print(" m"); 
    SerialOut.print("\t RX power: "); SerialOut.print(DW1000Ranging.getDistantDevice()->getRXPower()); SerialOut.println(" dBm");
  }
}
