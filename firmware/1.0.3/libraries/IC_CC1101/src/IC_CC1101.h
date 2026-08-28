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

#ifndef __IC_CC1101_h__
#define __IC_CC1101_h__
#include <Arduino.h>
#include <SPI.h>

//Pins and SPI setup
#if defined ARDUINO_RADINO32
#define GDO0      29
#define GDO2_B      30
#define GDO2_C      33
#define SS_PIN_B    33
#define SS_PIN_C    32
#define MISO_PIN  BOARD_SPI2_MISO_PIN
#define MOSI_PIN  BOARD_SPI2_MOSI_PIN
#define SCK_PIN   BOARD_SPI2_SCK_PIN
#else
#define SCK_PIN   SCK
#define MISO_PIN  MISO
#define MOSI_PIN  MOSI
#define SS_PIN    8
#define GDO0      9
#define GDO2      7
#endif

#define PATABLE_SIZE 8
#define PATABLE_DEFAULT 0xC0
//outputPower(dBm) frequency
//0x60: +0.1 433, -0.5 868, -1.1 915
//0xC0: +9.9 433, +10.7 868, +9.4 915

#define ICCC1101_PAKETSIZE 61  //CC1101 adds LEN, LQI, RSSI -- stay under fifo size of 64 byte (CC1101 buggy)
//Packet format delivered by the CC1101 RX
typedef struct __attribute__((packed))
{
  uint8_t len;
  uint8_t packetdata[ICCC1101_PAKETSIZE];
  uint16_t appended;
} transfer_t;

//Registers
#define IOCFG2       0x00        // GDO2 config
#define IOCFG1       0x01        // GDO1 config
#define IOCFG0       0x02        // GDO0 config
#define FIFOTHR      0x03        // RX/TX FIFO thresholds
#define SYNC1        0x04        // Sync word, high byte
#define SYNC0        0x05        // Sync word, low byte
#define PKTLEN       0x06        // Packet length
#define PKTCTRL1     0x07        // Packet handling 1
#define PKTCTRL0     0x08        // Packet handling 0
#define ADDR         0x09        // Address
#define CHANNR       0x0A        // Channel
#define FSCTRL1      0x0B        // Frequency synthesizer control 1
#define FSCTRL0      0x0C        // Frequency synthesizer control 0
#define FREQ2        0x0D        // Frequency control [23:16]
#define FREQ1        0x0E        // Frequency control [15:8]
#define FREQ0        0x0F        // Frequency control [7:0]
#define MDMCFG4      0x10        // Modem config 4
#define MDMCFG3      0x11        // Modem config 3
#define MDMCFG2      0x12        // Modem config 2
#define MDMCFG1      0x13        // Modem config 1
#define MDMCFG0      0x14        // Modem config 0
#define DEVIATN      0x15        // Modem deviation
#define MCSM2        0x16        // Main Control State Machine config 2
#define MCSM1        0x17        // Main Control State Machine config 1
#define MCSM0        0x18        // Main Control State Machine config 0
#define FOCCFG       0x19        // Frequency Offset Compensation config
#define BSCFG        0x1A        // Bit Synchronization config
#define AGCCTRL2     0x1B        // AGC control 2
#define AGCCTRL1     0x1C        // AGC control 1
#define AGCCTRL0     0x1D        // AGC control 0
#define WOREVT1      0x1E        // Event 0 timeout [15:8]
#define WOREVT0      0x1F        // Event 0 timeout [7:0]
#define WORCTRL      0x20        // Wake On Radio control
#define FREND1       0x21        // Front end RX config
#define FREND0       0x22        // Front end TX config
#define FSCAL3       0x23        // Frequency synthesizer calibration 3
#define FSCAL2       0x24        // Frequency synthesizer calibration 2
#define FSCAL1       0x25        // Frequency synthesizer calibration 1
#define FSCAL0       0x26        // Frequency synthesizer calibration 0
#define RCCTRL1      0x27        // RC config 1
#define RCCTRL0      0x28        // RC config 0
#define FSTEST       0x29        // Frequency synthesizer calibration control
#define PTEST        0x2A        // Production test
#define AGCTEST      0x2B        // AGC test
#define TEST2        0x2C        // Various test settings 2
#define TEST1        0x2D        // Various test settings 1
#define TEST0        0x2E        // Various test settings 0

//Strobe commands
#define SRES         0x30        // Reset chip.
#define SFSTXON      0x31        // Enable and calibrate frequency synthesizer (if MCSM0.FS_AUTOCAL=1).
                                 // If in RX/TX: Go to a wait state where only the synthesizer is
                                 // running (for quick RX / TX turnaround).
#define SXOFF        0x32        // Turn off crystal oscillator.
#define SCAL         0x33        // Calibrate frequency synthesizer and turn it off
                                 // (enables quick start).
#define SRX          0x34        // Enable RX. Perform calibration first if coming from IDLE and
                                 // MCSM0.FS_AUTOCAL=1.
#define STX          0x35        // In IDLE state: Enable TX. Perform calibration first if
                                 // MCSM0.FS_AUTOCAL=1. If in RX state and CCA is enabled:
                                 // Only go to TX if channel is clear.
#define SIDLE        0x36        // Exit RX / TX, turn off frequency synthesizer and exit
                                 // Wake-On-Radio mode if applicable.
#define SAFC         0x37        // Perform AFC adjustment of the frequency synthesizer
#define SWOR         0x38        // Start automatic RX polling sequence (Wake-on-Radio)
#define SPWD         0x39        // Enter power down mode when CSn goes high.
#define SFRX         0x3A        // Flush the RX FIFO buffer.
#define SFTX         0x3B        // Flush the TX FIFO buffer.
#define SWORRST      0x3C        // Reset real time clock.
#define SNOP         0x3D        // No operation. May be used to pad strobe commands to two
                                 // INT8Us for simpler software.
//STATUS REGISTERS
#define PARTNUM      0x30
#define VERSION      0x31
#define FREQEST      0x32
#define LQI          0x33
#define RSSI         0x34
#define MARCSTATE    0x35
#define WORTIME1     0x36
#define WORTIME0     0x37
#define PKTSTATUS    0x38
#define VCO_VC_DAC   0x39
#define TXBYTES      0x3A
#define RXBYTES      0x3B

//PATABLE,TXFIFO,RXFIFO
#define PATABLE      0x3E
#define TXFIFO       0x3F
#define RXFIFO       0x3F

//Cmd Flags
#define WRITE_BURST 0x40
#define READ_SINGLE 0x80
#define READ_BURST 0xC0

//State
enum IC_CC_STATE
{
  IC_CCIDLE = 0,
  IC_CCSENDING,
  IC_CCWAITINGRX,
  IC_CCRECEIVING,
  IC_CCGOTPACKET,
  IC_CCWAITIDLE,
};

//Class
class IC_CC1101
{
  private:
    uint8_t _GDO2=0xFF;
    uint8_t _SS_PIN=0xFF;
    IC_CC_STATE m_state;
    uint8_t m_maxPacketLength;
    uint32_t m_receiveTimeout;
    unsigned char m_paTableEntry = PATABLE_DEFAULT;
    unsigned char patable[PATABLE_SIZE];
    unsigned char MCSM1buf;
    
  public:
    float setFrequency(float desiredFreq);
    unsigned long setDatarate(unsigned long desiredDatarate);
    unsigned char setPaTable(unsigned char newValue);
    uint8_t wait_CCA(uint32_t timeout, uint32_t gap_us);
    unsigned char Transmit_wCCA(const unsigned char *txBuf, unsigned char cnt, uint32_t timeout, uint32_t gap_us);
    
    void Init(unsigned char maxPacketLength, uint32_t receiveTimeout);
    void Reset (void);
    IC_CC_STATE GetState(void);
    unsigned char ReadState(void);
    unsigned char Sleep(void);
    void ForceIdle(void);
    unsigned char StartReceive(unsigned long timeout_ms = 0);
    unsigned char StopReceive(void);
    
    //get RXFIFO content if any
    unsigned char GetData(unsigned char *rxBuf);
    //start packet transmission
    unsigned char Transmit(const unsigned char *txBuf, unsigned char cnt);
    unsigned char TXBusy(void);
    //get RXFIFO content if any else start listening
    unsigned char ReadPacket(unsigned char *rxBuf);
    //stop receive then start packet transmission
    unsigned char SendPacket(const unsigned char *txBuf, unsigned char cnt);

    void setAddressFilter(uint8_t address);

  private:
    void SetMandatoryConfig(void);
    void SetConfigRegisters(void);
    void startTransfer();
    unsigned char SpiTransfer(unsigned char wr);
    void halRfWriteReg(unsigned char addr, unsigned char wr);
    void halRfWriteBurstReg(unsigned char addr, const unsigned char *buf, unsigned char cnt);
    void halRfStrobe(unsigned char strobe);
    unsigned char halRfReadReg(unsigned char addr);
    void halRfReadBurstReg(unsigned char addr, unsigned char *buf, unsigned char cnt);
    unsigned char halRfReadStatus(unsigned char addr);
};

#endif	//__IC_CC1101_h__
