/*
  CC1101 radio-Chip on In-Circuit radino CC1101 modules
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

#include "IC_CC1101.h"

float IC_CC1101::setFrequency(float desiredFreq)
{
	//Frequency = (Fxosc/(2^16))*FREQ
	desiredFreq *= 65536;
	desiredFreq /= 26;
	desiredFreq = (uint32_t)desiredFreq;
  halRfWriteReg(FREQ2,(((uint32_t)desiredFreq)>>16)&0xFF);   //Frequency Control Word, High Byte
  halRfWriteReg(FREQ1,(((uint32_t)desiredFreq)>>8)&0xFF);   //Frequency Control Word, Middle Byte
  halRfWriteReg(FREQ0,(((uint32_t)desiredFreq)>>0)&0xFF);   //Frequency Control Word, Low Byte
  desiredFreq *= 26;
  desiredFreq /= 65536;
  return desiredFreq;
}

unsigned long IC_CC1101::setDatarate(unsigned long desiredDatarate)
{
	uint8_t exp,mant,reg;
	double tmp;
	//Datarate = (((256+Mant)*(2^Exp))/(2^28))*Fosc	-- Fxosc=26mHz
	//2^28 = 268435456
	//2^20 = 1048576
	//Exp = log2((Datarate*(2^20))/Fxosc)
	//Mant = ((Datarate*(2^28))/(Fxosc*(2^Exp)))-256
	tmp = desiredDatarate;
	tmp *= 1.048576;
	tmp /= 26;
	tmp = log(tmp)/log(2.);
	exp = tmp;
	exp &= 0xF;
	tmp = desiredDatarate;
	tmp *= 268.435456;
	tmp /= 26*(1<<exp);
	tmp -= 256;
	tmp += 0.5;
	if (tmp>=256)
	{
		exp+=1;
		mant=0;
	} else {
		mant=tmp;
	}
	exp &= 0xF;
	reg = halRfReadReg(MDMCFG4);
	reg &= 0xF0;
	reg |= exp;
  halRfWriteReg(MDMCFG4,reg);
  halRfWriteReg(MDMCFG3,mant);
  tmp = (256+mant);
  tmp *= (1<<exp);
	tmp /= 268.435456;
	tmp *= 26;
  return tmp;
}

unsigned char IC_CC1101::setPaTable(unsigned char newValue)
{
	m_paTableEntry = newValue;
  for (unsigned char i=0;i<PATABLE_SIZE;i++)
  	patable[i] = m_paTableEntry;
  halRfWriteBurstReg(PATABLE,patable,8);
}

unsigned char IC_CC1101::Transmit_wCCA(const unsigned char *txBuf, unsigned char cnt, uint32_t timeout, uint32_t gap_us)
{
  unsigned long i;
  uint32_t startTime = ((uint32_t)millis());
  uint8_t oldVal1 = halRfReadReg(IOCFG0);
	uint8_t ret = 0;
  bool cameFromRX = false;
  
  if (GetState()!=IC_CCIDLE)
	{
    while((GetState()==IC_CCRECEIVING) && ((millis()-startTime)<timeout))
    {}
		if (GetState()!=IC_CCWAITINGRX)
		{
			return 0;
		}
    cameFromRX = true;
		halRfStrobe(SIDLE);
		m_state = IC_CCIDLE;
	}
  
  halRfStrobe(SFTX);
  //MCSM1 -- [5:4]CCA Mode (always,rssi,!receiving,rssi+recv), [3:2]After RX (idle,fstxon,TX,StayRX), [1:0]After TX (idle,fstxon,stayTX,RX)
  halRfWriteReg(MCSM1,0x3F);		//CCA below Threshold and !recv, RX->RX, TX->RX
  //halRfWriteReg(MCSM1,0x0F);		//Always TX, RX->RX, TX->RX
  halRfStrobe(SRX);
	halRfWriteReg(IOCFG0,0x09);		//GDO0 Assert when CCA Ok
	
	do
	{
    if (digitalRead(GDO0))
    {
      for ( uint32_t i=0 ; !ret && digitalRead(GDO0) ; )
      {
        if (i>gap_us)
        {
          ret=1;
          halRfStrobe(STX);  //start send
        }
        else
        {
          delayMicroseconds(10);
          i+=10;
        }
      }
      if (!ret && digitalRead(_GDO2))
      {
        m_state = IC_CCGOTPACKET;
        halRfWriteReg(IOCFG0,oldVal1);
        halRfWriteReg(MCSM1,MCSM1buf);
        return 0;
      }
    }
  }
  while (!ret && ((millis()-startTime)<timeout));
	
	if (!ret)
	{
		halRfStrobe(SIDLE);
		m_state = IC_CCIDLE;
		halRfStrobe(SFTX);
		halRfStrobe(SFRX);
		halRfWriteReg(IOCFG0,oldVal1);
		halRfWriteReg(MCSM1,MCSM1buf);
    if (cameFromRX) StartReceive();
		return 0;
	}
  
  //halRfStrobe(STX);  //start send
	halRfWriteReg(TXFIFO,cnt);  //write len
	halRfWriteBurstReg(TXFIFO,txBuf,cnt);  //fill FIFO
	halRfWriteReg(IOCFG0,oldVal1);
	halRfWriteReg(MCSM1,MCSM1buf);
	
  i = ((uint32_t)millis());
  //wait for transfer start
  while (!digitalRead(GDO0))
  {
  	if ((((uint32_t)millis())-i)>=10)
  	{
		  halRfStrobe(SIDLE);
		  m_state = IC_CCIDLE;
		  halRfStrobe(SFTX);
		  halRfStrobe(SFRX);
      if (cameFromRX) StartReceive();
		  return 0;
		}
  }
  
  m_state = IC_CCSENDING;
  return 1;
}

uint8_t IC_CC1101::wait_CCA(uint32_t timeout, uint32_t gap_us)
{
	uint32_t startTime = ((uint32_t)millis());
	uint8_t oldVal1 = halRfReadReg(IOCFG2);
	uint8_t ret = 0;
	
	if (GetState()!=IC_CCIDLE) return 0;
	
	halRfWriteReg(MCSM1,0x3C);		//CCA below Threshold and !recv, RX->RX, TX->idle
	halRfWriteReg(IOCFG2,0x09);		//GDO2 Assert when CCA Ok
	halRfStrobe(SRX);
	
	do
	{
		if (digitalRead(_GDO2))
		{
			delayMicroseconds(gap_us);
			if (digitalRead(_GDO2)) ret=1;
		}
	}
	while (!ret && ((((uint32_t)millis())-startTime)<timeout));
	
	halRfStrobe(SIDLE);
	halRfWriteReg(IOCFG2,oldVal1);
	halRfWriteReg(MCSM1,MCSM1buf);
	return ret;
}

void IC_CC1101::startTransfer()
{
  digitalWrite(_SS_PIN, LOW);
#if defined ARDUINO_RADINO32
  pinMode(MISO_PIN,INPUT_PULLUP);
#endif
  //todo possible freeze
  while(digitalRead(MISO_PIN)) {};
#if defined ARDUINO_RADINO32
  SPI_intern.configPins();
#endif
}

//Sets the registers this library depends on
//These override user defined configs from SetConfigRegisters()
void IC_CC1101::SetMandatoryConfig(void)
{
  halRfWriteReg(IOCFG2,0x07);  //GDO2: Assert when packet with good crc received, deassert on fifo read
  halRfWriteReg(IOCFG0,0x06);  //GDO0: Asserts when sync word has been sent/received, de-asserts at the end of the packet or tx underflow
  halRfWriteReg(PKTLEN,61);  //Maximum packet length (is 61 byte so it fits into fifo completely)
  halRfWriteReg(PKTCTRL1, (halRfReadReg(PKTCTRL1) | 0x0C));  //append RSSI LQI and CRC OK, auto flush on CRC NOK, no address verification
  halRfWriteReg(PKTCTRL1, halRfReadReg(PKTCTRL1) & ~(0x03));	//disable address filter
  halRfWriteReg(PKTCTRL0, ((halRfReadReg(PKTCTRL0) & ~0x37) | 0x05));  //use FIFOs; CRC Enable; variable length packets
}

void IC_CC1101::Init(unsigned char maxPacketLength, uint32_t receiveTimeout)
{
  unsigned char mode=0;
  m_state = IC_CCIDLE;
  m_maxPacketLength = maxPacketLength;
  m_receiveTimeout = receiveTimeout;
  
#if defined ARDUINO_RADINO32
  if (_SS_PIN==0xFF)
  {
    //Find out which version
    //CC1101 Pulls Miso low when CS is pulled low externally and RCC is stable (<<1ms)
    pinMode(MISO_PIN,INPUT_PULLUP);
    digitalWrite(SS_PIN_B,HIGH);
    pinMode(SS_PIN_B,OUTPUT_OPEN_DRAIN);
    digitalWrite(SS_PIN_B,HIGH);
    digitalWrite(SS_PIN_C,LOW);
    pinMode(SS_PIN_C,OUTPUT_OPEN_DRAIN);
    digitalWrite(SS_PIN_C,LOW);
    delay(1);
    if (!digitalRead(MISO_PIN)) //RevB
    {
      _GDO2 = GDO2_C;
      _SS_PIN = SS_PIN_C;
    }
    else
    {
      digitalWrite(SS_PIN_C,HIGH);
      digitalWrite(SS_PIN_B,LOW);
      delay(1);
      if (!digitalRead(MISO_PIN)) //RevC
      {
        _GDO2 = GDO2_B;
        _SS_PIN = SS_PIN_B;
      }
      else
      {
        //Non supported Revision or Hardware Error - block
        pinMode(SS_PIN_C,INPUT);
        pinMode(SS_PIN_B,INPUT);
        pinMode(MISO_PIN,INPUT);
        while(1) {}
      }
    }
    pinMode(SS_PIN_C,INPUT);
    pinMode(SS_PIN_B,INPUT);
    pinMode(MISO_PIN,INPUT);
  }
#else
  _GDO2 = GDO2;
  _SS_PIN = SS_PIN;
#endif
  
  //SPI
  pinMode(_SS_PIN, OUTPUT);
  digitalWrite(_SS_PIN, HIGH);
  
#if defined ARDUINO_RADINO32
  SPI_intern.begin();
#else
  pinMode(MISO_PIN, INPUT);
  pinMode(MOSI_PIN, OUTPUT);
  digitalWrite(MOSI_PIN, LOW);
  pinMode(SCK_PIN, OUTPUT);
  digitalWrite(SCK_PIN, HIGH);
  
  // enable SPI Master, MSB, SPI mode 0, FOSC/4
  SPCR = 0;
  SPCR = (mode & 0x7F) | (1<<SPE) | (1<<MSTR);
#endif
  
  //GDO Pins
  pinMode(GDO0, INPUT);
  pinMode(_GDO2, INPUT);
  
  Reset();
  SetConfigRegisters();
  SetMandatoryConfig();
  for (unsigned char i=0;i<PATABLE_SIZE;i++)
  	patable[i] = m_paTableEntry;
  halRfWriteBurstReg(PATABLE,patable,8);
  delay(1);  //let it set
}

//Reset routine
void IC_CC1101::Reset (void)
{
  digitalWrite(_SS_PIN, LOW);
  delay(1);
  digitalWrite(_SS_PIN, HIGH);
  delay(1);
  startTransfer();
  SpiTransfer(SRES);
  pinMode(MISO_PIN,INPUT_PULLUP);
  //todo possible freeze
  while(digitalRead(MISO_PIN)) {};
  digitalWrite(_SS_PIN, HIGH);
}

static unsigned long rx_timeout = 0;
static unsigned long rxStartTime = 0;
IC_CC_STATE IC_CC1101::GetState(void)
{
  switch (m_state)
  {
    default:
    case IC_CCIDLE:
    case IC_CCGOTPACKET:
      break;
      
    case IC_CCSENDING:
      if (digitalRead(GDO0)) break;
      halRfStrobe(SFTX);  //Flush FIFIO
      if (digitalRead(_GDO2))
      {
        m_state = IC_CCGOTPACKET;
      }
      else
      {
        if ((MCSM1buf&0x3)==0x3)
        {
        	m_state = IC_CCWAITINGRX;
        	rx_timeout = m_receiveTimeout;
        	rxStartTime = ((uint32_t)millis());
        } else {
        	m_state = IC_CCIDLE;
        }
      }
      break;
      
    case IC_CCWAITINGRX:
      if (digitalRead(GDO0))
      {
        m_state = IC_CCRECEIVING;
        rx_timeout += 10;
      }
    case IC_CCRECEIVING:
      if (digitalRead(_GDO2))
      {
        m_state = IC_CCGOTPACKET;
        break;
      }
      if(!digitalRead(GDO0))
      {
      	m_state = IC_CCWAITINGRX;
      }
      if((((uint32_t)millis())-rxStartTime)>rx_timeout)
      {
        halRfStrobe(SIDLE);
        m_state = IC_CCIDLE;
        halRfStrobe(SFRX);
      }
      break;
  }
  return m_state;
}

unsigned char IC_CC1101::ReadState(void)
{
	unsigned char tmp1, tmp2;
	tmp1 = (halRfReadReg(MARCSTATE)&0x1F);
	tmp2 = (halRfReadReg(MARCSTATE)&0x1F);
	while (tmp1!=tmp2)
	{
		tmp1 = tmp2;
		tmp2 = (halRfReadReg(MARCSTATE)&0x1F);
	}
	return tmp1;
}

unsigned char IC_CC1101::Sleep(void)
{
	halRfStrobe(SIDLE);
	m_state=IC_CCIDLE;
	halRfStrobe(SFTX);
	halRfStrobe(SFRX);
	halRfStrobe(SPWD);
	return 1;
}

//Start/Stop receiving
unsigned char IC_CC1101::StartReceive(unsigned long timeout_ms)
{
  if (GetState() != IC_CCIDLE)
    return 0;
  halRfStrobe(SRX);
  if (timeout_ms)
    m_receiveTimeout = timeout_ms;
  rx_timeout = m_receiveTimeout;
  rxStartTime = ((uint32_t)millis());
  m_state = IC_CCWAITINGRX;
  return 1;
}
unsigned char IC_CC1101::StopReceive(void)
{
  if (GetState() == IC_CCWAITINGRX)
  {
    halRfStrobe(SIDLE);
    m_state = IC_CCIDLE;
  }
  else if (GetState() == IC_CCRECEIVING)
  {
    rx_timeout = 10;
    rxStartTime = ((uint32_t)millis());
  }
  else if (GetState() != IC_CCIDLE)
    return 0;
  return 1;
}

void IC_CC1101::ForceIdle(void)
{
	halRfStrobe(SIDLE);
	m_state = IC_CCIDLE;
	halRfStrobe(SFTX);
	halRfStrobe(SFRX);
}

//start packet transmission
unsigned char IC_CC1101::Transmit(const unsigned char *txBuf, unsigned char cnt)
{
  unsigned long i;
  if (GetState()!=IC_CCIDLE)
  {
    return 0;
  }
  
  halRfStrobe(STX);  //start send
  halRfWriteReg(TXFIFO,cnt);  //write len
  halRfWriteBurstReg(TXFIFO,txBuf,cnt);  //fill FIFO
  
  i = ((uint32_t)millis());
  //wait for transfer start
  while (!digitalRead(GDO0))
  {
  	if ((((uint32_t)millis())-i)>=10)
  	{
		  halRfStrobe(SIDLE);
		  m_state = IC_CCIDLE;
		  halRfStrobe(SFTX);
		  return 0;
		}
  }
  m_state = IC_CCSENDING;
  return 1;
}

//stop receive then start packet transmission
unsigned char IC_CC1101::SendPacket(const unsigned char *txBuf, unsigned char cnt)
{
  if (!StopReceive())
  {
  	return 0;
  }
  return Transmit(txBuf, cnt);
}

//are we currently transmitting data?
unsigned char IC_CC1101::TXBusy(void)
{
	if (GetState()==IC_CCSENDING) return 1;
	return 0;
}

//get RXFIFO content if any
unsigned char IC_CC1101::GetData(unsigned char *rxBuf)
{
  unsigned char cnt;
  if (GetState() != IC_CCGOTPACKET)
		return 0;
  cnt = halRfReadStatus(RXBYTES);
  if (cnt & 0x80)  // Overflow
  {
    cnt = 0;
  }
  else if (cnt)
  {
    halRfReadBurstReg(RXFIFO,rxBuf,cnt);
  }
	halRfStrobe(SIDLE);
  m_state = IC_CCIDLE;
  halRfStrobe(SFRX);
  return cnt;
}

//get RXFIFO content if any and start listening.
unsigned char IC_CC1101::ReadPacket(unsigned char *rxBuf)
{
	unsigned char ret;
	ret = GetData(rxBuf);
	StartReceive(0);
	return ret;
}

/********************************* Internal Functions  ***********************************/

//Write and read a byte
uint8_t IC_CC1101::SpiTransfer(unsigned char wr)
{
#if defined ARDUINO_RADINO32
  return SPI_intern.transfer(wr);
#else
  SPDR = wr;
  while (!(SPSR & (1<<SPIF)));
  return SPDR;
#endif
}

//write single byte register
void IC_CC1101::halRfWriteReg(unsigned char addr, unsigned char wr)
{
  startTransfer();
  SpiTransfer(addr);
  SpiTransfer(wr);
  digitalWrite(_SS_PIN, HIGH);
}

//write burst register
void IC_CC1101::halRfWriteBurstReg(unsigned char addr, const unsigned char *buf, unsigned char cnt)
{
  startTransfer();
  SpiTransfer(addr|WRITE_BURST);
  while(cnt--)
  {
    SpiTransfer(*buf++);
  }
  digitalWrite(_SS_PIN, HIGH);
}

//write strobe cmd
void IC_CC1101::halRfStrobe(unsigned char strobe)
{
  startTransfer();
  SpiTransfer(strobe);
  digitalWrite(_SS_PIN, HIGH);
}

//read single byte register
unsigned char IC_CC1101::halRfReadReg(unsigned char addr) 
{
  unsigned char ret;
  startTransfer();
  SpiTransfer(addr|READ_SINGLE);
  ret=SpiTransfer(0);
  digitalWrite(_SS_PIN, HIGH);
  return ret;
}

//read burst register into buffer
void IC_CC1101::halRfReadBurstReg(unsigned char addr, unsigned char *buf, unsigned char cnt)
{
  startTransfer();
  SpiTransfer(addr|READ_BURST);
  while(cnt--)
  {
    *buf++=SpiTransfer(0);
  }
  digitalWrite(_SS_PIN, HIGH);
}

//get status register
unsigned char IC_CC1101::halRfReadStatus(unsigned char addr)
{
  unsigned char ret;
  startTransfer();
  SpiTransfer(addr|READ_BURST);
  ret=SpiTransfer(0);
  digitalWrite(_SS_PIN, HIGH);
  return ret;
}


/************************** Configuration *********************************************/

//set config registers, copy and paste of RF-Studio generated code
void IC_CC1101::SetConfigRegisters(void) 
{
  //
  // Rf settings for CC1101
  //
  halRfWriteReg(IOCFG0,0x06);  //GDO0 Output Pin Configuration
  halRfWriteReg(FIFOTHR,0x47); //RX FIFO and TX FIFO Thresholds
  halRfWriteReg(PKTCTRL0,0x05);//Packet Automation Control
  halRfWriteReg(FSCTRL1,0x06); //Frequency Synthesizer Control
  //868.3 mHz
  halRfWriteReg(FREQ2,0x21);   //Frequency Control Word, High Byte
  halRfWriteReg(FREQ1,0x65);   //Frequency Control Word, Middle Byte
  halRfWriteReg(FREQ0,0x6A);   //Frequency Control Word, Low Byte
  halRfWriteReg(MDMCFG4,0xCA); //Modem Configuration
  halRfWriteReg(MDMCFG3,0x83); //Modem Configuration
  //halRfWriteReg(MDMCFG2,0x13); //Modem Configuration
  halRfWriteReg(MDMCFG2,0x1F); //Modem Configuration - Modulation GFSK, Manchester encode, 30/32 SyncWord detection if CS
  halRfWriteReg(DEVIATN,0x35); //Modem Deviation Setting
  MCSM1buf = 0x33;   //After TX -> RX
  halRfWriteReg(MCSM1,MCSM1buf);
  															//[5:4]CCA Mode (always,rssi,!receiving,rssi+recv), [3:2]After RX (idle,fstxon,TX,StayRX), [1:0]After TX (idle,fstxon,stayTX,RX)
  halRfWriteReg(MCSM0,0x18);   //Main Radio Control State Machine Configuration
  halRfWriteReg(FOCCFG,0x16);  //Frequency Offset Compensation Configuration
  halRfWriteReg(AGCCTRL2,0x43);//AGC Control
  halRfWriteReg(WORCTRL,0xFB); //Wake On Radio Control
  halRfWriteReg(FSCAL3,0xE9);  //Frequency Synthesizer Calibration
  halRfWriteReg(FSCAL2,0x2A);  //Frequency Synthesizer Calibration
  halRfWriteReg(FSCAL1,0x00);  //Frequency Synthesizer Calibration
  halRfWriteReg(FSCAL0,0x1F);  //Frequency Synthesizer Calibration
  halRfWriteReg(TEST2,0x81);   //Various Test Settings
  halRfWriteReg(TEST1,0x35);   //Various Test Settings
  halRfWriteReg(TEST0,0x09);   //Various Test Settings 
  
  // Aditional settings
  halRfWriteReg(IOCFG2,0x29);        //GDO2 Output Pin Configuration
  halRfWriteReg(PKTLEN,0x3D);        //Packet Length
  halRfWriteReg(PKTCTRL1,0x04);      //Packet Automation Control
  halRfWriteReg(PKTCTRL0,0x05);      //Packet Automation Control
  
  halRfWriteReg(FSCTRL0,0x00);       //Frequency Synthesizer Control
  halRfWriteReg(BSCFG,0x1C);         //Bit Synchronization Configuration
  halRfWriteReg(FREND1,0xB6);        //Front End RX Configuration
}

/************************** Configuration end *****************************************/

void IC_CC1101::setAddressFilter(uint8_t address)
{
  //halRfWriteReg(PKTCTRL1, halRfReadReg(PKTCTRL1) | 0x03); //0x00 and 0xFF broadcast
  halRfWriteReg(PKTCTRL1, halRfReadReg(PKTCTRL1) | 0x01); //no broadcasts
  halRfWriteReg(ADDR, address);
}
