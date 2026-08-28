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
 * Arduino global library (source file) working with the DW1000 library 
 * for the Decawave DW1000 UWB transceiver IC.
 */
 

#include "DW1000Ranging.h"
#include "DW1000Device.h"

DW1000RangingClass DW1000Ranging;

 
//other devices we are going to communicate with which are on our network:
DW1000Device DW1000RangingClass::_networkDevices[MAX_DEVICES];
byte DW1000RangingClass::_currentAddress[8];
byte DW1000RangingClass::_currentShortAddress[2];
byte DW1000RangingClass::_lastSentToShortAddress[2];
short DW1000RangingClass::_networkDevicesNumber=0;
short DW1000RangingClass::_lastDistantDevice=0;
DW1000Mac DW1000RangingClass::_globalMac;

//module type (anchor or tag)
int DW1000RangingClass::_type;
// message flow state
volatile byte DW1000RangingClass::_expectedMsgId;
// message sent/received state
volatile boolean DW1000RangingClass::_sentAck=false;
volatile boolean DW1000RangingClass::_receivedAck=false;
// protocol error state
boolean DW1000RangingClass::_protocolFailed=false;
// timestamps to remember
unsigned long DW1000RangingClass::timer=0;
short DW1000RangingClass::counterForBlink=0;


// data buffer
byte DW1000RangingClass::data[LEN_DATA];
// reset line to the chip
unsigned int DW1000RangingClass::_RST;
unsigned int DW1000RangingClass::_SS;
unsigned int DW1000RangingClass::_IRQ;
// watchdog and reset period
unsigned long DW1000RangingClass::_lastActivity;
unsigned long DW1000RangingClass::_resetPeriod;
// reply times (same on both sides for symm. ranging)
unsigned long DW1000RangingClass::_replyDelayTimeUS;
//timer delay
unsigned int DW1000RangingClass::_timerDelay;
// ranging counter (per second)
unsigned int DW1000RangingClass::_successRangingCount=0;
unsigned long DW1000RangingClass::_rangingCountPeriod=0;
//Here our handlers
void (*DW1000RangingClass::_handleNewRange)(void) = 0;

static boolean ledState_tx = LOW;
static boolean ledState_rx = LOW;

int32_t cri_value = 0;
float ppm_debug_cri=0;
int trim_index;
bool _sensingMode = true;
uint8_t DW1000RangingClass::_pollSeq = 0;
uint16_t DW1000RangingClass::_blinkSeq=0;

byte temp[3] = {1,2,3};
/* ###########################################################################
 * #### Init and end #######################################################
 * ######################################################################### */

void DW1000RangingClass::initCommunication(unsigned int mySS, unsigned int myIRQ,unsigned int myRST){
    // reset line to the chip
    _RST = myRST;
    _SS = mySS;
    _IRQ = myIRQ;
    _resetPeriod = DEFAULT_RESET_PERIOD;
    // reply times (same on both sides for symm. ranging)
    _replyDelayTimeUS = DEFAULT_REPLY_DELAY_TIME;
    //we set our timer delay
    _timerDelay = DEFAULT_TIMER_DELAY;
    
    
    DW1000.begin(_SS, _IRQ, _RST);
    DW1000.select(_SS);
}
 

void DW1000RangingClass::configureNetwork(unsigned int deviceAddress, unsigned int networkId, const byte mode[]){
    // general configuration
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(deviceAddress);
    DW1000.setNetworkId(networkId);
    DW1000.enableMode(mode);
    DW1000.commitConfiguration();
    
}

void DW1000RangingClass::generalStart(){
    // attach callback for (successfully) sent and received messages
    DW1000.attachSentHandler(handleSent);
    DW1000.attachReceivedHandler(handleReceived);
    // anchor starts in receiving mode, awaiting a ranging poll message
    
    
    if(DEBUG){
        // DEBUG monitoring
        dw1000Serial.println("DW1000-arduino");
        // initialize the driver
        
        
        dw1000Serial.println("configuration..");
        // DEBUG chip info and registers pretty printed
        char msg[90];
        DW1000.getPrintableDeviceIdentifier(msg);
        dw1000Serial.print("Device ID: "); dw1000Serial.println(msg);
        DW1000.getPrintableExtendedUniqueIdentifier(msg);
        dw1000Serial.print("Unique ID: "); dw1000Serial.print(msg);
        char string[6];
        sprintf(string, "%02X:%02X", _currentShortAddress[0], _currentShortAddress[1]);
        dw1000Serial.print(" short: ");dw1000Serial.println(string);
        
        DW1000.getPrintableNetworkIdAndShortAddress(msg);
        dw1000Serial.print("Network ID & Device Address: "); dw1000Serial.println(msg);
        DW1000.getPrintableDeviceMode(msg);
        dw1000Serial.print("Device mode: "); dw1000Serial.println(msg);
    }
    
    
    // anchor starts in receiving mode, awaiting a ranging poll message
    receiver();
    // for first time ranging frequency computation
    _rangingCountPeriod = millis();
    noteActivity();
}


void DW1000RangingClass::startAsAnchor(char address[],  const byte mode[], unsigned short myShortAddress){
    // DW1000.setXtalTrim5bit(19);
    //save the address
    DW1000.convertToByte(address, _currentAddress);
    //write the address on the DW1000 chip
    DW1000.setEUI(address);
    //dw1000Serial.print("device address: ");
    //dw1000Serial.println(address);
    _currentShortAddress[0]=((myShortAddress>>0)&0xFF);
    _currentShortAddress[1]=((myShortAddress>>8)&0xFF);
    
    //we configur the network for mac filtering
    //(device Address, network ID, frequency)
    DW1000Ranging.configureNetwork(_currentShortAddress[0]*256+_currentShortAddress[1], 0xDECA, mode);
    
    //general start:
    DW1000.setXtalTrim5bit(15);  
    generalStart();
    
    //defined type as anchor
    _type=ANCHOR;
    
    //dw1000Serial.println("### ANCHOR ###");
    
}

void DW1000RangingClass::startAsTag(char address[],  const byte mode[], unsigned short myShortAddress){
    //save the address
    DW1000.convertToByte(address, _currentAddress);
    //write the address on the DW1000 chip
    DW1000.setEUI(address);
    //dw1000Serial.print("device address: ");
    //dw1000Serial.println(address);
    _currentShortAddress[0]=((myShortAddress>>0)&0xFF);
    _currentShortAddress[1]=((myShortAddress>>8)&0xFF);
    
    //we configur the network for mac filtering
    //(device Address, network ID, frequency)
    DW1000Ranging.configureNetwork(_currentShortAddress[0]*256+_currentShortAddress[1], 0xDECA, mode);
    DW1000.setXtalTrim5bit(15);  
    generalStart();
    
    //defined type as anchor
    _type=TAG;
    //_pollSeq = 0;
    
    //dw1000Serial.println("### TAG ###");
}

void DW1000RangingClass::startAsListener(char address[],  const byte mode[], unsigned short myShortAddress){
    //save the address
    DW1000.convertToByte(address, _currentAddress);
    //write the address on the DW1000 chip
    DW1000.setEUI(address);
    _currentShortAddress[0]=((myShortAddress>>0)&0xFF);
    _currentShortAddress[1]=((myShortAddress>>8)&0xFF);
    DW1000Ranging.configureNetwork(_currentShortAddress[0]*256+_currentShortAddress[1], 0xDECA, mode);
    DW1000.setXtalTrim5bit(15);  

    //generalStart();

    DW1000.attachSentHandler(handleSent);
    DW1000.attachReceivedHandler(handleReceived);

    // DW1000.setFrameFilter(false);
    // DW1000.suppressFrameCheck(true);
    DW1000.receivePermanently(true);
    //DW1000.setDoubleBuffering(true);
    //DW1000.writeSystemConfigurationRegister();
    //receiver();
    //DW1000.receivePermanently(true);
    //defined type as anchor
    DW1000.newReceive();
    DW1000.startReceive();
    _type=LISTENER;
    
    //dw1000Serial.println("### TAG ###");
}



boolean DW1000RangingClass::addNetworkDevices(DW1000Device *device, boolean shortAddress)
{
    boolean addDevice=true;
    //we test our network devices array to check
    //we don't already have it
    for(short i=0; i<_networkDevicesNumber; i++){
        if(_networkDevices[i].isAddressEqual(device) && !shortAddress)
        {
            //the device already exists
            addDevice=false;
            return false;
        }
        else if(_networkDevices[i].isShortAddressEqual(device) && shortAddress)
        {
            //the device already exists
            addDevice=false;
            return false;
        }
        
    }
    
    if(addDevice)
    {
        memcpy(&_networkDevices[_networkDevicesNumber], device, sizeof(DW1000Device));
        _networkDevices[_networkDevicesNumber].setIndex(_networkDevicesNumber);
        _networkDevicesNumber++;
        return true;
    }
    
    return false;
}

boolean DW1000RangingClass::addNetworkDevices(DW1000Device *device)
{
    boolean addDevice=true;
    //we test our network devices array to check
    //we don't already have it
    for(short i=0; i<_networkDevicesNumber; i++){
        if(_networkDevices[i].isAddressEqual(device) && _networkDevices[i].isShortAddressEqual(device))
        {
            //the device already exists
            addDevice=false;
            return false;
        }
        
    }
    
    if(addDevice)
    {
        if(_type==ANCHOR) //for now let's start with 1 TAG
        {
            _networkDevicesNumber=0;
        }
        memcpy(&_networkDevices[_networkDevicesNumber], device, sizeof(DW1000Device));
        _networkDevices[_networkDevicesNumber].setIndex(_networkDevicesNumber);
        _networkDevicesNumber++;
        return true;
    }
    
    return false;
}

void DW1000RangingClass::removeNetworkDevices(short index){
    //if we have just 1 element
    if(_networkDevicesNumber==1){
        _networkDevicesNumber=0;
    }
    else if(index==_networkDevicesNumber-1) //if we delete the last element
    {
        _networkDevicesNumber--;
    }
    else
    {
        //we translate all the element wich are after the one we want to delete.
        for(int i=index; i<_networkDevicesNumber-1; i++)
        {
            memcpy(&_networkDevices[i], &_networkDevices[i+1], sizeof(DW1000Device));
            _networkDevices[i].setIndex(i);
        }
        _networkDevicesNumber--;
    }
}

/* ###########################################################################
 * #### Setters and Getters ##################################################
 * ######################################################################### */

//setters
void DW1000RangingClass::setReplyTime(unsigned long replyDelayTimeUs){ _replyDelayTimeUS=replyDelayTimeUs;}
void DW1000RangingClass::setResetPeriod(unsigned long resetPeriod){ _resetPeriod=resetPeriod;}
 


DW1000Device* DW1000RangingClass::searchDistantDevice(byte shortAddress[]){
    
    
    //we compare the 2 bytes address with the others
    for(int i=0; i<_networkDevicesNumber; i++)
    {
        if(memcmp(shortAddress, _networkDevices[i].getByteShortAddress(), 2)==0)
        {
            //we have found our device !
            return &_networkDevices[i];
        }
    }
    
    return NULL;
}

DW1000Device* DW1000RangingClass::getDistantDevice(){
    //we get the device which correspond to the message which was sent (need to be filtered by MAC address)

    return &_networkDevices[_lastDistantDevice];
    
}




/* ###########################################################################
 * #### Public methods #######################################################
 * ######################################################################### */

float DW1000RangingClass::getNearestRange()
{
  float curNearest = 0;
  for(int i=0; i<_networkDevicesNumber; i++)
  {
    if (_networkDevices[i].isInactive()) continue;
    if (_networkDevices[i].getRange()<=0) continue;
    if ((curNearest==0) || (_networkDevices[i].getRange()<curNearest))
      curNearest = _networkDevices[i].getRange();
  }
  return curNearest;
}

DW1000Device* DW1000RangingClass::getDeviceAtIdx(unsigned int idx)
{
  if (idx>=_networkDevicesNumber) return NULL;
  return &_networkDevices[idx];
}

void DW1000RangingClass::checkForReset(){
    unsigned long curMillis = millis();
    if(!_sentAck && !_receivedAck) {
        // check if inactive
        if(curMillis - _lastActivity > _resetPeriod) {
            resetInactive();
        }
        return;
    }
}

void DW1000RangingClass::checkForInactiveDevices(){
    for(int i=0; i<_networkDevicesNumber; i++){
        if(_networkDevices[i].isInactive()){
            dw1000Serial.print("delete inactive device: ");
            dw1000Serial.println(_networkDevices[i].getShortAddress(), HEX);
            //we need to delete the device from the array:
            removeNetworkDevices(i);
            
        }
    }
}

short DW1000RangingClass::detectMessageType(byte datas[]){
    if(datas[0]==0xC5)
    {
        return BLINK;
    }
    else if(datas[0]==FC_1 && datas[1]==FC_2)
    {
        //we have a long MAC frame message (ranging init)
        return datas[LONG_MAC_LEN];
    }
    else if(datas[0]==FC_1 && datas[1]==FC_2_SHORT)
    {
        //we have a short mac frame message (poll, range, range report, etc..)
        return datas[SHORT_MAC_LEN];
    }  
}

void DW1000RangingClass::loop(){
    //we check if needed to reset !
    checkForReset();
    unsigned long time=millis();
    if(time-timer>_timerDelay){
        timer=time;
      if (_expectedMsgId == POLL_ACK)
      {
        _expectedMsgId = RANGE_REPORT;
        //at least one of our partners did not respond in time transmit the next message
        //transmitRange(NULL);
      } else {
        timerTick();
      }
    }
    
    if(_sentAck){
        _sentAck = false;
        
        
        int messageType=detectMessageType(data);
        
        if(messageType!=POLL_ACK && messageType!= POLL && messageType!=RANGE)
            return;
        
        
        //A msg was sent. We launch the ranging protocole when a message was sent
        if(_type==ANCHOR){
            if(messageType == POLL_ACK) {
                DW1000Device *myDistantDevice=searchDistantDevice(_lastSentToShortAddress);
                
                DW1000.getTransmitTimestamp(myDistantDevice->timePollAckSent);
            }
        }
        else if(_type==TAG){
            if(messageType == POLL) {
                DW1000Time timePollSent;
                DW1000.getTransmitTimestamp(timePollSent);
                //if the last device we send the POLL is broadcast:
                if(_lastSentToShortAddress[0]==0xFF && _lastSentToShortAddress[1]==0xFF)
                {
                    //we save the value for all the devices !
                    for(short i=0; i<_networkDevicesNumber; i++){
                        _networkDevices[i].timePollSent=timePollSent;
                    }
                }
                else
                {
                    //we search the device associated with the last send address
                    DW1000Device *myDistantDevice=searchDistantDevice(_lastSentToShortAddress);
                    //we save the value just for one device
                    myDistantDevice->timePollSent=timePollSent;
                }
            }
            else if(messageType == RANGE_REPORT){
                //DW1000.useSmartPower(true);
                DW1000.setManualTxPower(33.5);
            }
            else if(messageType == RANGE) {
                DW1000Time timeRangeSent;
                DW1000.getTransmitTimestamp(timeRangeSent);
                DW1000Time baseDelay = DW1000Time(_replyDelayTimeUS, DW_MICROSECONDS);
                DW1000Time delay200ns = DW1000Time((unsigned long)150, DW_NANOSECONDS);
                DW1000Time totalDelay = timeRangeSent + baseDelay + delay200ns;
                transmitNullPacket(totalDelay);
                //if the last device we send the POLL is broadcast:
                if(_lastSentToShortAddress[0]==0xFF && _lastSentToShortAddress[1]==0xFF)
                {
                    //Serial.println("RANGE sent");
                    //we save the value for all the devices !
                    for(short i=0; i<_networkDevicesNumber; i++){
                        _networkDevices[i].timeRangeSent=timeRangeSent;
                    }
                }
                else
                {
                    //we search the device associated with the last send address
                    
                    DW1000Device *myDistantDevice=searchDistantDevice(_lastSentToShortAddress);
                    //we save the value just for one device
                    myDistantDevice->timeRangeSent=timeRangeSent;
                    // DW1000Time baseDelay = DW1000Time(50000, DW_MICROSECONDS);
                    // DW1000Time delay200ns = DW1000Time(150, DW_NANOSECONDS);
                    // DW1000Time totalDelay = myDistantDevice->timeRangeSent + baseDelay + delay200ns;
                    // transmitNullPacket(totalDelay);
                }

            }
        }
        
    }
    
    //check for new received message
    if(_receivedAck){
        _receivedAck=false;
        
        
        //we read the datas from the modules:
        // get message and parse
        DW1000.getData(data, LEN_DATA);
        
        
        int messageType=detectMessageType(data);
        
        //we have just received a BLINK message from tag
        if(messageType==BLINK && _type==ANCHOR){ 
            byte address[8];
            byte shortAddress[2];
            _globalMac.decodeBlinkFrame(data, address, shortAddress);
            //we crate a new device with th tag
            DW1000Device myTag(address, shortAddress);
            
            if(addNetworkDevices(&myTag))
            {
                // dw1000Serial.print("blink; 1 device added ! -> "); 
                // dw1000Serial.print(" short:");
                // dw1000Serial.println(myTag.getShortAddress(), HEX);
                
                //we relpy by the transmit ranging init message
                transmitRangingInit(&myTag);
                noteActivity();
            }
            _expectedMsgId=POLL;
        }
        else if(messageType==RANGING_INIT && _type==TAG){
            
            byte address[2];
            _globalMac.decodeLongMACFrame(data, address);
            //we crate a new device with the anchor
            DW1000Device myAnchor(address, true);
            
            if(addNetworkDevices(&myAnchor, true))
            {
                // dw1000Serial.print("ranging init; 1 device added ! -> ");
                // dw1000Serial.print(" short:");
                // dw1000Serial.println(myAnchor.getShortAddress(), HEX);
            }
        
            noteActivity();
            
        }
        else
        {
            //we have a short mac layer frame !
            byte address[2];
            _globalMac.decodeShortMACFrame(data, address);
            
            
            
            //we get the device which correspond to the message which was sent (need to be filtered by MAC address)
            DW1000Device *myDistantDevice=searchDistantDevice(address);
            
            
            
            if(myDistantDevice==NULL && _type != LISTENER)
            {
                dw1000Serial.println("Not found");
                //we don't have the short address of the device in memory
                /*
                dw1000Serial.print("unknown: ");
                dw1000Serial.print(address[0], HEX);
                dw1000Serial.print(":");
                dw1000Serial.println(address[1], HEX);
                */
                return;
            } 
            
        
            //then we proceed to range protocole
            if(_type==ANCHOR){
                // if(messageType != _expectedMsgId) {
                //     // unexpected message, start over again (except if already POLL)
                //     _protocolFailed = true;
                // }
                if(messageType == POLL) {
                    //we receive a POLL which is a broacast message
                    //we need to grab info about it
                    short numberDevices=0;
                    memcpy(&numberDevices, data+SHORT_MAC_LEN+1, 1);
                    
                    for(short i=0; i<numberDevices; i++)
                    {
                        //we need to test if this value is for us:
                        //we grab the mac address of each devices:
                        byte shortAddress[2];
                        memcpy(shortAddress, data+SHORT_MAC_LEN+2+i*4, 2);
                        
                        //we test if the short address is our address
                        if(shortAddress[0]==_currentShortAddress[0] && shortAddress[1]==_currentShortAddress[1])
                        {
                            //we grab the replytime wich is for us
                            unsigned short replyTime = 0;
                            memcpy(&replyTime, data+SHORT_MAC_LEN+2+i*4+2, 2);
                            //we configure our replyTime;
                            _replyDelayTimeUS=replyTime;
                            
                            // on POLL we (re-)start, so no protocol failure
                            _protocolFailed = false;
                            
                            DW1000.getReceiveTimestamp(myDistantDevice->timePollReceived);
                            //we note activity for our device:
                            myDistantDevice->noteActivity();

                            //we indicate our next receive message for our ranging protocole
                            _expectedMsgId = RANGE;
                            transmitPollAck(myDistantDevice);
                            noteActivity();
                            
                            return;
                        }
                    
                    }
                    
                    
                }
                else if(messageType == RANGE) {
                    //we receive a RANGE which is a broacast message
                    //we need to grab info about it
                    short numberDevices=0;
                    memcpy(&numberDevices, data+SHORT_MAC_LEN+1, 1);
                    
                    
                    for(short i=0; i<numberDevices; i++)
                    {
                        //we need to test if this value is for us:
                        //we grab the mac address of each devices:
                        byte shortAddress[2];
                        memcpy(shortAddress, data+SHORT_MAC_LEN+2+i*17, 2);
                        
                        //we test if the short address is our address
                        if(shortAddress[0]==_currentShortAddress[0] && shortAddress[1]==_currentShortAddress[1])
                        {
                            //we grab the replytime wich is for us
                            DW1000.getReceiveTimestamp(myDistantDevice->timeRangeReceived);
                            noteActivity();
                            _expectedMsgId = POLL;
                            
                            if(!_protocolFailed) {
                                
                                myDistantDevice->timePollSent.setTimestamp(data+SHORT_MAC_LEN+4+17*i);
                                myDistantDevice->timePollAckReceived.setTimestamp(data+SHORT_MAC_LEN+9+17*i);
                                myDistantDevice->timeRangeSent.setTimestamp(data+SHORT_MAC_LEN+14+17*i);
                                
                                // (re-)compute range as two-way ranging is done
                                DW1000Time myTOF;
                                computeRangeAsymmetric(myDistantDevice, &myTOF); // CHOSEN RANGING ALGORITHM
                                
                                float distance=myTOF.getAsMeters();
                                distance += IC_ANTENNA_RANGE_OFFSET;
                                distance *= IC_ANTENNA_RANGE_CORFACT;
                                IC_ANTENNA_RANGE_CORNEAR(distance);
                                
                                myDistantDevice->setRXPower(DW1000.getReceivePower());
                                myDistantDevice->setRange(distance);
                                
                                myDistantDevice->setFPPower(DW1000.getFirstPathPower());
                                myDistantDevice->setQuality(DW1000.getReceiveQuality());
                                uint8_t seq = data[SHORT_MAC_LEN + 2 + numberDevices * 17] ;
                                myDistantDevice->setLastPollSeq(seq);
                                //we send the range to TAG
                                transmitRangeReport(myDistantDevice);
                                
                                //we have finished our range computation. We send the corresponding handler
                                _lastDistantDevice=myDistantDevice->getIndex(); 
                                if((_handleNewRange != 0)&&(distance>(-10.0)&&distance<(750.0))) {
                                    (*_handleNewRange)();
                                }
                                
                            }
                            else{
                                transmitRangeFailed(myDistantDevice);
                            }
                            
                            
                            return;
                        }
                        
                    }

                    
                    
                
                }
            }

            else if(_type==LISTENER){
                // if(messageType != _expectedMsgId) {
                //     // unexpected message, start over again
                //     _expectedMsgId = POLL_ACK;
                //     return;
                // }
                //Serial.print("MESSAGE TYPE: ");
                //Serial.println(messageType);
                if(messageType == RANGE_REPORT) {
                    // Serial.println("RANGE REPORT RECEIVED");
                    byte senderaddress[2];
                    _globalMac.decodeShortMACFrame(data, senderaddress);
                    // Serial.print("FROM: ");
                    // Serial.print(senderaddress[0], HEX);
                    // Serial.print(":");
                    // Serial.println(senderaddress[1], HEX);
                    if(senderaddress[0]!=_currentShortAddress[0] || senderaddress[1]!=_currentShortAddress[1])
                    {
                        // Serial.println("TO ME");
                    DW1000.receivePermanently(false);
                    DW1000.idle();
                    DW1000.setClockAcc();
                    //DW1000.readAccumulatorData(cirData, CIR_DATA_LENGTH_100);
                    DW1000.readAccumulatorDataMulti(730*4+1, 200, 850*4+1, 200, cirData_2) ;
                    DW1000.resetClockAcc();
                    _sensingMode = true;
                    uint8_t ack_seq = data[SHORT_MAC_LEN + 9];
                    memcpy(&g_lastRange, data+1+SHORT_MAC_LEN, 4);
                    // Serial.print("RANGE: ");
                    // Serial.println(g_lastRange);
                    DW1000.GetDiagnosticInfo(diag_2, cirData_2);
                    DW1000.sendPacket(ack_seq, &temp[1], diag_2);
                    DW1000.clearAllStatus();
                    DW1000.receivePermanently(true);
                    DW1000.newReceive();
                    DW1000.startReceive();
                    }
                    else{
                    // Serial.print("NULL_PACKET from: ");
                    // Serial.print(senderaddress[0], HEX);
                    // Serial.print(":");
                    // Serial.println(senderaddress[1], HEX);
                    // Serial.print("My address: ");
                    // Serial.print(_currentShortAddress[0], HEX);
                    // Serial.print(":");
                    // Serial.println(_currentShortAddress[1], HEX);
                    // Serial.print("Data: ");
                    // for(int i=0; i<30; i++) {
                    //     if(data[i] < 0x10) Serial.print("0");
                    //     Serial.print(data[i], HEX);
                    //     Serial.print(" ");
                    // }
                    DW1000.receivePermanently(false);
                    DW1000.idle();
                    DW1000.setClockAcc();
                    //DW1000.readAccumulatorData(cirData, CIR_DATA_LENGTH_100);
                    DW1000.readAccumulatorDataMulti(730*4+1, 200, 850*4+1, 200, cirData_2) ;
                    DW1000.resetClockAcc();
                    // _sensingMode = true;
                    uint8_t ack_seq = data[SHORT_MAC_LEN + 19];
                    memcpy(&g_lastRange, data+1+SHORT_MAC_LEN, 4);
                    // Serial.print("RANGE: ");
                    // Serial.println(g_lastRange);
                    DW1000.GetDiagnosticInfo(diag_2, cirData_2);
                    DW1000.sendPacket(ack_seq, &temp[2], diag_2);
                    DW1000.clearAllStatus();
                    DW1000.receivePermanently(true);
                    DW1000.newReceive();
                    DW1000.startReceive();
                    }
                }
                if(messageType == RANGE){
                    //uint8_t ack_seq = data[SHORT_MAC_LEN + 1];
                    // DW1000.GetDiagnosticInfo(diag);
                    // DW1000.sendPacket(0, &address[0]);
                    float ppm_cri = DW1000.cfo_ppm_via_formula(cri_value);
                    ppm_debug_cri = ppm_cri;
                    static uint8_t s_trim = DW1000.getXtalTrim5bit();
                    trim_index = s_trim;
                    s_trim = DW1000.servoXtalByPpm(ppm_cri, s_trim, 0.8f);
                    // Serial.print("CFO ppm: ");
                    // Serial.print(ppm_cri);
                }
                if(messageType == POLL || messageType == BLINK){
                    //Serial.println("POLL RECEIVED");
                    DW1000.receivePermanently(false);
                    DW1000.idle();
                    DW1000.setClockAcc();
                    DW1000.readAccumulatorData(cirData, CIR_DATA_LENGTH_100);
                    DW1000.resetClockAcc();
                    _sensingMode = true;
                    //uint8_t ack_seq = data[SHORT_MAC_LEN + 1];
                    DW1000.GetDiagnosticInfo(diag, cirData);
                    if(messageType == BLINK){
                        uint16_t blinkSeq = (uint16_t)data[12] | ((uint16_t)data[13] << 8);
                        diag->firstPathAmp2 = blinkSeq;
                    }
                    DW1000.sendPacket(0, &temp[0], diag);
                    DW1000.clearAllStatus();
                    DW1000.receivePermanently(true);
                    DW1000.newReceive();
                    DW1000.startReceive();
                     // broadcast
                }
            }



            else if(_type==TAG){
                // get message and parse
                if(messageType != _expectedMsgId) {
                    // unexpected message, start over again
                    //not needed ?
                    return;
                    _expectedMsgId = POLL_ACK;
                    return;
                }
                if(messageType == POLL_ACK) {
                    DW1000.getReceiveTimestamp(myDistantDevice->timePollAckReceived);
                    //we note activity for our device:
                    myDistantDevice->noteActivity();
                    noteActivity();
                    float ppm_cri = DW1000.cfo_ppm_via_formula(cri_value);
                    ppm_debug_cri = ppm_cri;
                    static uint8_t s_trim = DW1000.getXtalTrim5bit();
                    trim_index = s_trim;
                    s_trim = DW1000.servoXtalByPpm(ppm_cri, s_trim, 0.8f);
                    // Serial.print("CFO ppm: ");
                    // Serial.print(ppm_cri);
                    
                    
                    
                    //in the case the message come from our last device:
                    if(myDistantDevice->getIndex()==_networkDevicesNumber-1){
                    _expectedMsgId = RANGE_REPORT;
                    //and transmit the next message (range) of the ranging protocole (in broadcast)
                    transmitRange(NULL);
                    }
                }
                else if(messageType == RANGE_REPORT) {
                    
                    float curRange;
                    memcpy(&curRange, data+1+SHORT_MAC_LEN, 4);
                    float curRXPower;
                    memcpy(&curRXPower, data+5+SHORT_MAC_LEN, 4);
                    //we have a new range to save !
                    myDistantDevice->setRange(curRange);
                    myDistantDevice->setRXPower(curRXPower);
                    
                    
                    //We can call our handler !
                    //we have finished our range computation. We send the corresponding handler
                    _lastDistantDevice=myDistantDevice->getIndex();
                    if((_handleNewRange != 0)&&(curRange>(-10.0)&&curRange<(750.0))) {
                        (*_handleNewRange)();
                    }
                    noteActivity();
                }
                else if(messageType == RANGE_FAILED) {
                    //not needed as we have a timer;
                    return;
                    _expectedMsgId = POLL_ACK;
                }
            }
        }
        
    }
}










/* ###########################################################################
 * #### Private methods and Handlers for transmit & Receive reply ############
 * ######################################################################### */


void DW1000RangingClass::handleSent() {
    
    // status change on sent success
    _sentAck = true;
        const int LED_PIN = 18;
    ledState_tx = ! ledState_tx;
    digitalWrite(LED_PIN, ledState_tx); 
    
    
}

void DW1000RangingClass::handleReceived() {
    
    
    // status change on received success
    _receivedAck = true;

    const int LED_PIN = 17;
    ledState_rx = !ledState_rx;
    digitalWrite(LED_PIN, ledState_rx);
    if(_type==LISTENER || _type==TAG){
        cri_value = DW1000.getCarrierIntegratorRaw();
    }
    // if(_type==LISTENER && _sensingMode){
    //     // static uint32_t rxCount = 0;
    //     // rxCount++;
    //     // Serial.print("RX:");
    //     // Serial.println(rxCount);
    //     // 中断轮询方案：在中断中直接等待DMA完成
    //     DW1000.setClockAcc();
    //     //DW1000.readBytesDMA_Async(ACC_MEM, 1, cirData, CIR_DATA_LENGTH_100, NULL);
    //     DW1000.readAccumulatorData(cirData, CIR_DATA_LENGTH_100);
    //     DW1000.resetClockAcc();
    //     _sensingMode = false;
    // }


    
  
}


void DW1000RangingClass::noteActivity() {
    // update activity timestamp, so that we do not reach "resetPeriod"
    _lastActivity = millis();
}

void DW1000RangingClass::resetInactive() {
    //if inactive
    if(_type==ANCHOR){
        _expectedMsgId = POLL;
        receiver();
    }
    else if(_type==TAG){
        _networkDevicesNumber=0;
        receiver();
    }
    noteActivity();
}

void DW1000RangingClass::timerTick(){
        // if(_networkDevicesNumber>0 && counterForBlink!=0)
        // {
        //     if(_type==TAG){
        //         _expectedMsgId = POLL_ACK;
        //         //send a prodcast poll
        //         transmitPoll(NULL);
        //     }
        // }
        // else if(counterForBlink==0)
        // {
        //     if(_type==TAG){
        //         transmitBlink();
        //     }
        //     //check for inactive devices if we are a TAG or ANCHOR
        //     //checkForInactiveDevices();
        // }
        // counterForBlink++;
        // //if(counterForBlink>10){
        // if(counterForBlink>3){
        //     counterForBlink=0;
            
        // }
        if(_networkDevicesNumber>0)
        {
            if(_type==TAG){
                _expectedMsgId = POLL_ACK;
                //send a prodcast poll
                transmitPoll(NULL);
            }
        }
        else{
            if(_type==TAG){
                transmitBlink();
            }
            //check for inactive devices if we are a TAG or ANCHOR
            //checkForInactiveDevices();
        }
    
}



void DW1000RangingClass::copyShortAddress(byte address1[],byte address2[]){
    *address1=*address2;
    *(address1+1)=*(address2+1);
}

/* ###########################################################################
 * #### Methods for ranging protocole   ######################################
 * ######################################################################### */

void DW1000RangingClass::transmitInit(){
    DW1000.newTransmit();
    //DW1000.setDefaults();
}


void DW1000RangingClass::transmit(byte datas[]){
    DW1000.setData(datas, LEN_DATA);
    DW1000.startTransmit();
}


void DW1000RangingClass::transmit(byte datas[], DW1000Time time){
    DW1000.setDelay(time);
    DW1000.setData(data, LEN_DATA);
    DW1000.startTransmit();
}

void DW1000RangingClass::transmitBlink(){
    _timerDelay=DEFAULT_TIMER_DELAY;
    transmitInit();
    //DW1000.setManualTxPower(33.5);
    _globalMac.generateBlinkFrame(data, _currentAddress, _currentShortAddress);
    data[12] = _blinkSeq & 0xFF;
    data[13] = (_blinkSeq >> 8) & 0xFF;
    _blinkSeq++;
    //    byte originalTxPower[4];
    //DW1000.readBytes(TX_POWER, NO_SUB, originalTxPower, 4);
    // Serial.print("Original TX power: ");
    // Serial.println(*(unsigned int*)originalTxPower, HEX);

   // DW1000.setManualTxPower(0.0);
    // byte modifiedTxPower[4];
    // DW1000.readBytes(TX_POWER, NO_SUB, modifiedTxPower, 4);
    // Serial.print("Modified TX power: ");
    // Serial.println(*(unsigned int*)modifiedTxPower, HEX);

    transmit(data);
    // DW1000.writeBytes(TX_POWER, NO_SUB, originalTxPower, 4);
}

void DW1000RangingClass::transmitRangingInit(DW1000Device *myDistantDevice){
    transmitInit();
    //we generate the mac frame for a ranging init message
    _globalMac.generateLongMACFrame(data, _currentShortAddress, myDistantDevice->getByteAddress());
    //we define the function code
    data[LONG_MAC_LEN]=RANGING_INIT;
    
    copyShortAddress(_lastSentToShortAddress,myDistantDevice->getByteShortAddress());
    
    transmit(data);
}

void DW1000RangingClass::transmitPoll(DW1000Device *myDistantDevice) {

    transmitInit();
    
    //DW1000.setManualTxPower(33.5);
    if(myDistantDevice==NULL)
    {
        //we need to set our timerDelay:
        //_timerDelay=DEFAULT_TIMER_DELAY+(int)(_networkDevicesNumber*2*DEFAULT_REPLY_DELAY_TIME_normal/1000+DEFAULT_REPLY_DELAY_TIME/1000);
        _timerDelay=DEFAULT_TIMER_DELAY+(int)(_networkDevicesNumber*(2+3+3));
        byte shortBroadcast[2]={0xFF, 0xFF};
        _globalMac.generateShortMACFrame(data, _currentShortAddress, shortBroadcast);
        data[SHORT_MAC_LEN] = POLL;
        //we enter the number of devices
        data[SHORT_MAC_LEN+1]=_networkDevicesNumber;
        
        for(short i=0; i<_networkDevicesNumber; i++)
        {
            //each devices have a different reply delay time.
            _networkDevices[i].setReplyTime((2*i+1)*DEFAULT_REPLY_DELAY_TIME_normal);
            //we write the short address of our device:
            memcpy(data+SHORT_MAC_LEN+2+4*i, _networkDevices[i].getByteShortAddress(), 2);
            
            //we add the replyTime
            unsigned short replyTime=_networkDevices[i].getReplyTime();
            memcpy(data+SHORT_MAC_LEN+2+2+4*i, &replyTime, 2);
            
        }
        
        copyShortAddress(_lastSentToShortAddress,shortBroadcast);
        
    }
    else{
        //we redefine our default_timer_delay for just 1 device;
        _timerDelay=DEFAULT_TIMER_DELAY;
        
        _globalMac.generateShortMACFrame(data, _currentShortAddress, myDistantDevice->getByteShortAddress());
        
        data[SHORT_MAC_LEN] = POLL;
        data[SHORT_MAC_LEN+1]=1;
        unsigned int replyTime=myDistantDevice->getReplyTime();
        memcpy(data+SHORT_MAC_LEN+2, &replyTime, sizeof(int));
        
        copyShortAddress(_lastSentToShortAddress,myDistantDevice->getByteShortAddress());
    }
    
    
    
    
    
    //byte originalTxPower[4];
    //DW1000.readBytes(TX_POWER, NO_SUB, originalTxPower, 4);
    // Serial.print("Original TX power: ");
    // Serial.println(*(unsigned int*)originalTxPower, HEX);

    //DW1000.setManualTxPower(0.0);
    // byte modifiedTxPower[4];
    // DW1000.readBytes(TX_POWER, NO_SUB, modifiedTxPower, 4);
    // Serial.print("Modified TX power: ");
    // Serial.println(*(unsigned int*)modifiedTxPower, HEX);

    transmit(data);
    //DW1000.writeBytes(TX_POWER, NO_SUB, originalTxPower, 4);
}


void DW1000RangingClass::transmitPollAck(DW1000Device *myDistantDevice) {
    transmitInit();
    //DW1000.setManualTxPower(33.5);
    _globalMac.generateShortMACFrame(data, _currentShortAddress, myDistantDevice->getByteShortAddress());
    data[SHORT_MAC_LEN] = POLL_ACK;
    // delay the same amount as ranging tag
    DW1000Time deltaTime = DW1000Time(DEFAULT_REPLY_DELAY_TIME_normal, DW_MICROSECONDS);
    DW1000.setDelay(deltaTime);
    // DW1000Time tTarget = myDistantDevice->timePollReceived + DW1000Time(DEFAULT_REPLY_DELAY_TIME, DW_MICROSECONDS);
    // DW1000.setDelayAbsolute(tTarget);
    
    //myDistantDevice->timePollAckSent = tTarget;
    // DW1000.setData(data, LEN_DATA);
    // DW1000.startTransmit();
    copyShortAddress(_lastSentToShortAddress,myDistantDevice->getByteShortAddress());
    transmit(data, deltaTime);
}

void DW1000RangingClass::transmitRange(DW1000Device *myDistantDevice) {
    //transmit range need to accept broadcast for multiple anchor
    transmitInit();
    
    //DW1000.setManualTxPower(33.5);
    if(myDistantDevice==NULL)
    {
        //we need to set our timerDelay:
        //_timerDelay=DEFAULT_TIMER_DELAY+(int)(_networkDevicesNumber*3*DEFAULT_REPLY_DELAY_TIME/1000);
        
        byte shortBroadcast[2]={0xFF, 0xFF};
        _globalMac.generateShortMACFrame(data, _currentShortAddress, shortBroadcast);
        data[SHORT_MAC_LEN] = RANGE;
        //we enter the number of devices
        data[SHORT_MAC_LEN+1]=_networkDevicesNumber;
        
        // delay sending the message and remember expected future sent timestamp
        DW1000Time deltaTime = DW1000Time(DEFAULT_REPLY_DELAY_TIME_normal, DW_MICROSECONDS);
        DW1000Time timeRangeSent = DW1000.setDelay(deltaTime);
        // DW1000Time tTarget = _networkDevices[_networkDevicesNumber-1].timePollAckReceived 
        //            + DW1000Time(DEFAULT_REPLY_DELAY_TIME, DW_MICROSECONDS);
        // DW1000Time timeRangeSent = tTarget;
        for(short i=0; i<_networkDevicesNumber; i++)
        {
            //we write the short address of our device:
            memcpy(data+SHORT_MAC_LEN+2+17*i, _networkDevices[i].getByteShortAddress(), 2);
            
            
            //we get the device which correspond to the message which was sent (need to be filtered by MAC address)
            _networkDevices[i].timeRangeSent = timeRangeSent;
            _networkDevices[i].timePollSent.getTimestamp(data+SHORT_MAC_LEN+4+17*i);
            _networkDevices[i].timePollAckReceived.getTimestamp(data+SHORT_MAC_LEN+9+17*i);
            _networkDevices[i].timeRangeSent.getTimestamp(data+SHORT_MAC_LEN+14+17*i);
            
        }
        data[SHORT_MAC_LEN+2+17*_networkDevicesNumber]=_pollSeq++;
        copyShortAddress(_lastSentToShortAddress, shortBroadcast);
        //DW1000.setDelayAbsolute(tTarget);
        // DW1000.setData(data, LEN_DATA);
        // DW1000.startTransmit();
    }
    else{
        _globalMac.generateShortMACFrame(data, _currentShortAddress, myDistantDevice->getByteShortAddress());
        data[SHORT_MAC_LEN] = RANGE;
        // delay sending the message and remember expected future sent timestamp
        DW1000Time tTarget = myDistantDevice->timePollAckReceived 
                   + DW1000Time(_replyDelayTimeUS, DW_MICROSECONDS);
        myDistantDevice->timeRangeSent = tTarget;
       // DW1000Time deltaTime = DW1000Time(DEFAULT_REPLY_DELAY_TIME, DW_MICROSECONDS);
        //we get the device which correspond to the message which was sent (need to be filtered by MAC address)
        //myDistantDevice->timeRangeSent = DW1000.setDelay(deltaTime);
        myDistantDevice->timePollSent.getTimestamp(data+1+SHORT_MAC_LEN);
        myDistantDevice->timePollAckReceived.getTimestamp(data+6+SHORT_MAC_LEN);
        myDistantDevice->timeRangeSent.getTimestamp(data+11+SHORT_MAC_LEN);
        copyShortAddress(_lastSentToShortAddress,myDistantDevice->getByteShortAddress());
        // DW1000.setDelayAbsolute(tTarget);
        // DW1000.setData(data, LEN_DATA);
        // DW1000.startTransmit();
    }

    transmit(data);
    
    // if(myDistantDevice != NULL) {
    //     //DW1000Time timeDiff = (myDistantDevice->timePollAckReceived - myDistantDevice->timePollSent).wrap();
    //     DW1000Time baseDelay = DW1000Time(50000, DW_MICROSECONDS);
    //     DW1000Time delay200ns = DW1000Time(150, DW_NANOSECONDS);
    //     DW1000Time totalDelay = myDistantDevice->timeRangeSent + baseDelay + delay200ns;
    //     transmitNullPacket(totalDelay);
    // }
    // else {
    //     if(_networkDevicesNumber > 0) {
    //         //DW1000Time timeDiff = (_networkDevices[0].timePollAckReceived - _networkDevices[0].timePollSent).wrap();
    //     DW1000Time baseDelay = DW1000Time(50000, DW_MICROSECONDS);
    //     DW1000Time delay200ns = DW1000Time(150, DW_NANOSECONDS);
    //     DW1000Time totalDelay = baseDelay + delay200ns;
            
    //         transmitNullPacket(totalDelay);
    //     }
    // }

}


void DW1000RangingClass::transmitRangeReport(DW1000Device *myDistantDevice) {
    transmitInit();
    //DW1000.setManualTxPower(33.5);
    _globalMac.generateShortMACFrame(data, _currentShortAddress, myDistantDevice->getByteShortAddress());
    data[SHORT_MAC_LEN] = RANGE_REPORT;
    // write final ranging result
    float curRange=myDistantDevice->getRange();
    float curRXPower=myDistantDevice->getRXPower();
    //We add the Range and then the RXPower
    memcpy(data+1+SHORT_MAC_LEN, &curRange, 4);
    memcpy(data+5+SHORT_MAC_LEN, &curRXPower, 4);
    data[SHORT_MAC_LEN+9]=myDistantDevice->getLastPollSeq();
    // byte originalTxPower[4];
    // DW1000.readBytes(TX_POWER, NO_SUB, originalTxPower, 4);
    // DW1000.setManualTxPower(30);
    //DW1000Time tTarget = myDistantDevice->timeRangeReceived + DW1000Time(_replyDelayTimeUS, DW_MICROSECONDS);
    DW1000Time tTarget = myDistantDevice->timeRangeReceived + DW1000Time(DEFAULT_REPLY_DELAY_TIME, DW_MICROSECONDS);
    DW1000.setDelayAbsolute(tTarget);
    DW1000.setData(data, LEN_DATA);
    DW1000.startTransmit();
    copyShortAddress(_lastSentToShortAddress,myDistantDevice->getByteShortAddress());
    //DW1000.writeBytes(TX_POWER, NO_SUB, originalTxPower, 4);
    //transmit(data, DW1000Time(_replyDelayTimeUS, DW_MICROSECONDS));
}

void DW1000RangingClass::transmitNullPacket(DW1000Time delayTime) {
    transmitInit();
    _globalMac.generateShortMACFrame(data, _currentShortAddress, _currentShortAddress);
    data[SHORT_MAC_LEN] = RANGE_REPORT;
    data[SHORT_MAC_LEN+9]= 2;
    
    byte originalTxPower[4];
    DW1000.readBytes(TX_POWER, NO_SUB, originalTxPower, 4);
    // Serial.print("Original TX power: ");
    // Serial.println(*(unsigned int*)originalTxPower, HEX);

    DW1000.setManualTxPower(0.0);
    // byte modifiedTxPower[4];
    // DW1000.readBytes(TX_POWER, NO_SUB, modifiedTxPower, 4);
    // Serial.print("Modified TX power: ");
    // Serial.println(*(unsigned int*)modifiedTxPower, HEX);

    DW1000.setDelayAbsolute(delayTime);
    DW1000.setData(data, LEN_DATA);
    DW1000.startTransmit();
    copyShortAddress(_lastSentToShortAddress, _currentShortAddress);
    //DW1000.writeBytes(TX_POWER, NO_SUB, originalTxPower, 4);
    //transmit(data, delayTime);
    //Serial.println("Null packet transmitted");
}

void DW1000RangingClass::transmitRangeFailed(DW1000Device *myDistantDevice) {
    transmitInit();
    _globalMac.generateShortMACFrame(data, _currentShortAddress, myDistantDevice->getByteShortAddress());
    data[SHORT_MAC_LEN] = RANGE_FAILED;
    
    copyShortAddress(_lastSentToShortAddress,myDistantDevice->getByteShortAddress());
    transmit(data);
}

void DW1000RangingClass::receiver() {
    DW1000.newReceive();
    DW1000.setDefaults();
    // so we don't need to restart the receiver manually
    DW1000.receivePermanently(true);
    DW1000.startReceive();
}









/* ###########################################################################
 * #### Methods for range computation and corrections  #######################
 * ######################################################################### */


void DW1000RangingClass::computeRangeAsymmetric(DW1000Device *myDistantDevice, DW1000Time *myTOF) {
    // asymmetric two-way ranging (more computation intense, less error prone)
    DW1000Time round1 = (myDistantDevice->timePollAckReceived-myDistantDevice->timePollSent).wrap();
    DW1000Time reply1 = (myDistantDevice->timePollAckSent-myDistantDevice->timePollReceived).wrap();
    DW1000Time round2 = (myDistantDevice->timeRangeReceived-myDistantDevice->timePollAckSent).wrap();
    DW1000Time reply2 = (myDistantDevice->timeRangeSent-myDistantDevice->timePollAckReceived).wrap();
    
    myTOF->setTimestamp((round1 * round2 - reply1 * reply2) / (round1 + round2 + reply1 + reply2));
    //myTOF->setTimestamp((round1 - reply1) / 2);
    /*rou
    dw1000Serial.print("timePollAckReceived ");myDistantDevice->timePollAckReceived.print();
    dw1000Serial.print("timePollSent ");myDistantDevice->timePollSent.print();
    dw1000Serial.print("round1 "); dw1000Serial.println((long)round1.getTimestamp());
    
    dw1000Serial.print("timePollAckSent ");myDistantDevice->timePollAckSent.print();
    dw1000Serial.print("timePollReceived ");myDistantDevice->timePollReceived.print();
    dw1000Serial.print("reply1 "); dw1000Serial.println((long)reply1.getTimestamp());
    
    dw1000Serial.print("timeRangeReceived ");myDistantDevice->timeRangeReceived.print();
    dw1000Serial.print("timePollAckSent ");myDistantDevice->timePollAckSent.print();
    dw1000Serial.print("round2 "); dw1000Serial.println((long)round2.getTimestamp());
    
    dw1000Serial.print("timeRangeSent ");myDistantDevice->timeRangeSent.print();
    dw1000Serial.print("timePollAckReceived ");myDistantDevice->timePollAckReceived.print();
    dw1000Serial.print("reply2 "); dw1000Serial.println((long)reply2.getTimestamp());
     */
    //Serial.print("round1: ");
    //Serial.println((long)round1.getTimestamp());
    // Serial.print("reply1: ");
    // Serial.println((long)reply1.getTimestamp());
    // Serial.print("round2: ");
    // Serial.println((long)round2.getTimestamp());
    // Serial.print("reply2: ");
    // Serial.println((long)reply2.getTimestamp());
    // Serial.print("TOF: ");
    // Serial.println((long)myTOF->getTimestamp());
}


/* FOR DEBUGGING*/
void DW1000RangingClass::visualizeDatas(byte datas[]){
    char string[60];
    sprintf(string, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
            datas[0], datas[1], datas[2], datas[3], datas[4], datas[5], datas[6], datas[7],datas[8],datas[9],datas[10],datas[11],datas[12],datas[13],datas[14],datas[15]);
    dw1000Serial.println(string);
}





