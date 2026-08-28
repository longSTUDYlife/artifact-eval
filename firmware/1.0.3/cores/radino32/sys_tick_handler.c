#include <inttypes.h>

volatile uint32_t systick_uptime_millis;

#define MMIO32(addr) (*(volatile uint32_t *)(addr))
#define GPIO_BSRR(base) MMIO32((base) + 0x18)
#define PERIPH_BASE 0x40000000
#define PERIPH_BASE_AHB (PERIPH_BASE + 0x20000)
#define GPIO_PORT_A_BASE (PERIPH_BASE_AHB + 0x0000)
#define digitalWriteEx(port,bit,value) (GPIO_BSRR(GPIO_PORT_##port##_BASE) = (uint32_t)(1<<bit)<<(value?0:16))

void sys_tick_handler()
{

//digitalWriteEx(A,3,1);

    systick_uptime_millis++;

//digitalWriteEx(A,3,0);

}
