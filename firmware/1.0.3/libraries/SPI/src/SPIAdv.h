#ifndef __SPIADV_H__
#define __SPIADV_H__

#include "SPI.h"
#include <stm32/l1/dma.h>

class SPIAdvClass: public SPIClass {
public:
    bool m_rxDMACompleted;
    bool m_txDMACompleted;

    explicit SPIAdvClass(unsigned long spiDevice);

    void begin();
    void end();

    void dma_int_enable(void);

    //int transfer(unsigned short dat);
    int transfer(void *buf, size_t count);

    int transfer(unsigned short dat);

    // DMA transfer methods with blocking
    int transferDMA(void *txBuf, void *rxBuf, size_t count);
    
    // DMA transfer methods with interrupt support
    int transferDMAINT(void *txBuf, void *rxBuf, size_t count);

    void rxDMACompleteCallback(){
      m_rxDMACompleted = true;
    }
    void txDMACompleteCallback(){
      m_txDMACompleted = true;
    }
    void resetDMACompleteFlags(){
      m_rxDMACompleted = false;
      m_txDMACompleted = false;
    }
    bool isDMAComplete(){   
      return m_rxDMACompleted;
    }
};

extern SPIAdvClass SPIAdv_intern;
extern SPIAdvClass SPIAdv;

#endif // __SPIADV_H__