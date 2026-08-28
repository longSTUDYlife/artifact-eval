#include "SPI.h"

SPIClass SPI_intern(SPI2);
SPIClass SPI(SPI1);

SPIClass::SPIClass(unsigned long spiDevice)
{
  m_spiDevice = spiDevice;
  m_prescaler = 4;
  m_mode = SPI_NSS_SOFTWARE | SPI_NSS_HIGH | SPI_MASTER | SPI_ENABLE;
};
void SPIClass::begin()
{
  configPins();
  if (m_spiDevice == SPI1)
  {
    rcc_enable_clock(RCC_SPI1);
  }
  if (m_spiDevice == SPI2)
  {
    rcc_enable_clock(RCC_SPI2);
  }
  configApply();
};

void SPIClass::end()
{
  if (m_spiDevice == SPI1)
  {
    spi_disable(SPI1);
    rcc_disable_clock(RCC_SPI1);
  }
  if (m_spiDevice == SPI2)
  {
    spi_disable(SPI2);
    rcc_disable_clock(RCC_SPI2);
  }
}

void SPIClass::beginTransaction(SPISettings settings)
{
  m_prescaler = settings.prescaler;
  m_mode = settings.mode;
  configApply();
}

void SPIClass::endTransaction(void)
{}

void SPIClass::configApply()
{
  if (m_spiDevice == SPI1)
  {
    spi_set_mode(SPI1, m_prescaler, m_mode);
  }
  if (m_spiDevice == SPI2)
  {
    spi_set_mode(SPI2, m_prescaler, m_mode);
  }
}

void SPIClass::configPins()
{
  if (m_spiDevice == SPI1)
    gpio_config_altfn(GPIO_SPI1_2, GPIO_PUSHPULL, GPIO_40MHZ, GPIO_NOPUPD, GPIO_PB(SPI1_SCK, SPI1_MISO, SPI1_MOSI));
  if (m_spiDevice == SPI2)
    gpio_config_altfn(GPIO_SPI1_2, GPIO_PUSHPULL, GPIO_40MHZ, GPIO_NOPUPD, GPIO_PB(SPI2_SCK, SPI2_MISO, SPI2_MOSI));
}

void SPIClass::setBitOrder(enum eSpiBitOrder bitOrder)
{
  if (bitOrder==LSBFIRST)
    m_mode |= SPI_LSB_FIRST;
  else
    m_mode &= ~(SPI_LSB_FIRST);
  configApply();
}

void SPIClass::setClockDivider(enum eSpiClkDiv clkDiv)
{
  m_prescaler = clkDiv;
  configApply();
}

void SPIClass::setClock(unsigned long clock)
{
  m_prescaler = 1;
  while (clock<PCLK1 && m_prescaler<128)
  {
    m_prescaler *= 2;
    clock *= 2;
  }
  configApply();
}

void SPIClass::setDataMode(enum eSpiMode dataMode)
{
  m_mode &= ~(SPI_CLOCK_POLARITY | SPI_CLOCK_PHASE);
  switch(dataMode)
  {
    default:
    case SPI_MODE0: break;
    case SPI_MODE1: m_mode|=SPI_CLOCK_PHASE; break;
    case SPI_MODE3: m_mode|=SPI_CLOCK_PHASE;
    case SPI_MODE2: m_mode|=SPI_CLOCK_POLARITY; break;
  }
  configApply();
}

int SPIClass::transfer(unsigned short dat)
{
  if (m_spiDevice == SPI1) return spi_transfer(SPI1, dat);
  if (m_spiDevice == SPI2) return spi_transfer(SPI2, dat);
  return -1;
}

int SPIClass::transfer(void *buf, size_t count)
{
  unsigned char * p = (unsigned char *)buf;
  if (m_spiDevice == SPI1)
    while (--count > 0)
      if (0 > (*p++ = spi_transfer(SPI1, *p)))
        return -1;
  if (m_spiDevice == SPI2)
    while (--count > 0)
      if (0 > (*p++ = spi_transfer(SPI2, *p)))
        return -1;
  return -1;
}
