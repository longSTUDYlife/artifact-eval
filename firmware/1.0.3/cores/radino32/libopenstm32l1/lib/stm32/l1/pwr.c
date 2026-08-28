/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2013 Toshiaki Yoshida <yoshida@mpc.net>
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

#include <stm32/l1/pwr.h>

void pwr_set_vos(pwr_vos_t vos)
{
	u32 reg32;

	while (PWR_CSR & PWR_CSR_VOSF)
		;
	reg32 = PWR_CR;
	if ((reg32 & (PWR_CR_VOS1 | PWR_CR_VOS0)) == vos)
		return;
	reg32 &= ~(PWR_CR_VOS1 | PWR_CR_VOS0);
	reg32 |= vos;
	PWR_CR = reg32;
	while (PWR_CSR & PWR_CSR_VOSF)
		;
}

int pwr_get_vos(void)
{
	return PWR_CR & (PWR_CR_VOS1 | PWR_CR_VOS0);
}

/* Run mode */
void pwr_set_run_mode(void)
{
	u32 reg32;

	reg32 = PWR_CR;
	reg32 &= ~PWR_CR_LPRUN;
	PWR_CR = reg32;
	reg32 &= ~(PWR_CR_PDDS | PWR_CR_LPSDSR);
	PWR_CR = reg32;
}

/* Low power run mode */
void pwr_set_low_power_run_mode(void)
{
	u32 reg32;

	reg32 = PWR_CR;
	reg32 &= ~PWR_CR_PDDS;
	reg32 |= PWR_CR_LPSDSR;
	PWR_CR = reg32;
	reg32 |= PWR_CR_LPRUN;
	PWR_CR = reg32;
}

/* Sleep mode */
void pwr_set_sleep_mode(void)
{
	u32 reg32;

	reg32 = PWR_CR;
	reg32 &= ~PWR_CR_LPRUN;
	PWR_CR = reg32;
	reg32 &= ~(PWR_CR_PDDS | PWR_CR_LPSDSR);
	PWR_CR = reg32;
}

/* Low power sleep mode */
void pwr_set_low_power_sleep_mode(void)
{
	u32 reg32;

	reg32 = PWR_CR;
	reg32 &= ~(PWR_CR_LPRUN | PWR_CR_PDDS);
	PWR_CR = reg32;
	reg32 |= PWR_CR_LPSDSR;
	PWR_CR = reg32;
}

#define __I volatile const
#define __IO volatile
/** \brief  Structure type to access the System Control Block (SCB).
 */
typedef struct
{
  __I  uint32_t CPUID;                   /*!< Offset: 0x000 (R/ )  CPUID Base Register                                   */
  __IO uint32_t ICSR;                    /*!< Offset: 0x004 (R/W)  Interrupt Control and State Register                  */
  __IO uint32_t VTOR;                    /*!< Offset: 0x008 (R/W)  Vector Table Offset Register                          */
  __IO uint32_t AIRCR;                   /*!< Offset: 0x00C (R/W)  Application Interrupt and Reset Control Register      */
  __IO uint32_t SCR;                     /*!< Offset: 0x010 (R/W)  System Control Register                               */
  __IO uint32_t CCR;                     /*!< Offset: 0x014 (R/W)  Configuration Control Register                        */
  __IO uint8_t  SHP[12];                 /*!< Offset: 0x018 (R/W)  System Handlers Priority Registers (4-7, 8-11, 12-15) */
  __IO uint32_t SHCSR;                   /*!< Offset: 0x024 (R/W)  System Handler Control and State Register             */
  __IO uint32_t CFSR;                    /*!< Offset: 0x028 (R/W)  Configurable Fault Status Register                    */
  __IO uint32_t HFSR;                    /*!< Offset: 0x02C (R/W)  HardFault Status Register                             */
  __IO uint32_t DFSR;                    /*!< Offset: 0x030 (R/W)  Debug Fault Status Register                           */
  __IO uint32_t MMFAR;                   /*!< Offset: 0x034 (R/W)  MemManage Fault Address Register                      */
  __IO uint32_t BFAR;                    /*!< Offset: 0x038 (R/W)  BusFault Address Register                             */
  __IO uint32_t AFSR;                    /*!< Offset: 0x03C (R/W)  Auxiliary Fault Status Register                       */
  __I  uint32_t PFR[2];                  /*!< Offset: 0x040 (R/ )  Processor Feature Register                            */
  __I  uint32_t DFR;                     /*!< Offset: 0x048 (R/ )  Debug Feature Register                                */
  __I  uint32_t ADR;                     /*!< Offset: 0x04C (R/ )  Auxiliary Feature Register                            */
  __I  uint32_t MMFR[4];                 /*!< Offset: 0x050 (R/ )  Memory Model Feature Register                         */
  __I  uint32_t ISAR[5];                 /*!< Offset: 0x060 (R/ )  Instruction Set Attributes Register                   */
       uint32_t RESERVED0[5];
  __IO uint32_t CPACR;                   /*!< Offset: 0x088 (R/W)  Coprocessor Access Control Register                   */
} SCB_Type;
#define SCB                 ((SCB_Type       *)     SCB_BASE      )   /*!< SCB configuration struct           */
#define  SCB_SCR_SLEEPDEEP                   ((uint8_t)0x04)               /*!< Sleep deep bit */

/* Stop mode */
void pwr_set_stop_mode(void)
{
	u32 reg32;

	reg32 = PWR_CR;
	reg32 &= ~(PWR_CR_LPRUN | PWR_CR_PDDS);
	PWR_CR = reg32;
	reg32 |= (PWR_CR_CWUF | PWR_CR_LPSDSR);
	PWR_CR = reg32;
}

/* Stop mode */
void pwr_exec_stop_mode(void)
{
	u32 reg32;

	reg32 = PWR_CR;
	reg32 &= ~(PWR_CR_LPRUN | PWR_CR_PDDS);
	PWR_CR = reg32;
	reg32 |= (PWR_CR_CWUF | PWR_CR_LPSDSR);
	PWR_CR = reg32;
	
	SCB->SCR |= SCB_SCR_SLEEPDEEP;
	asm("wfe");
	SCB->SCR &= (uint32_t)~((uint32_t)SCB_SCR_SLEEPDEEP);
}

/* Standby mode */
void pwr_set_standby_mode(void)
{
	u32 reg32;

	reg32 = PWR_CR;
	reg32 &= ~PWR_CR_LPRUN;
	PWR_CR = reg32;
	reg32 &= ~PWR_CR_LPSDSR;
	reg32 |= (PWR_CR_CSBF | PWR_CR_CWUF | PWR_CR_PDDS);
	PWR_CR = reg32;
}

void pwr_enable_ultralow_power_mode(bool fast_wakeup)
{
	u32 reg32;

	reg32 = PWR_CR;
	if (fast_wakeup) {
		reg32 |= (PWR_CR_ULP | PWR_CR_FWU);
	} else {
		reg32 &= ~PWR_CR_FWU;
		reg32 |= PWR_CR_ULP;
	}
	PWR_CR = reg32;
}

void pwr_disable_ultralow_power_mode(void)
{
	PWR_CR &= ~PWR_CR_ULP;
}

void pwr_disable_backup_write_protection(void)
{
	PWR_CR |= PWR_CR_DBP;
}

void pwr_enable_backup_write_protection(void)
{
	PWR_CR &= ~PWR_CR_DBP;
}

void pwr_set_pvd_level(pwr_pvd_t level)
{
	u32 reg32;

	reg32 = PWR_CR;
	reg32 &= ~(PWR_CR_PLS2 | PWR_CR_PLS1 | PWR_CR_PLS0);
	reg32 |= level;
	PWR_CR = reg32;
}

void pwr_enable_pvd(void)
{
	PWR_CR |= PWR_CR_PVDE;
}

void pwr_disable_pvd(void)
{
	PWR_CR &= ~PWR_CR_PVDE;
}

void pwr_enable_wkup_pin(int ewup)
{
	PWR_CSR |= ewup;
}

void pwr_disable_wkup_pin(int ewup)
{
	PWR_CSR &= ~ewup;
}

int pwr_get_flag(int flag)
{
	return PWR_CSR & flag;
}

void pwr_clear_standby_flag(void)
{
	PWR_CR |= PWR_CR_CSBF;
}

void pwr_clear_wakeup_flag(void)
{
	PWR_CR |= PWR_CR_CWUF;
}

void pwr_wait_for_regulator_main_mode(void)
{
	while (PWR_CSR & PWR_CSR_REGLPF)
		;
}

void pwr_wait_for_vrefint_ready(void)
{
	while (!(PWR_CSR & PWR_CSR_VREFINTRDYF))
		;
}
