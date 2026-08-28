#include "SPIAdv.h"




SPIAdvClass SPIAdv_intern(SPI2);
SPIAdvClass SPIAdv(SPI1);

SPIAdvClass::SPIAdvClass(unsigned long spiDevice)
    : SPIClass(spiDevice)  
{
     m_prescaler = 2;
     //resetDMACompleteFlags();
     //this->dma_int_enable();
}


void SPIAdvClass::begin(){
    SPIClass::begin();
    /* Enable DMA1 clock */
    rcc_enable_clock(RCC_DMA1);
}

void SPIAdvClass::end(){
    SPIClass::end();
    /* Disable DMA1 clock */
    rcc_disable_clock(RCC_DMA1);
}

int SPIAdvClass::transfer(void *buf, size_t count)
{
   void* tx_buf = malloc(count);
    if (tx_buf == NULL) {
        return -1;  // allocation failed
    }
    memcpy(tx_buf, buf, count);

    SPIAdv_intern.resetDMACompleteFlags();
    bool result = transferDMA(tx_buf, buf, count);

    // volatile bool status = SPIAdv_intern.isDMAComplete();
	  // while (!status){
		//   status = SPIAdv_intern.isDMAComplete();
		//   iwdg_reset();  
	  // }

    free(tx_buf);        
    tx_buf = NULL;
    return result;
}

void SPIAdvClass::dma_int_enable(void) {
   if (m_spiDevice == SPI1)
   {
        /* SPI1 RX on DMA1 Channel 2 */
        nvic_set_priority(NVIC_DMA1_CHANNEL2_IRQ, 0);
        nvic_enable_irq(NVIC_DMA1_CHANNEL2_IRQ);
        /* SPI1 TX on DMA1 Channel 3 */
        nvic_set_priority(NVIC_DMA1_CHANNEL3_IRQ, 0);
        nvic_enable_irq(NVIC_DMA1_CHANNEL3_IRQ);
    }
    if (m_spiDevice == SPI2)
    { 
        /* SPI2 RX on DMA1 Channel 4 */
 	    nvic_set_priority(NVIC_DMA1_CHANNEL4_IRQ, 0);
	    nvic_enable_irq(NVIC_DMA1_CHANNEL4_IRQ);
	    /* SPI2 TX on DMA1 Channel 5 */
	    nvic_set_priority(NVIC_DMA1_CHANNEL5_IRQ, 0);
	    nvic_enable_irq(NVIC_DMA1_CHANNEL5_IRQ);
    }
}


int SPIAdvClass::transferDMA(void *txBuf, void *rxBuf, size_t count)
{
    // Check for valid length
    if (count<1) return -1;

    // DMA channel selection
    dma_channel_t dma_tx = (m_spiDevice==SPI1)?DMA1_CHANNEL3: DMA1_CHANNEL5;
    dma_channel_t dma_rx = (m_spiDevice==SPI1)?DMA1_CHANNEL2: DMA1_CHANNEL4;

    //Reset DMA channels
    //dma_channel_reset(dma_tx); //TX
    //dma_channel_reset(dma_rx); //RX

    //SerialOut.print("DMA channels reset...\r\n");
   
    // Reset SPI data and status registers.
    //uint32_t spi_sr = reset_spi_data_control_regs(m_spiDevice==SPI1?SPI1:SPI2);
    volatile uint8_t temp_data __attribute__ ((unused));
    if (m_spiDevice == SPI1) {
      while (SPI1_SR & (SPI_SR_RXNE | SPI_SR_OVR)) {
        temp_data = SPI1_DR;
      }
    } else {
      while (SPI2_SR & (SPI_SR_RXNE | SPI_SR_OVR)) {  
        temp_data = SPI2_DR;
      }
    }
    
    //SerialOut.print("Reset SPI done...\r\n");
    
    // Setup DMA channels
    uint32_t spi_dr_addr;
    if (m_spiDevice == SPI1) {
        spi_dr_addr = (uint32_t)&SPI1_DR; 
    } else {
        spi_dr_addr = (uint32_t)&SPI2_DR;
    }

    dma_setup_channel(dma_rx, (uint32_t)rxBuf, spi_dr_addr, count, 
                      DMA_CCR_PL_HIGH | DMA_CCR_MSIZE_8BIT | DMA_CCR_PSIZE_8BIT |
                      DMA_CCR_MINC | DMA_CCR_TCIE);
    dma_setup_channel(dma_tx, (uint32_t)txBuf, spi_dr_addr, count,
                      DMA_CCR_PL_HIGH | DMA_CCR_MSIZE_8BIT | DMA_CCR_PSIZE_8BIT |
                      DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_TCIE);

    //SerialOut.print("DMA channels setup done...\r\n");

    //dma_enable_interrupt(dma_rx, DMA_COMPLETE);
    //dma_enable_interrupt(dma_tx, DMA_COMPLETE);

    // Enable SPI RX and TX DMA requests
    if (m_spiDevice == SPI1) {
        SPI1_CR2 |= SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN;
    } else {
        SPI2_CR2 |= SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN;
    }
    
    //SerialOut.print("Enable SPI RX and TX DMA requests...\r\n");    

    dma_enable(dma_rx);
    dma_enable(dma_tx);
    
   // SerialOut.print("DMA channels enabled...\r\n");
  
    // Wait for completion
    while ((dma_get_interrupt_status(dma_rx, DMA_COMPLETE) == 0) ||
           (dma_get_interrupt_status(dma_tx, DMA_COMPLETE) == 0));

   // SerialOut.print("Transfer complete.\r\n");

    // Clear interrupt flags
    dma_clear_interrupt(dma_rx, DMA_COMPLETE);
    dma_clear_interrupt(dma_tx, DMA_COMPLETE);
  
   // SerialOut.print("Cleared DMA interrupt flags...\r\n");

    // Disable SPI RX and TX DMA requests
    if (m_spiDevice == SPI1) {
        SPI1_CR2 &= ~(SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN);
    } else {
        SPI2_CR2 &= ~(SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN);
    }

   // SerialOut.print("Disabled SPI RX and TX DMA requests...\r\n");

    dma_disable(dma_rx);
    dma_disable(dma_tx);

   // SerialOut.print("Disabled SPI RX and TX DMA requests...\r\n");

    return 0;

}

int SPIAdvClass::transfer(unsigned short dat)
{
  if (m_spiDevice == SPI1) return spi_transfer(SPI1, dat);
  if (m_spiDevice == SPI2) return spi_transfer(SPI2, dat);
  return -1;
}



int SPIAdvClass::transferDMAINT(void *txBuf, void *rxBuf, size_t count)
{
    // Check for valid length
    if (count<1) return -1;

    // DMA channel selection
    dma_channel_t dma_tx = (m_spiDevice==SPI1)?DMA1_CHANNEL3: DMA1_CHANNEL5;
    dma_channel_t dma_rx = (m_spiDevice==SPI1)?DMA1_CHANNEL2: DMA1_CHANNEL4;

    //Reset DMA channels
    //dma_channel_reset(dma_tx); //TX
    //dma_channel_reset(dma_rx); //RX

    //SerialOut.print("DMA channels reset...\r\n");
   
    // Reset SPI data and status registers.
    //uint32_t spi_sr = reset_spi_data_control_regs(m_spiDevice==SPI1?SPI1:SPI2);

    volatile uint8_t temp_data __attribute__ ((unused));
    if (m_spiDevice == SPI1) {
      while (SPI1_SR & (SPI_SR_RXNE | SPI_SR_OVR)) {
        temp_data = SPI1_DR;
      }
    } else {
      while (SPI2_SR & (SPI_SR_RXNE | SPI_SR_OVR)) {  
        temp_data = SPI2_DR;
      }
    }

    //SerialOut.print("Reset SPI done...\r\n");
    
    // Setup DMA channels
    uint32_t spi_dr_addr;
    if (m_spiDevice == SPI1) {
        spi_dr_addr = (uint32_t)&SPI1_DR; 
    } else {
        spi_dr_addr = (uint32_t)&SPI2_DR;
    }

    dma_setup_channel(dma_rx, (uint32_t)rxBuf, spi_dr_addr, count, 
                      DMA_CCR_PL_HIGH | DMA_CCR_MSIZE_8BIT | DMA_CCR_PSIZE_8BIT |
                      DMA_CCR_MINC | DMA_CCR_TCIE);
    dma_setup_channel(dma_tx, (uint32_t)txBuf, spi_dr_addr, count,
                      DMA_CCR_PL_HIGH | DMA_CCR_MSIZE_8BIT | DMA_CCR_PSIZE_8BIT |
                      DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_TCIE);

    //SerialOut.print("DMA channels setup done...\r\n");

    // Enable SPI RX and TX DMA requests
    if (m_spiDevice == SPI1) {
        SPI1_CR2 |= SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN;
    } else {
        SPI2_CR2 |= SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN;
    }
    
    //SerialOut.print("Enable SPI RX and TX DMA requests...\r\n");    

    dma_enable(dma_rx);
    dma_enable(dma_tx);
}

// extern "C" {


// /* SPI receive completed with DMA */
// void dma1_channel4_isr(void)
// {  
//   dma_channel_t dma_rx = DMA1_CHANNEL4;

//   //clear DMA interrupt flag
// 	if ((DMA1_ISR &DMA_ISR_TCIF4) != 0) {
// 		DMA1_IFCR |= DMA_IFCR_CTCIF4;
// 	}

//   //disable DMA transfer complete interrupt
// 	dma_disable_interrupt(dma_rx, DMA_COMPLETE);

//   //disable SPI RX DMA request
//   SPI2_CR2 &= ~SPI_CR2_RXDMAEN;
  
//   //disable DMA channel
// 	dma_disable(dma_rx);
  
//   SPIAdv_intern.rxDMACompleteCallback();
//   //digitalWrite(17, HIGH);

// }


// /* SPI transmit completed with DMA */
// void dma1_channel5_isr(void)
// {  
//   dma_channel_t dma_tx = DMA1_CHANNEL5;

// 	if ((DMA1_ISR &DMA_ISR_TCIF5) != 0) {
// 		DMA1_IFCR |= DMA_IFCR_CTCIF5;
// 	}
//  //disable DMA transfer complete interrupt
// 	dma_disable_interrupt(dma_tx, DMA_COMPLETE);

//   //disable SPI RX DMA request
//   SPI2_CR2 &= ~SPI_CR2_TXDMAEN;
  
//   //disable DMA channel
// 	dma_disable(dma_tx);
//   SPIAdv_intern.txDMACompleteCallback();
//  //digitalWrite(18, HIGH);
// }

// }