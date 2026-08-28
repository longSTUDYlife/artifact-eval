/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2010 Thomas Otto <tommi@viadmin.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBOPENSTM32L1_SYSTICK_H
#define LIBOPENSTM32L1_SYSTICK_H

#ifdef __cplusplus
extern "C"{
#endif



#include <stm32/l1/memorymap.h>
#include <libopencm3.h>


#define __IO volatile
#define __I volatile const
typedef struct
{
  __IO uint32_t CTRL;                    /*!< Offset: 0x000 (R/W)  SysTick Control and Status Register */
  __IO uint32_t LOAD;                    /*!< Offset: 0x004 (R/W)  SysTick Reload Value Register       */
  __IO uint32_t VAL;                     /*!< Offset: 0x008 (R/W)  SysTick Current Value Register      */
  __I  uint32_t CALIB;                   /*!< Offset: 0x00C (R/ )  SysTick Calibration Register        */
} SysTick_Type;

/* SysTick Control / Status Register Definitions */
#define SysTick_CTRL_COUNTFLAG_Pos         16                                             /*!< SysTick CTRL: COUNTFLAG Position */
#define SysTick_CTRL_COUNTFLAG_Msk         (1UL << SysTick_CTRL_COUNTFLAG_Pos)            /*!< SysTick CTRL: COUNTFLAG Mask */
#define SysTick_CTRL_CLKSOURCE_Pos          2                                             /*!< SysTick CTRL: CLKSOURCE Position */
#define SysTick_CTRL_CLKSOURCE_Msk         (1UL << SysTick_CTRL_CLKSOURCE_Pos)            /*!< SysTick CTRL: CLKSOURCE Mask */
#define SysTick_CTRL_TICKINT_Pos            1                                             /*!< SysTick CTRL: TICKINT Position */
#define SysTick_CTRL_TICKINT_Msk           (1UL << SysTick_CTRL_TICKINT_Pos)              /*!< SysTick CTRL: TICKINT Mask */
#define SysTick_CTRL_ENABLE_Pos             0                                             /*!< SysTick CTRL: ENABLE Position */
#define SysTick_CTRL_ENABLE_Msk            (1UL << SysTick_CTRL_ENABLE_Pos)               /*!< SysTick CTRL: ENABLE Mask */

#define SCS_BASE            (0xE000E000UL)                            /*!< System Control Space Base Address  */
#define SysTick_BASE        (SCS_BASE +  0x0010UL)                    /*!< SysTick Base Address               */
#define SysTick             ((SysTick_Type   *)     SysTick_BASE  )   /*!< SysTick configuration struct       */


/*
 * STMicroelectronics(www.st.com)
 *
 * PM0056: STM32F10xxx/20xxx/21xxx/L1xxxx Cortex-M3 programming manual
 * (27-May-2013 Rev 5)
 *
 * 4.5 SysTick timer (STK)
 */

/* --- SYSTICK registers --------------------------------------------------- */
/*
 * Offset	Register
 * 0x00		STK_CTRL	SysTick control and status register
 * 0x04		STK_LOAD	SysTick reload value register
 * 0x08		STK_VAL		SysTick current value register
 * 0x0c		STK_CALIB	SysTick calibration value register
 */

#define STK_CTRL			MMIO32(STK_BASE + 0x00)
#define STK_LOAD			MMIO32(STK_BASE + 0x04)
#define STK_VAL				MMIO32(STK_BASE + 0x08)
#define STK_CALIB			MMIO32(STK_BASE + 0x0C)

/* --- STK_CTRL values ----------------------------------------------------- */

#define STK_CTRL_COUNTFLAG		(1 << 16)
#define STK_CTRL_CLKSOURCE		(1 << 2)
#define STK_CTRL_TICKINT		(1 << 1)
#define STK_CTRL_ENABLE			(1 << 0)

/* --- STK_LOAD values ----------------------------------------------------- */

/* STK_LOAD[23:0]: RELOAD[23:0]: RELOAD value */

/* --- STK_VAL values ------------------------------------------------------ */

/* STK_VAL[23:0]: CURRENT[23:0]: Current counter value */

/* --- STK_CALIB values ---------------------------------------------------- */

#define STK_CALIB_NOREF			(1 << 31)
#define STK_CALIB_SKEW			(1 << 30)
/* STK_CALIB[23:0]: TENMS[23:0]: Calibration value */

/* --- Function Prototypes ------------------------------------------------- */

/* Control */
enum {
	SYSTICK_AHB_8 = 0,
	SYSTICK_AHB = 4,	/* clock source */
	SYSTICK_INT = 2,
	SYSTICK_ENABLE = 1
};

void systick_set_reload(int value);
int systick_get_value(void);
void systick_set_clocksource(int clocksource);
void systick_enable_interrupt(void);
void systick_disable_interrupt(void);
void systick_enable_counter(void);
void systick_disable_counter(void);
bool systick_countflag(void);
int systick_get_calib(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBOPENSTM32L1_SYSTICK_H */
