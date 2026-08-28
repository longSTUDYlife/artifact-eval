#ifndef __SPI_H__
#define __SPI_H__

#include <Arduino.h>
#include <stm32/l1/gpio.h>
#include <stm32/l1/rcc.h>
#include <stm32/l1/spi.h>

enum eSpiBitOrder {
  LSBFIRST = 0,
  MSBFIRST = 1,
};

enum eSpiMode {
  SPI_MODE0 = 0,
  SPI_MODE1 = 1,
  SPI_MODE2 = 2,
  SPI_MODE3 = 3,
};

enum eSpiClkDiv {
  SPI_CLOCK_DIV1 = 1,
  SPI_CLOCK_DIV2 = 2,
  SPI_CLOCK_DIV4 = 4,
  SPI_CLOCK_DIV8 = 8,
  SPI_CLOCK_DIV16 = 16,
  SPI_CLOCK_DIV32 = 32,
  SPI_CLOCK_DIV64 = 64,
  SPI_CLOCK_DIV128 = 128,
};

class SPISettings {
  public:
    SPISettings(uint32_t clock, enum eSpiBitOrder bitOrder, enum eSpiMode dataMode)
    {
      if (__builtin_constant_p(clock)) {
        init_AlwaysInline(clock, bitOrder, dataMode);
      } else {
        init_MightInline(clock, bitOrder, dataMode);
      }
    }
    SPISettings() {
      init_AlwaysInline(4000000, MSBFIRST, SPI_MODE0);
    }
  
  private:
    void init_MightInline(uint32_t clock, enum eSpiBitOrder bitOrder, enum eSpiMode dataMode)
    {
      init_AlwaysInline(clock, bitOrder, dataMode);
    }
    void init_AlwaysInline(uint32_t clock, enum eSpiBitOrder bitOrder, enum eSpiMode dataMode) __attribute__((__always_inline__))
    {
      prescaler = 1;
      while (clock<PCLK1 && prescaler<128)
      {
        prescaler *= 2;
        clock *= 2;
      }
      mode = SPI_NSS_SOFTWARE | SPI_NSS_HIGH | SPI_MASTER | SPI_ENABLE;
      mode |= (bitOrder==LSBFIRST)?SPI_LSB_FIRST:0;
      switch(dataMode)
      {
        default:
        case SPI_MODE0: break;
        case SPI_MODE1: mode|=SPI_CLOCK_PHASE; break;
        case SPI_MODE3: mode|=SPI_CLOCK_PHASE;
        case SPI_MODE2: mode|=SPI_CLOCK_POLARITY; break;
      }
    }
    int prescaler;
    int mode;
  
  friend class SPIClass;
};

class SPIClass
{
  protected:
    unsigned long m_spiDevice;
    int m_prescaler;
    int m_mode;
  public:
    //todo interrupt handling bound to start/end transaction
    void usingInterrupt(uint8_t interruptNumber)
    {}
    void notUsingInterrupt(uint8_t interruptNumber)
    {}
    
    SPIClass(unsigned long spiDevice);
    
    void begin();
    void end();
    
    void beginTransaction(SPISettings settings);
    void endTransaction(void);
    
    void configApply();
    void configPins();
    void setBitOrder(enum eSpiBitOrder bitOrder);
    void setClockDivider(enum eSpiClkDiv clkDiv);
    void setClock(unsigned long clock);
    void setDataMode(enum eSpiMode dataMode);
    
    int transfer(unsigned short dat);
    int transfer(void *buf, size_t count);
};

extern SPIClass SPI_intern;
extern SPIClass SPI;

#endif