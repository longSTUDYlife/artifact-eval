/******************************************************************************
 * The MIT License
 *
 * Copyright (c) 2011 LeafLabs, LLC.
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
 * @file   wirish/boards/nucleo/board.cpp
 * @author Grégoire Passault <g.passault@gmail.com>
 * @brief  Nucleo board file
 *
 * This mapping was done using the NUCLEO documentation and may be incomplete
 * or contains error
 *
 * If you want to use the PWM outputs, consider understanding all the remapping
 * process that can be involved. You may have to tweak this file regarding your goals.
 */

#include <wirish_types.h> // For stm32_pin_info and its contents
                                 // (these go into PIN_MAP).

#include "boards_private.h"      // For PMAP_ROW(), which makes
                                 // PIN_MAP easier to read.
#include <board/board.h>
#include <stm32/l1/tim.h>
#include <stm32/l1/adc.h>

// boardInit(): NUCLEO rely on some remapping
void boardInit(void) {
    //afio_remap(AFIO_REMAP_TIM2_FULL);
    //afio_remap(AFIO_REMAP_TIM3_PARTIAL);
}

// Pin map: this lets the basic I/O functions (digitalWrite(),
// analogRead(), pwmWrite()) translate from pin numbers to STM32
// peripherals.
//
//
// - GPIO device & PIN for the pin (GPIOA1, etc.)
// - Timer device, or TIMx if none
// - Timer channel (TIM1CC1,..), or TIMxCCx if none
// - ADC channel, or ADCx if none
// - GPIO alternative Function name for PWM
extern const stm32_pin_info PIN_MAP[BOARD_NR_GPIO_PINS] = {
    {GPIO_PA10, TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //0
    {GPIO_PA9 , TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //1
    {GPIO_PB7 , TIM4,  TIM4_CC2,  ADCx,        GPIO_TIM3_4},  //2
    {GPIO_PB6 , TIM4,  TIM4_CC1,  ADCx,        GPIO_TIM3_4},  //3
    {0        , TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //4
    {GPIO_PB9 , TIM4,  TIM4_CC4,  ADCx,        GPIO_TIM3_4},  //5
    {GPIO_PA3 , TIM2,  TIM2_CC4,  ADC_IN_PA3,  GPIO_TIM3_5},  //6
    {0        , TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //7
    {0        , TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //8
    {0        , TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //9
    {GPIO_PA2 , TIM2,  TIM2_CC3,  ADC_IN_PA2,  GPIO_TIM2},    //10
    {GPIO_PB0 , TIM3,  TIM3_CC3,  ADC_IN_PB0,  GPIO_TIM3_4},  //11
    {GPIO_PA6 , TIM10, TIM10_CC1, ADC_IN_PA6,  GPIO_TIM9_11}, //12
    {GPIO_PB1 , TIM3,  TIM3_CC4,  ADC_IN_PB1,  GPIO_TIM3_4},  //13
    {GPIO_PB4 , TIM3,  TIM3_CC1,  ADCx,        GPIO_TIM3_4},  //14
    {GPIO_PB3 , TIM2,  TIM2_CC2,  ADCx,        GPIO_TIM2},    //15
    {GPIO_PB5 , TIM3,  TIM3_CC2,  ADCx,        GPIO_TIM3_4},  //16
    {GPIO_PA15, TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //17(RXLED)
    {GPIO_PB8 , TIM4,  TIM4_CC3,  ADCx,        GPIO_TIM3_4},  //18(TXLED)
    {GPIO_PA0 , TIM2,  TIM2_CC1,  ADC_IN_PA0,  GPIO_TIM3_5},  //A0/19
    {GPIO_PA1 , TIMx,  TIMx_CCx,  ADC_IN_PA1,  GPIO_TIM3_5},  //A1/20
    {GPIO_PA4 , TIMx,  TIMx_CCx,  ADC_IN_PA4,  GPIO_NONE},    //A2/21
    {GPIO_PA7 , TIM11, TIM11_CC1, ADC_IN_PA7,  GPIO_TIM9_11}, //A3/22
    {0        , TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //A4/23
    {GPIO_PB12, TIMx,  TIMx_CCx,  ADC_IN_PB12, GPIO_NONE},    //A5/24
    {GPIO_PC13, TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //25/TAMPER
    {GPIO_PA11, TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //USBM/26
    {GPIO_PA12, TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //USBP/27

    {GPIO_PA5 , TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //28
    {GPIO_PA8 , TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //29
    {GPIO_PA13, TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //30
    {GPIO_PA14, TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //31
    {GPIO_PB10, TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //32
    {GPIO_PB11, TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //33
    {GPIO_PB13, TIMx,  TIMx_CCx,  ADCx,        GPIO_NONE},    //34
    {GPIO_PB14, TIMx,  TIMx_CCx,  ADC_IN_PB14, GPIO_NONE},    //35
    {GPIO_PB15, TIMx,  TIMx_CCx,  ADC_IN_PB15, GPIO_NONE},    //36

};

// // Array of pins you can use for pwmWrite(). Keep it in Flash because
// // it doesn't change, and so we don't waste RAM.
// extern const uint8 boardPWMPins[] __FLASH__ = {
//     3, 5, 6, 10, 11, 16, 17, 19
// };

// // Array of pins you can use for analogRead().
// extern const uint8 boardADCPins[] __FLASH__ = {
//     16, 17, 18, 19, 20, 21
// };

// // Array of pins that the board uses for something special. Other than
// // the button and the LED, it's usually best to leave these pins alone
// // unless you know what you're doing.
// extern const uint8 boardUsedPins[] __FLASH__ = {
//     BOARD_LED_PIN, BOARD_BUTTON_PIN
// };
