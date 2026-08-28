#ifndef C_EXT_INTERRUPTS_H
#define C_EXT_INTERRUPTS_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stm32/l1/exti.h>

#define NULL 0
typedef void (*voidFuncPtr)(void);
typedef void (*voidArgumentFuncPtr)(void *);

typedef struct exti_channel {
    voidFuncPtr handler;
    //void (*handler)(void *);
    void *arg;
} exti_channel;

int get_exti_startupcycles(int portbits);
void exti_attach_interrupt(int portbits, voidFuncPtr handler, exti_trigger_t mode);
void exti_detach_interrupt(int portbits);
#ifdef __cplusplus
} // extern "C"
#endif

#endif
