#include "wirish_time.h"

/* 1 - 32767 usec */
static inline void delay_us(uint16 us)
{

	/* Load prescaler value (2MHz). */
	tim_load_prescaler_value(TIM6, TIMX_CLK_APB1 / 2000000 - 1);

	/* Set auto-reload value (us * 2). */
	tim_set_autoreload_value(TIM6, (us << 1) - 1);

	/* Enable counter. */
	tim_enable_counter(TIM6);

	/* Wait for update interrupt flag. */
	while (!tim_get_interrupt_status(TIM6, TIM_UPDATE))
		;

	/* Clear update interrupt flag. */
	tim_clear_interrupt(TIM6, TIM_UPDATE);
}

/* 1 - 32767 msec */
static inline void delay_ms(u16 ms)
{
	/* Load prescaler value (2kHz). */
        tim_load_prescaler_value(TIM6, TIMX_CLK_APB1 / 2000 - 1);

	/* Set auto-reload value (ms * 2). */
	tim_set_autoreload_value(TIM6, (ms << 1) - 1);

	/* Enable counter. */
	tim_enable_counter(TIM6);

	/* Wait for update interrupt flag. */
	while (!tim_get_interrupt_status(TIM6, TIM_UPDATE))
		;

	/* Clear update interrupt flag. */
	tim_clear_interrupt(TIM6, TIM_UPDATE);
}

void delay(u16 ms) {
    delay_ms(ms);
}

void delayMicroseconds(u16 us) {
    delay_us(us);
}

