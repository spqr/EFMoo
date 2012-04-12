#include <mymoo.h>
#if (MOO_MCU == MOO_MCU_MSP430)
	#include <MSP430_mcu.h>
#elif (MOO_MCU == MOO_MCU_EFM32)
	#include <EFM32_mcu.h>
#endif