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

#include <Arduino.h>
#include <IC_CC1101.h>

#define ICCC1101BRIDGE_UARTBUF_SIZE 256  //Buffer for UART in
#define ICCC1101BRIDGE_RADIOBUF_SIZE 256  //Buffer for radio in

#define ICCC1101BRIDGE_DEFAULT_FREQ 868.3
#define ICCC1101BRIDGE_DEFAULT_DATARATE 38400
#define ICCC1101BRIDGE_DEFAULT_PATABLE 0x60

/****************** Packet structures *******************/
#define ICCC1101BRIDGE_MAXDATALEN (ICCC1101_PAKETSIZE-sizeof(ICCC1101BRIDGE_header_t))
#define ICCC1101BRIDGE_PING_HOPC 0xFF
#define ICCC1101BRIDGE_PONG_HOPC 0xFE

//Header for our data packets
typedef struct __attribute__((packed))
{
  uint8_t nwid;
  uint8_t src;
  uint8_t dest;
  uint8_t seq;
  uint8_t pseq;
  uint8_t hopc;		//0xFF used as ping, 0xFE used as pong
  uint8_t len;
  uint8_t crc;
} ICCC1101BRIDGE_header_t;

//Structure of our data packets
typedef struct __attribute__((packed))
{
  ICCC1101BRIDGE_header_t header;
  uint8_t data[ICCC1101BRIDGE_MAXDATALEN];
} ICCC1101BRIDGE_packet_t;

class IC_CC1101_Bridge
{
	public:
		IC_CC1101 wireless;  //The CC1101 device
		
  	IC_CC1101_Bridge(float pFreq, unsigned long dataRate, unsigned char paTable);
    IC_CC1101_Bridge();
    
    void setConfig(uint8_t nwId, uint8_t myAddr, uint8_t tAddr, uint8_t accBC);
    void useErrLed(uint8_t pin);
    
    void init(uint8_t nwId, uint8_t myAddr, uint8_t tAddr, uint8_t accBC);
    void end();
    void start();
    void stop();
    void handle();
    
  	uint16_t txBufFree();
  	uint8_t write(uint8_t ch);
  	uint16_t print(String str);
  	
  	uint16_t available();
  	uint8_t read();
  	
  	bool startPing(uint8_t nwId, uint8_t myAddr, uint8_t tAddr);
  	int8_t checkPingResult(uint8_t nwId, uint8_t myAddr, uint8_t tAddr, uint8_t * otherID, float * otherRSSI, float * ourRSSI);
    
  private:
  	transfer_t RX_buffer;
		ICCC1101BRIDGE_packet_t TX_packet;
    uint8_t m_nwId;
    uint8_t m_myAddr;
    uint8_t m_tAddr;
    uint8_t m_accBC;
    float m_freq;
    unsigned long m_dataRate;
    unsigned char m_paTable;
    uint8_t m_useLed=0xFF;
	  uint32_t uart_timeout = 0;
	  unsigned long resend_to = 0;  //delay before send
	  uint8_t currentSequenceNumber=0;  //currently pending sequence number
	  uint8_t lastAckedSequenceNumber=0;  //last succesfully sent sequence number
	  uint8_t currentpartnerseqnr=0;  //partners sequence number received in last packet
	  uint8_t lastpartnerseqnr=0;  //last partner sequence number we sent an ack for
	  uint8_t retrycnt=0;  //counter for retries
	  uint8_t txRestart=0;  //restart after failed transmit
	  ICCC1101BRIDGE_packet_t * pPacket;  //to access packet information within buffer
	  ICCC1101BRIDGE_header_t * pHeader;  //to access header information within buffer
	  float rssi;
	  uint32_t pingStart = 0;
	  
  	bool ishexch(unsigned char ch);
		unsigned char fromhexch(unsigned char ch);
		
  public:
    void setUartBuf(char * buf, unsigned short len);
    void setRadioBuf(char * buf, unsigned short len);
    
  private:
    unsigned short uartBufSize = ICCC1101BRIDGE_UARTBUF_SIZE;
    char _uartBuf[ICCC1101BRIDGE_UARTBUF_SIZE] = {0,};
  	char * uartBuf = _uartBuf;
		unsigned short uartBufWrite = 0;
		unsigned short uartBufRead = 0;
		unsigned short uartBuf_Len() { return ((uartBufWrite+uartBufSize-uartBufRead)%uartBufSize); }
		unsigned short uartBuf_available() { return uartBuf_Len(); }
		bool uartBuf_NE() { return (uartBufWrite!=uartBufRead?1:0); }
		bool uartBuf_NF() { return	(uartBuf_NextWrite()!=uartBufRead); }
		void uartBuf_Clear() { uartBufRead=uartBufWrite; }
		unsigned short uartBuf_NextRead() { return ((uartBufRead+1)%uartBufSize); }
		unsigned short uartBuf_NextWrite() { return ((uartBufWrite+1)%uartBufSize); }
		unsigned short uartBuf_Free() { return (uartBufSize-uartBuf_Len()); }
		uint8_t uartBuf_read();
		uint8_t uartBuf_write(uint8_t chr);
		uint16_t uartBuf_print(String str);
    
    unsigned short radioBufSize = ICCC1101BRIDGE_RADIOBUF_SIZE;
    char _radioBuf[ICCC1101BRIDGE_RADIOBUF_SIZE] = {0,};
		char * radioBuf = _radioBuf;
		unsigned short radioBufWrite = 0;
		unsigned short radioBufRead = 0;
		unsigned short radioBuf_Len() { return ((radioBufWrite+radioBufSize-radioBufRead)%radioBufSize); }
		unsigned short radioBuf_available() { return radioBuf_Len(); }
		bool radioBuf_NE() { return (radioBufWrite!=radioBufRead?1:0); }
		bool radioBuf_NF() { return (radioBuf_NextWrite()!=radioBufRead); }
		void radioBuf_Clear() { radioBufRead=radioBufWrite; }
		unsigned short radioBuf_NextRead() { return ((radioBufRead+1)%radioBufSize); }
		unsigned short radioBuf_NextWrite() { return ((radioBufWrite+1)%radioBufSize); }
		unsigned short radioBuf_Free() { return (radioBufSize-radioBuf_Len()); }
		uint8_t radioBuf_read();
		uint8_t radioBuf_write(uint8_t chr);
		uint16_t radioBuf_print(String str);
};
