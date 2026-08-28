/******************************************************************************
 * The MIT License
 *
 * Copyright (c) 2010 Perry Hung.
 * Copyright (c) 2011, 2012 LeafLabs, LLC.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *****************************************************************************/

/**
 * @file wirish/boards.cpp
 * @brief init() and board routines.
 *
 * This file is mostly interesting for the init() function, which
 * configures Flash, the core clocks, and a variety of other available
 * peripherals on the board so the rest of Wirish doesn't have to turn
 * things on before using them.
 *
 * Prior to returning, init() calls boardInit(), which allows boards
 * to perform any initialization they need to. This file includes a
 * weak no-op definition of boardInit(), so boards that don't need any
 * special initialization don't have to define their own.
 *
 * How init() works is chip-specific. See the boards_setup.cpp files
 * under e.g. wirish/stm32f1/, wirish/stmf32f2 for the details, but be
 * advised: their contents are unstable, and can/will change without
 * notice.
 */

#define __weak
#include <stm32/l1/rcc.h>
#include <stm32/l1/pwr.h>
#include <stm32/l1/flash.h>
#include <stm32/l1/gpio.h>
#include <stm32/l1/usart.h>
#include <stm32/l1/adc.h>
#include <stm32/l1/nvic.h>
#include <stm32/l1/systick.h>
#include <boards.h>
#include "boards_private.h"
#include "io.h"

#include "HardwareSerial.h"
#include "usb_serial.h"

static void setup_flash(void);
static void setup_clocks(void);
static void setup_nvic(void);
static void setup_adcs(void);
static void setup_timers(void);

/*
 * Exported functions
 */

static void usart_setup(void)
{
    /* Enable USART1 clock. */
    rcc_enable_clock(RCC_USART1);
    rcc_enable_clock(RCC_USART2);
    rcc_enable_clock(RCC_USART3);

    /* Enable the USART interrupt. */
    nvic_enable_irq(NVIC_USART1_IRQ);
    nvic_enable_irq(NVIC_USART2_IRQ);
    nvic_enable_irq(NVIC_USART3_IRQ);

    /* RXLED und TXLED */
    // pinMode(17, OUTPUT);
    // pinMode(18, OUTPUT);
    // digitalWrite(17, LOW);
    // digitalWrite(18, LOW);
}

void init(void) {
    int i,c=0;

    setup_clocks();

    /* Enable GPIOA clock. */
	rcc_enable_clock(RCC_GPIOA);
    rcc_enable_clock(RCC_GPIOB);
    rcc_enable_clock(RCC_GPIOC);

    usart_setup();

    rcc_enable_clock(RCC_TIM2);
    rcc_enable_clock(RCC_TIM3);
    rcc_enable_clock(RCC_TIM4);
    rcc_enable_clock(RCC_TIM5);
    rcc_enable_clock(RCC_TIM10);
    rcc_enable_clock(RCC_TIM11);

    /* Enable DAC clock. */
	rcc_enable_clock(RCC_DAC);

    /* Enable SYSCFG clcok. (for ext interrupts)*/
	rcc_enable_clock(RCC_SYSCFG);


    setup_timers();
    setup_adcs();

    systick_set_clocksource(SYSTICK_AHB);
    systick_set_reload(STM32_PCLK1/1000 - 1);
    systick_enable_interrupt();
    systick_enable_counter();
}


/* You could farm this out to the files in boards/ if e.g. it takes
 * too long to test on boards with lots of pins. */
bool boardUsesPin(uint8_t pin) {
    /*    for (int i = 0; i < BOARD_NR_USED_PINS; i++) {
        if (pin == boardUsedPins[i]) {
            return true;
        }
    }
    return false;*/
}

/*
 * Auxiliary routines
 */

static void setup_clocks(void) {
	/* Enable PWR clock. */
	rcc_enable_clock(RCC_PWR);

	/* Set VCORE to 1.8V */
	pwr_set_vos(PWR_1_8_V);

	/*
	 * Set Flash memory latency.
	 *
	 *          VCORE=1.8V  VCORE=1.5V  VCORE=1.2V
	 * 0WS from 0-16MHz     0-8MHz      0-2MHz
	 * 1WS from 16-32MHz    8-16MHz     2-4MHz
	 */
	flash_enable_64bit_access(1);

	/* Enable internal high-speed oscillator (16MHz). */
	rcc_enable_osc(RCC_HSE);

	/*
	 * APB2 16MHz (Max. 32MHz)
	 * APB1 16MHz (Max. 32MHz)
	 * AHB  16MHz (Max. 32MHz)
	 */
	/* AHB, APB1 and APB2 prescaler value is default. */
	//rcc_set_prescaler(1, 1, 1);

    //96MHz PLL to make USB work
    rcc_setup_pll(RCC_HSE, 4, 3);
    rcc_enable_osc(RCC_PLL);
	/* Select SYSCLK source. */
	//rcc_set_sysclk_source(RCC_HSE);
	rcc_set_sysclk_source(RCC_PLL);

	rcc_enable_osc(RCC_HSI);

}

/*
 * These addresses are where usercode starts when a bootloader is
 * present. If no bootloader is present, the user NVIC usually starts
 * at the Flash base address, 0x08000000.
 */
#if defined(BOOTLOADER_maple)
	#define USER_ADDR_ROM 0x08005000
#else
	#if defined(BOOTLOADER_robotis)
		#define USER_ADDR_ROM 0x08003000
	#else
		#define USER_ADDR_ROM 0x08000000
	#endif
#endif
#define USER_ADDR_RAM 0x20000C00
extern char __text_start__;

static void setup_nvic(void) {
    /*#ifdef VECT_TAB_FLASH
    nvic_init(USER_ADDR_ROM, 0);
#elif defined VECT_TAB_RAM
    nvic_init(USER_ADDR_RAM, 0);
#elif defined VECT_TAB_BASE
    nvic_init((uint32)0x08000000, 0);
#elif defined VECT_TAB_ADDR
    // A numerically supplied value
    nvic_init((uint32)VECT_TAB_ADDR, 0);
#else
    // Use the __text_start__ value from the linker script; this
    // should be the start of the vector table.
    nvic_init((uint32)&__text_start__, 0);
#endif
    */
}

static void setup_adcs(void) {
    /* Enable ADC clock. */
	rcc_enable_clock(RCC_ADC);

	/* Enable ADC interrupt. */
	nvic_enable_irq(NVIC_ADC_IRQ);

	/* Set ADC prescaler to 1(default). */
	// adc_set_prescaler(1);

	/* Enable overrun interrupt. */
	adc_enable_interrupt(ADC_OVERRUN);
}

static void setup_delay_timers(void) {
	rcc_enable_clock(RCC_TIM6);

	/* Enable one-pulse mode. */
	tim_enable_one_pulse_mode(TIM6);

	/* Generate update interrupt on counter overflow. */
	tim_disable_update_interrupt_on_any(TIM6);
}

static tim_t timers[] = {TIM2, TIM3, TIM4, TIM5, TIM10, TIM11};

static void setup_timers(void) {
    int i;

    /* Timer fuer PWM */
    for (i = 0; i < sizeof(timers) / sizeof(timers[0]); i++)
        setup_pwm_freq(timers[i], TIMX_CLK_APB1/MAX_CNT/PWM_FREQ, MAX_CNT - 1);

    for (i = 0; i < BOARD_NR_GPIO_PINS; i++)
        if (PIN_MAP[i].timer_channel != TIMx_CCx)
            tim_set_capture_compare_mode(PIN_MAP[i].timer_channel, TIM_OC_PRELOAD | TIM_OC_PWM1 | TIM_CC_ENABLE);


    for (i = 0; i < sizeof(timers) / sizeof(timers[0]); i++) {
        tim_start_capture_compare(timers[i]);
        tim_enable_counter(timers[i]);
    }

    setup_delay_timers();
}

void setup_pwm_freq(tim_t tim, uint32_t prescaler, uint32_t maxcnt) {
    tim_setup_counter(tim, prescaler, maxcnt - 1);

}
