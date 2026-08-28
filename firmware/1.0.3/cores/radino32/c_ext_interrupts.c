#include <stm32/l1/nvic.h>
#include <stm32/l1/gpio.h>
#include <stm32/l1/syscfg.h>
#include "c_ext_interrupts.h"

static inline void dispatch_single_exti(uint32_t exti_num);
static inline void dispatch_extis(uint32_t start, uint32_t stop);

static exti_channel exti_channels[] = {
    { .handler = NULL, .arg = NULL },  // EXTI0
    { .handler = NULL, .arg = NULL },  // EXTI1
    { .handler = NULL, .arg = NULL },  // EXTI2
    { .handler = NULL, .arg = NULL },  // EXTI3
    { .handler = NULL, .arg = NULL },  // EXTI4
    { .handler = NULL, .arg = NULL },  // EXTI5
    { .handler = NULL, .arg = NULL },  // EXTI6
    { .handler = NULL, .arg = NULL },  // EXTI7
    { .handler = NULL, .arg = NULL },  // EXTI8
    { .handler = NULL, .arg = NULL },  // EXTI9
    { .handler = NULL, .arg = NULL },  // EXTI10
    { .handler = NULL, .arg = NULL },  // EXTI11
    { .handler = NULL, .arg = NULL },  // EXTI12
    { .handler = NULL, .arg = NULL },  // EXTI13
    { .handler = NULL, .arg = NULL },  // EXTI14
    { .handler = NULL, .arg = NULL },  // EXTI15
};

int get_exti_startupcycles(int portbits)
{
  switch (GPIO_BITS(portbits)) {
        default:
            return -1;
        case EXTI0: //2.92us -- 70 cycles 62+8
        case EXTI1:
        case EXTI2:
        case EXTI3:
        case EXTI4:
            return 8;
        case EXTI5: //3.40us -- 82 cycles 62+20
        case EXTI10:
            return 20;
        case EXTI6: //4.24us -- 102 cycles 62+40
        case EXTI11:
            return 40;
        case EXTI7: //5.08us -- 122 cycles 62+60
        case EXTI12:
            return 60;
        case EXTI8:
        case EXTI13:
            return 80;
        case EXTI9:
        case EXTI14:
            return 100;
        case EXTI15:
            return 120;
    }
}

void exti_attach_interrupt(int portbits,
                           voidFuncPtr handler,
                           exti_trigger_t mode) {
    /* __builtin_ctz returns number of trailing 0 bits, so turns 2^n in n */
    exti_channels[__builtin_ctz(portbits)].handler = handler;

    /* Enable EXTIn interrupt. */
    switch (GPIO_BITS(portbits)) {
        case EXTI0:
            nvic_enable_irq(NVIC_EXTI0_IRQ);
            break;
        case EXTI1:
            nvic_enable_irq(NVIC_EXTI1_IRQ);
            break;
        case EXTI2:
            nvic_enable_irq(NVIC_EXTI2_IRQ);
            break;
        case EXTI3:
            nvic_enable_irq(NVIC_EXTI3_IRQ);
            break;
        case EXTI4:
            nvic_enable_irq(NVIC_EXTI4_IRQ);
            break;
        case EXTI5:
        case EXTI6:
        case EXTI7:
        case EXTI8:
        case EXTI9:
            nvic_enable_irq(NVIC_EXTI9_5_IRQ);
            break;
        case EXTI10:
        case EXTI11:
        case EXTI12:
        case EXTI13:
        case EXTI14:
        case EXTI15:
            nvic_enable_irq(NVIC_EXTI15_10_IRQ);
            break;
    }

    /* Set GPIO to 'input float'. */
    gpio_config_input(GPIO_FLOAT, portbits);

    /* Connect PXn to EXTIn */
    switch (GPIO_PORT(portbits)) {
    case GPIOA:
        syscfg_select_exti_source(GPIO_BITS(portbits), SYSCFG_PA);
        break;
    case GPIOB:
        syscfg_select_exti_source(GPIO_BITS(portbits), SYSCFG_PB);
        break;
    case GPIOC:
        syscfg_select_exti_source(GPIO_BITS(portbits), SYSCFG_PC);
        break;
    default:
        return;
    };

    /* trigger mode for EXTIn */
    exti_set_trigger(GPIO_BITS(portbits), mode);

    /* Enable interrupt. */
    exti_enable_interrupt(GPIO_BITS(portbits));
}

void exti_detach_interrupt(int portbits) {
    /* Enable interrupt. */
    exti_disable_interrupt(GPIO_BITS(portbits));

    /* Finally, unregister the user's handler */
    exti_channels[__builtin_ctz(portbits)].handler = NULL;
}

/*
 * Interrupt handlers
 */

void exti0_isr(void) {
    dispatch_single_exti(0);
}

void exti1_isr(void) {
    dispatch_single_exti(1);
}

void exti2_isr(void) {
    dispatch_single_exti(2);
}

void exti3_isr(void) {
    dispatch_single_exti(3);
}

void exti4_isr(void) {
    dispatch_single_exti(4);
}

void exti9_5_isr(void) {
    dispatch_extis(5, 9);
}

void exti15_10_isr(void) {
    dispatch_extis(10, 15);
}

/* This dispatch routine is for non-multiplexed EXTI lines only*/
static __attribute__((always_inline)) void dispatch_single_exti(uint32_t exti) {
    //voidArgumentFuncPtr handler = exti_channels[exti].handler;
    voidFuncPtr handler = exti_channels[exti].handler;
    uint32_t eb = (1U << exti);

    if (!handler) {
        return;
    }

    if (exti_get_interrupt_mask(eb) &&
        exti_get_interrupt_status(eb)) {

        handler();
    }
    exti_clear_interrupt(1U << exti);
}

/* Dispatch routine for EXTIs which share an IRQ. */
static __attribute__((always_inline)) void dispatch_extis(uint32_t start, uint32_t stop) {
    uint32_t handled_msk = 0;
    uint32_t exti;

    /* Dispatch user handlers for pending EXTIs. */
    for (exti = start; exti <= stop; exti++) {
        uint32_t eb = (1U << exti);
        if (exti_get_interrupt_mask(eb) &&
            exti_get_interrupt_status(eb)) {
            voidArgumentFuncPtr handler = exti_channels[exti].handler;
            if (handler) {
                handler(exti_channels[exti].arg);
                handled_msk |= eb;
            }
        }
    }

    /* Clear the pending bits for handled EXTIs. */
    exti_clear_interrupt(handled_msk);
}
