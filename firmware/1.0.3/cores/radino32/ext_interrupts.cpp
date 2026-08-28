#include "ext_interrupts.h"
#include "c_ext_interrupts.h"
#include <boards.h>
#include <stm32/l1/exti.h>

static inline exti_trigger_t exti_out_mode(ExtIntTriggerMode mode) {
    switch (mode) {
    case RISING:
        return EXTI_RISING;
    case FALLING:
        return EXTI_FALLING;
    case CHANGE:
        return EXTI_BOTH;
    }
    return EXTI_TRIGGER_NONE;
}

int getInterruptDelay(uint8_t pin)
{
  return get_exti_startupcycles(PIN_MAP[pin].gpio_pin);
}

void deactivateInterrupt(uint8_t pin)
{
  exti_disable_interrupt(GPIO_BITS(PIN_MAP[pin].gpio_pin));
}

void reactivateInterrupt(uint8_t pin)
{
  exti_enable_interrupt(GPIO_BITS(PIN_MAP[pin].gpio_pin));
}

void attachInterrupt(uint8 pin, voidFuncPtr handler, ExtIntTriggerMode mode) {

    if (pin >= BOARD_NR_GPIO_PINS || !handler) {
        return;
    }

    exti_trigger_t outMode = exti_out_mode(mode);
    exti_attach_interrupt(PIN_MAP[pin].gpio_pin, handler, outMode);
}

void detachInterrupt(uint8 pin) {
    if (pin >= BOARD_NR_GPIO_PINS) {
        return;
    }

    exti_detach_interrupt(PIN_MAP[pin].gpio_pin);
}
