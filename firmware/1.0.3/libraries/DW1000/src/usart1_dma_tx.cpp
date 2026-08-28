#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <Arduino.h>  // 提供 SystemCoreClock
#include "usart1_dma_tx.h"

#define SystemCoreClock 32000000UL


// ===== 手动定义寄存器结构和地址 =====
#define USART1_BASE        0x40013800UL
#define DMA1_BASE          0x40020000UL
#define DMA1_Channel4_BASE (DMA1_BASE + 0x48)

#define USART1             ((USART_TypeDef *) USART1_BASE)
#define DMA1               ((DMA_TypeDef *) DMA1_BASE)
#define DMA1_Channel4      ((DMA_Channel_TypeDef *) DMA1_Channel4_BASE)

typedef struct {
  volatile uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR;
} USART_TypeDef;

typedef struct {
  volatile uint32_t ISR, IFCR;
} DMA_TypeDef;

typedef struct {
  volatile uint32_t CCR, CNDTR, CPAR, CMAR;
} DMA_Channel_TypeDef;

#define RCC_APB2ENR        (*(volatile uint32_t*)0x40021018)
#define RCC_AHBENR         (*(volatile uint32_t*)0x40021014)
#define RCC_APB2ENR_USART1EN  (1 << 14)
#define RCC_AHBENR_IOPAEN     (1 << 0)
#define RCC_AHBENR_DMA1EN     (1 << 0)

#define DMA_CCR_EN       (1 << 0)
#define DMA_CCR_DIR      (1 << 4)
#define DMA_CCR_MINC     (1 << 7)
#define DMA_CCR_TCIE     (1 << 1)
#define DMA_CCR_PL_1     (1 << 13)
#define DMA_ISR_TCIF4    (1 << 12)
#define DMA_IFCR_CTCIF4  (1 << 12)

#define DMA1_Channel4_IRQn 14
#define NVIC_ISER0 ((volatile uint32_t*)0xE000E100)

// ===== 状态变量（非 static）以供 DW1000.cpp 外部访问 =====
uint8_t* current_dma_buf = NULL;
uint16_t current_dma_len = 0;
volatile bool dma_busy = false;

void uart_dma_init(void) {
  RCC_APB2ENR |= RCC_APB2ENR_USART1EN;
  RCC_AHBENR  |= RCC_AHBENR_IOPAEN | RCC_AHBENR_DMA1EN;

  USART1->BRR = SystemCoreClock / 921600;
  USART1->CR3 |= (1 << 7);                 // DMAT
  USART1->CR1 |= (1 << 3) | (1 << 13);     // TE, UE

  DMA1_Channel4->CPAR = (uint32_t)&USART1->DR;
  DMA1_Channel4->CCR = DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_TCIE | DMA_CCR_PL_1;

  // 启用中断
  NVIC_ISER0[0] |= (1 << DMA1_Channel4_IRQn);
}

void uart_send_dma(uint8_t* data, uint16_t len) {
  if (dma_busy || len == 0) return;

  dma_busy = true;
  current_dma_buf = data;
  current_dma_len = len;

  DMA1_Channel4->CMAR = (uint32_t)data;
  DMA1_Channel4->CNDTR = len;
  DMA1_Channel4->CCR |= DMA_CCR_EN;
}

extern "C" void DMA1_Channel4_IRQHandler(void) {
    // digitalWrite(13, !digitalRead(13));  // 中断触发就闪一下 LED
  if (DMA1->ISR & DMA_ISR_TCIF4) {
    DMA1->IFCR |= DMA_IFCR_CTCIF4;
    DMA1_Channel4->CCR &= ~DMA_CCR_EN;
    dma_busy = false;
    if (current_dma_buf) {
      free(current_dma_buf);
      current_dma_buf = NULL;
    }
  }
}
