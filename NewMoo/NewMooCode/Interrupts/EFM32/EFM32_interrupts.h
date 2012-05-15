#include <mcu.h>
#include <rfid.h>

#include <efm32_emu.h>
#include <efm32_gpio.h>
#include <efm32_vcmp.h>

void VCMP_IRQHandler();
void TIMER0_IRQHandler();