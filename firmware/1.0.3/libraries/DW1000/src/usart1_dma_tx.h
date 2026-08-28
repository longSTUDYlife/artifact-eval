#ifndef USART1_DMA_TX_H
#define USART1_DMA_TX_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_dma_init(void);
void uart_send_dma(uint8_t* data, uint16_t len);
extern volatile bool dma_busy;

#ifdef __cplusplus
}
#endif

#endif  // USART1_DMA_TX_H
