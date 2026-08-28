#include "usart_irq.h"
#include <stm32/l1/gpio.h>

void usart1_isr(void)
{
    usart_rx_interrupt(1);
}

void usart2_isr(void)
{
    usart_rx_interrupt(2);
}

void usart3_isr(void)
{
    usart_rx_interrupt(3);
}

