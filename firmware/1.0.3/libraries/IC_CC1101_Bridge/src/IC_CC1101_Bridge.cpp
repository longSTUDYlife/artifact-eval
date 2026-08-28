/*
  radino CC1101 Bridge
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

#include "IC_CC1101_Bridge.h"

//Write messages on dbgSerial. It has to be initialized elsewhere.
//0:none
//1:Errors
//2:Information
#define ICCC1101BRIDGE_DBGLVL 0
#define ICCC1101BRIDGE_dbgSerial Serial1

/***************************** Debug messages ***********************/

//Write out errors on dbgSerial
#if ICCC1101BRIDGE_DBGLVL>=1
  #define DBGERR(x) ICCC1101BRIDGE_dbgSerial.print(x)
  #define DBGERR2(x,y) ICCC1101BRIDGE_dbgSerial.print(x,y)
  #define DBGERRWRITE(x) ICCC1101BRIDGE_dbgSerial.write(x)
#else
  #define DBGERR(x) do{}while(0)
  #define DBGERR2(x,y) do{}while(0)
  #define DBGERRWRITE(x) do{}while(0)
#endif

//Write out information on dbgSerial
#if ICCC1101BRIDGE_DBGLVL>=2
  #define DBGINFO(x) ICCC1101BRIDGE_dbgSerial.print(x)
  #define DBGINFO2(x,y) ICCC1101BRIDGE_dbgSerial.print(x,y)
  #define DBGWRITE(x) ICCC1101BRIDGE_dbgSerial.write(x)
#else
  #define DBGINFO(x) do{}while(0)
  #define DBGINFO2(x,y) do{}while(0)
  #define DBGWRITE(x) do{}while(0)
#endif

//delay between packets:
//modify according to expected serial speed and partner reaction speed
//(prevent bufferoverflow, but allow other side to empty receive buffer)
//send after receive: PACKET_GAP
//send after non acked receive: PACKET_GAP+PACKET_PAUSE
//send after sent self: RESEND_TIMEOUT (Is overwritten on ACK receive with PACKET_GAP)
//send after sendError: PACKET_GAP_ERR
//send after sent with noAck: PACKET_GAP_NOACK
#define UART_TIMEOUT 10  //UART is considered empty after this time in ms
#define RESEND_TIMEOUT 75  //Wait this time in ms for an radio ack
#define PACKET_GAP 10 //Delay between receive and outgoing ack
#define PACKET_PAUSE 10 //Delay for ACK when buffer nearly full (Added to GAP)
#define PACKET_GAP_NOACK 50 //Delay between packets when no ack is expected
#define PACKET_GAP_ERR 25 //Delay on failed transmit

#define SEND_RETRIES_ON_ERROR 3 //How often we try to transmit after transmission start failed (e.g. channel busy)

#define RECEIVE_TO 1000  //Wait at max this long in ms for packet arrival
#define RETRIES 5
#define HOPCOUNT 1

uint16_t IC_CC1101_Bridge::txBufFree()
{
  return uartBuf_Free();
}

uint8_t IC_CC1101_Bridge::write(uint8_t ch)
{
  if (uartBuf_NF())
  {
    uartBuf_write(ch);
    uart_timeout = millis();
    return 1;
  }
  return 0;
}

uint16_t IC_CC1101_Bridge::print(String str)
{
  return uartBuf_print(str);
}

uint16_t IC_CC1101_Bridge::available()
{
  return radioBuf_available();
}

uint8_t IC_CC1101_Bridge::read()
{
  return radioBuf_read();
}


bool IC_CC1101_Bridge::startPing(uint8_t nwId, uint8_t myAddr, uint8_t tAddr)
{
	wireless.Sleep();
	//send ping
  pPacket = &TX_packet;
  pHeader = &pPacket->header;
  pHeader->nwid = nwId;
  pHeader->src = myAddr;
  pHeader->dest = tAddr;
  pHeader->seq = 0;
  pHeader->pseq = 0;
  pHeader->hopc = ICCC1101BRIDGE_PING_HOPC;
  pHeader->len = 0;
  
  //TODO calculate real crc
  pHeader->crc=0;
  uint8_t crc=42;
  uint8_t i;
  for( i=0 ; i<(sizeof(ICCC1101BRIDGE_header_t)+pHeader->len) ; i++)
  {
    crc+=((uint8_t*)&TX_packet)[i];
  }
  pHeader->crc=crc;
	
	if (wireless.Transmit_wCCA((uint8_t*)&TX_packet, i, 50, 1250))
	{
		pingStart = millis();
		DBGINFO("PINGOUT\r\n");
		return true;
	}
	pingStart = millis()-2001;
	DBGERR("PINGERR\r\n");
	return false;
}

//uint32_t IC_CC1101_Bridge::checkPingResult(uint8_t nwId, uint8_t myAddr, uint8_t tAddr)
int8_t IC_CC1101_Bridge::checkPingResult(uint8_t nwId, uint8_t myAddr, uint8_t tAddr, uint8_t * otherID, float * otherRSSI, float * ourRSSI)
{
	uint8_t cnt,i,crc;
	uint32_t ret;
	
	if ((millis()-pingStart)>2000)
	{
		wireless.Sleep();
		DBGINFO("DiscTO\r\n");
		return 0;
	}
	
	if (cnt=wireless.ReadPacket((uint8_t*)&RX_buffer))
	{
/**** Pong Packet checks start ****/
if( RX_buffer.len!=(1+sizeof(ICCC1101BRIDGE_header_t)) )
{
DBGERR("!SIZE\r\n");
return -1;
}
//Set pointers to packet parts
pPacket = (ICCC1101BRIDGE_packet_t*)&RX_buffer.packetdata;
pHeader = &pPacket->header;
if( (pHeader->nwid!=nwId) || (pHeader->dest!=myAddr) || ((tAddr!=0xFF)&&(pHeader->src!=tAddr)) )
{
DBGERR("ADDR!\r\n");
return -2;
}
if( pHeader->hopc!=ICCC1101BRIDGE_PONG_HOPC )
{
DBGERR("TYPE!\r\n");
return -3;
}
//Todo actually implement crc check
cnt = pHeader->crc;
pHeader->crc = 0;
crc=42;
for(i=0 ; i<RX_buffer.len ; i++) crc+=((uint8_t*)pPacket)[i];
//valid packet crc
if (crc!=cnt)
{
DBGERR("!CRC\r\n");
return -4;
}

//Calc RSSI
crc = pPacket->data[pHeader->len]; rssi = crc; if (crc&0x80) rssi -= 256; rssi /= 2; rssi -= 74;

*otherID = pHeader->src;
*otherRSSI = -(((float)pPacket->data[0])/2);
*ourRSSI = rssi;
return 1;
/**** Pong Packet checks end ****/
	}
	return -5;
}

IC_CC1101_Bridge::IC_CC1101_Bridge(float pFreq, unsigned long dataRate, unsigned char paTable)
{
	m_useLed = 0xFF;m_nwId=0;m_myAddr=0xFF;m_tAddr=0xFF;m_accBC=1;m_freq=pFreq;m_dataRate=dataRate;m_paTable=paTable;
}

IC_CC1101_Bridge::IC_CC1101_Bridge()
{
  m_useLed = 0xFF;m_nwId=0;m_myAddr=0xFF;m_tAddr=0xFF;m_accBC=1;
  m_freq=ICCC1101BRIDGE_DEFAULT_FREQ;
  m_dataRate=ICCC1101BRIDGE_DEFAULT_DATARATE;
  m_paTable=ICCC1101BRIDGE_DEFAULT_PATABLE;
}

void IC_CC1101_Bridge::setUartBuf(char * buf, unsigned short len)
{
  uartBufSize = len;
  uartBuf = buf;
  uartBufWrite = 0;
  uartBufRead = 0;
}
void IC_CC1101_Bridge::setRadioBuf(char * buf, unsigned short len)
{
  radioBufSize = len;
  radioBuf = buf;
  radioBufWrite = 0;
  radioBufRead = 0;
}

void IC_CC1101_Bridge::useErrLed(uint8_t pin)
{
  if (m_useLed!=0xFF)
  { pinMode(m_useLed,INPUT); }
  if (pin!=0xFF)
  {
  	pinMode(pin,OUTPUT);
  	digitalWrite(pin,LOW);
  }
  m_useLed = pin;
}

void IC_CC1101_Bridge::start()
{
	wireless.Init(ICCC1101_PAKETSIZE,RECEIVE_TO);
  wireless.setFrequency(m_freq);
  wireless.setDatarate(m_dataRate);
  wireless.setPaTable(m_paTable);
  wireless.StartReceive(0);
}

void IC_CC1101_Bridge::stop()
{
  wireless.Sleep();
}

void IC_CC1101_Bridge::end()
{
	wireless.Sleep();
}

void IC_CC1101_Bridge::init(uint8_t nwId, uint8_t myAddr, uint8_t tAddr, uint8_t accBC)
{
  wireless.Init(ICCC1101_PAKETSIZE,RECEIVE_TO);
  wireless.setFrequency(m_freq);
  wireless.setDatarate(m_dataRate);
  wireless.setPaTable(m_paTable);
  setConfig(nwId, myAddr, tAddr, accBC);
}

void IC_CC1101_Bridge::setConfig(uint8_t nwId, uint8_t myAddr, uint8_t tAddr, uint8_t accBC)
{
  m_nwId = nwId; m_myAddr = myAddr; m_tAddr = tAddr; m_accBC = accBC?1:0;
  currentSequenceNumber = lastAckedSequenceNumber = currentpartnerseqnr = lastpartnerseqnr = retrycnt = txRestart = 0;
}

uint8_t IC_CC1101_Bridge::uartBuf_read()
{
  uint8_t ret = '\0';
  if (uartBuf_NE())
  {
    ret = uartBuf[uartBufRead];
    uartBufRead = uartBuf_NextRead();
  }
  return ret;
}
uint8_t IC_CC1101_Bridge::uartBuf_write(uint8_t chr)
{
  if (uartBuf_NF())
  {
    uartBuf[uartBufWrite] = chr;
    uartBufWrite = uartBuf_NextWrite();
    return 1;
  }
  return 0;
}
uint16_t IC_CC1101_Bridge::uartBuf_print(String str)
{
  uint16_t i=0;
  while(str[i]!='\0')
  {
    if (!uartBuf_write(str[i])) break;
    i++;
  }
  return i;
}

uint8_t IC_CC1101_Bridge::radioBuf_read()
{
  uint8_t ret = '\0';
  if (radioBuf_NE())
  {
    ret = radioBuf[radioBufRead];
    radioBufRead = radioBuf_NextRead();
  }
  return ret;
}
uint8_t IC_CC1101_Bridge::radioBuf_write(uint8_t chr)
{
  if (radioBuf_NF())
  {
    radioBuf[radioBufWrite] = chr;
    radioBufWrite = radioBuf_NextWrite();
    return 1;
  }
  return 0;
}
uint16_t IC_CC1101_Bridge::radioBuf_print(String str)
{
  uint16_t i=0;
  while(str[i]!='\0')
  {
    if (!radioBuf_write(str[i])) break;
    i++;
  }
  return i;
}

bool IC_CC1101_Bridge::ishexch(unsigned char ch)
{
	if (ch>='0' && ch<='9')  return true;
  if (ch>='a' && ch<='f')  return true;
  if (ch>='A' && ch<='F')  return true;
  return false;
}

//Convert hex char to uint
unsigned char IC_CC1101_Bridge::fromhexch(unsigned char ch)
{
  if (ch>='0' && ch<='9')  return (ch-'0');
  if (ch>='a' && ch<='f')  return (ch-'a'+10);
  if (ch>='A' && ch<='F')  return (ch-'A'+10);
  return 0;
}

void IC_CC1101_Bridge::handle()
{
  unsigned char crc, cnt;
  unsigned short i;
  //CC1101 finished reception of a packet
  if (cnt=wireless.ReadPacket((uint8_t*)&RX_buffer))  //read the packet if any (returns length of packet, 0 if no packet available)
  {
#if ICCC1101BRIDGE_DBGLVL>=2
    DBGINFO2(millis()&0xFF,HEX); DBGINFO(" Receive("); DBGINFO(cnt); DBGINFO("): ");
    for (i=0;i<cnt && i<sizeof(ICCC1101BRIDGE_header_t)+1;i++) { DBGINFO2(((uint8_t*)&RX_buffer)[i],HEX); DBGWRITE(' '); }
    DBGINFO("-> ");
    for (i=sizeof(ICCC1101BRIDGE_header_t)+1;i<cnt;i++)
    {
      DBGINFO2(((uint8_t*)&RX_buffer)[i],HEX);DBGWRITE(':');DBGWRITE(((uint8_t*)&RX_buffer)[i]);DBGWRITE(' ');
    }
#endif
    
    //Is the packet length valid?
    if( (RX_buffer.len<sizeof(ICCC1101BRIDGE_header_t))||(RX_buffer.len>sizeof(ICCC1101BRIDGE_packet_t)) )
        DBGERR("!SIZE\r\n");
    else
    {
      //Set pointers to packet parts
      pPacket = (ICCC1101BRIDGE_packet_t*)&RX_buffer.packetdata;
      pHeader = &pPacket->header;
      
      //our network? our ID addressed or (broadcast and we accept boradcasts)?
      if( (pHeader->nwid!=m_nwId) || ((pHeader->dest!=0xFF)&&(pHeader->dest!=m_myAddr)) || ((pHeader->dest==0xFF)&&(!m_accBC)) )
          DBGERR("!DEST-ID\r\n");
      else
      {
        //Todo actually implement crc check
        cnt = pHeader->crc;
        pHeader->crc = 0;
        crc=42;
        for(i=0 ; i<RX_buffer.len ; i++) crc+=((uint8_t*)pPacket)[i];
        
        //valid packet crc
        if (crc!=cnt)
            DBGERR("!CRC\r\n");
        else
        {
          //Calculate RSSI
          crc = pPacket->data[pHeader->len]; rssi = crc; if (crc&0x80) rssi -= 256; rssi /= 2; rssi -= 74;
          
          if (pHeader->hopc==ICCC1101BRIDGE_PONG_HOPC)
          {
          	DBGERR("!PONG\r\n");
          	return;
          }
          if (pHeader->hopc==ICCC1101BRIDGE_PING_HOPC)
          {
          	wireless.Sleep();
            //send ping
			      pPacket = &TX_packet;
			      pHeader = &pPacket->header;
			      pHeader->nwid = m_nwId;
			      pHeader->src = m_myAddr;
			      pHeader->dest = ((ICCC1101BRIDGE_packet_t*)&RX_buffer.packetdata)->header.src;
			      pHeader->seq = 0;
			      pHeader->pseq = 0;
			      pHeader->hopc = ICCC1101BRIDGE_PONG_HOPC;
			      pHeader->len = 1;
			      
			      if (rssi<0) pPacket->data[0]=-(2*rssi);
			      else pPacket->data[0]=(2*rssi);
			      
			      //TODO calculate real crc
	          pHeader->crc=0;
	          crc=42;
	          for( i=0 ; i<(sizeof(ICCC1101BRIDGE_header_t)+pHeader->len) ; i++)
	          {
	            crc+=((uint8_t*)&TX_packet)[i];
	          }
	          pHeader->crc=crc;
			    	
			    	if (wireless.Transmit_wCCA((uint8_t*)&TX_packet, i, 2250, 1250))
			    	{
			    		DBGINFO("Pong\r\n");
			    		resend_to = millis()+PACKET_GAP_NOACK;
			    	} else {
			    		DBGERR("TXERR\r\n");
			    		resend_to = millis()+PACKET_GAP_ERR;
			    	}
			    	return;
		    	}
          
#if ICCC1101BRIDGE_DBGLVL>=2
          DBGINFO(" RSSI: "); DBGINFO(rssi); DBGINFO("dBm");
#endif
					
          resend_to = millis()+PACKET_GAP;  //short delay after receive
          
          //No sequencenumber managment on broadcasts
          if (pHeader->dest!=0xFF && pHeader->src!=0xFF)
          {
            currentpartnerseqnr = pHeader->seq;  //remember their sequence number
            if (currentSequenceNumber == pHeader->pseq) {if(m_useLed!=0xFF)digitalWrite(m_useLed,LOW);}  //got ack for our last data
            lastAckedSequenceNumber = pHeader->pseq;  //remember last acked sequence number
          }
          
          //did they alter their sequence number? implies they got an ack and sent new data
          if (pHeader->dest==0xFF || pHeader->src==0xFF || currentpartnerseqnr!=lastpartnerseqnr)
          {
            for(i=0 ; i<pHeader->len && radioBuf_Free() ; i++)  //fill uart output buffer
            {
              radioBuf_write(pPacket->data[i]);
            }
            //If buffer is filled beyond threshold
            if (radioBuf_Len()>=ICCC1101BRIDGE_MAXDATALEN && pHeader->dest!=0xFF && pHeader->src!=0xFF)
              resend_to += PACKET_PAUSE;  //delayed ack, allows us to drain some buffer content
          }
          //got packet with same sequence number we already acked -> retransmit ack
          else if (pHeader->len && currentpartnerseqnr==lastpartnerseqnr)
          {
            lastpartnerseqnr--;
          }
          
#if ICCC1101BRIDGE_DBGLVL>=2
          DBGINFO(" buf: "); DBGINFO(radioBuf_Len()); DBGINFO("\r\n");
#endif
        }
      }
    }
  }
  
  //delay between packets is over
  if (resend_to<millis())
  {
    //Last send not finished yet
    if (wireless.TXBusy())
    {
      DBGINFO("Still sending\r\n");
      resend_to= millis()+2;
    }
    //(restart last transmit OR have Data AND datablock on uart complete) OR buffer over threshold OR waiting for ack OR partner sent new data
    else if (txRestart || ((uartBuf_NE() && (millis()-uart_timeout)>UART_TIMEOUT) || uartBuf_Len()>=ICCC1101BRIDGE_MAXDATALEN || currentSequenceNumber!=lastAckedSequenceNumber || currentpartnerseqnr!=lastpartnerseqnr))
    {
      DBGINFO2(millis()&0xFF,HEX); DBGINFO(" Buf:"); DBGINFO(uartBuf_Len()); DBGINFO(" ");
      
      //Init packet with common values
      pPacket = &TX_packet;
      pHeader = &pPacket->header;
      pHeader->nwid = m_nwId;
      pHeader->src = m_myAddr;
      pHeader->dest = m_tAddr;
      pHeader->pseq = currentpartnerseqnr;
      pHeader->hopc = HOPCOUNT;
      
      //forced retry caused by transmit error
      if (txRestart)
      {
        {if(m_useLed!=0xFF)digitalWrite(m_useLed,HIGH);}
        DBGINFO("Repeat (");
        lastpartnerseqnr = currentpartnerseqnr;  //update ack signal
      }
      //Broadcasts are not acked so update status as if received one
      else if (pHeader->src==0xFF || pHeader->dest==0xFF)
      {
        lastpartnerseqnr=currentpartnerseqnr;
        lastAckedSequenceNumber=currentSequenceNumber;
      }
      
      //still no ack for last sent sequence number
      if (currentSequenceNumber!=lastAckedSequenceNumber)
      {
        //retried too often
        if (retrycnt>=RETRIES)
        {
          {if(m_useLed!=0xFF)digitalWrite(m_useLed,HIGH);}
          lastAckedSequenceNumber = currentSequenceNumber;  //start next transmission with a different sequence number
          pHeader->seq = currentSequenceNumber;
          retrycnt = 0;  //reset retries
          uartBuf_Clear();  //kill buffer
          DBGERR("reached max Retries\r\n");
          return;
        } else {
          {if(m_useLed!=0xFF)digitalWrite(m_useLed,HIGH);}
          DBGINFO("Retry #"); DBGINFO(retrycnt); DBGINFO(" (");
          lastpartnerseqnr = currentpartnerseqnr;  //update ack signal
          retrycnt++;
        }
      }
      
      //all our data was acked
      else
      {
        lastpartnerseqnr = currentpartnerseqnr;  //update ack signal
        
        //Next data chunk [ (we have data AND uart timed out) OR uart beyond threshold ]
        if ((uartBuf_NE() && (millis()-uart_timeout)>UART_TIMEOUT) || uartBuf_Len()>=ICCC1101BRIDGE_MAXDATALEN)
        {
          //Refill packet buffer with new data
          do {
            pHeader->seq = ++currentSequenceNumber;  //new data -> increase sequence number
          } while (!currentSequenceNumber);  //0 is reserved for "not used before"
          
          //No ACK on broadcasts -> fake one
          //if (m_myAddr==0xFF || m_tAddr==0xFF)
          //    lastAckedSequenceNumber=currentSequenceNumber;
          
          pHeader->len = uartBuf_Len()<ICCC1101BRIDGE_MAXDATALEN ? uartBuf_Len() : ICCC1101BRIDGE_MAXDATALEN;  //amount of data to be transmitted
          for ( i=0 ; i<pHeader->len ; i++ )
          {
            pPacket->data[i] = uartBuf_read();
          }
          
          DBGINFO("Sending (");
          retrycnt = 0;
        }
        
        //no new data to send -> send only ack
        else
        {
          //no ACKs on broadcasts
          if (m_myAddr==0xFF || m_tAddr==0xFF)
          {
            DBGINFO("NoAck on BC\r\n");
            return;
          }
          DBGINFO("SendAck (");
          pHeader->len = 0;
        }
      }
      
      //TODO calculate real crc
      pHeader->crc=0;
      crc=42;
      for( i=0 ; i<(sizeof(ICCC1101BRIDGE_header_t)+pHeader->len) ; i++)
      {
        crc+=((uint8_t*)&TX_packet)[i];
      }
      pHeader->crc=crc;
      
      DBGINFO(i); DBGINFO(","); DBGINFO(pHeader->len); DBGINFO(")\r\n");
      
      static uint8_t txErrorCount = 0;
      //Transmit packet
      if (wireless.Transmit_wCCA((uint8_t*)&TX_packet, i, 20, 1500))
      {
        if (txRestart) {if(m_useLed!=0xFF)digitalWrite(m_useLed,LOW);}
        txRestart = txErrorCount = 0;
        //Smaller delay on boradcasts because there is no ACK
        if (pHeader->src==0xFF || pHeader->dest==0xFF)
          resend_to = millis()+PACKET_GAP_NOACK;
        else
          resend_to = millis()+RESEND_TIMEOUT;
      } else {
        if ((txErrorCount++)>SEND_RETRIES_ON_ERROR)
        {
          stop();
          DBGERR("Wireless Reset\r\n");
          delay(10);
          start();
        } else {
        	wireless.StartReceive(0);
          //transmit error .. retry later
          txRestart = (txRestart) ? (txRestart-1) : SEND_RETRIES_ON_ERROR;
          resend_to = millis()+PACKET_GAP_ERR;
          DBGERR("ERROR Transmit\r\n");
        }
      }
    }
  }
}
