#include <mymoo.h>
#if (MOO_MCU == MOO_MCU_MSP430)
	#include <MPS430_interrupts.h>
#elif (MOO_MCU == MOO_MCU_EFM32)
	#include <EFM32_interrupts.h>
#endif