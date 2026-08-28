#ifndef __EEPROM_H__
#define __EEPROM_H__

#include <stm32/l1/flash.h>

#define EEADDR_EEPROM_START  0x08080000
#define EEINFO_EEPROM_SIZE   8192

#include <Arduino.h>

class c_EEPROM
{
	public:
		unsigned char read(unsigned long address)
		{
			if (address>=EEINFO_EEPROM_SIZE) return 0;
      address += EEADDR_EEPROM_START;
			return *((volatile unsigned char*)(address));
		}
		void write(unsigned long address, unsigned char data)
		{
      union {
        unsigned char byte[8];
        unsigned long lu[2];
      } temp;
      if (address>=EEINFO_EEPROM_SIZE) return;
      address += EEADDR_EEPROM_START;
      flash_unlock_pecr();
      temp.lu[0] = *((volatile unsigned long *)((address&0xFFFFFFF8)+0));
      temp.lu[1] = *((volatile unsigned long *)((address&0xFFFFFFF8)+4));
      temp.byte[address&0x7] = data;
      flash_erase_double_word(address&0xFFFFFFF8);
      while (FLASH_SR & FLASH_SR_BSY) {}
      flash_program_double_word(address&0xFFFFFFF8, (unsigned long *)&temp);
    	while (FLASH_SR & FLASH_SR_BSY) {}
      flash_lock_pecr();
    }
};

extern c_EEPROM EEPROM;

#endif