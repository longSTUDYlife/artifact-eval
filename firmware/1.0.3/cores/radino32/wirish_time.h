/******************************************************************************
 * The MIT License
 *
 * Copyright (c) 2010 Perry Hung.
.* Modified 2016 for radino32 compatibility by In-Circuit GmbH
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
 * @file wirish/include/wirish/wirish_time.h
 * @brief Timing and delay functions.
 */

#ifndef _WIRISH_WIRISH_TIME_H_
#define _WIRISH_WIRISH_TIME_H_


#include <boards.h>
#include <stm32/l1/systick.h>

extern volatile uint32_t systick_uptime_millis;

/**
 * Returns time (in milliseconds) since the beginning of program
 * execution. On overflow, restarts at 0.
 * @see micros()
 */
static inline uint32 millis(void) {
    return systick_uptime_millis;
    //return systick_get_value();
}

static inline uint32 uptimeMillis(void) {
    return systick_uptime_millis;
}

/**
 * Returns time (in microseconds) since the beginning of program
 * execution.  On overflow, restarts at 0.
 * @see millis()
 */
/*
static inline uint32 micros(void) {
//TODO
}
*/
__attribute__((always_inline)) static inline uint32_t micros()
{
  uint32_t reload = SysTick->LOAD;
  uint32_t value = 0;
  uint32_t ret = 0;
  do {
    ret = millis();
    value = SysTick->VAL;
  } while (ret != millis());
  value = reload - value;
  value = (value*1000)/reload;
  ret = 1000*ret + value;
  return ret;
}

/**
 * Delay for at least the given number of milliseconds.
 *
 * Interrupts, etc. may cause the actual number of milliseconds to
 * exceed ms.  However, this function will return no less than ms
 * milliseconds from the time it is called.
 *
 * @param ms the number of milliseconds to delay.
 * @see delayMicroseconds()
 */
void delay(uint16 ms);

/**
 * Delay for at least the given number of microseconds.
 *
 * Interrupts, etc. may cause the actual number of microseconds to
 * exceed us.  However, this function will return no less than us
 * microseconds from the time it is called.
 *
 * @param us the number of microseconds to delay.
 * @see delay()
 */
void delayMicroseconds(uint16 us);

#endif
