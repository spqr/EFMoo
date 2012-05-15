#pragma once

#include <efm32.h>          //Info specific to chip
#include <efm32_chip.h>     //Chip
#include <efm32_cmu.h>      //Clock Management
#include <efm32_emu.h>      //Energy Management
#include <efm32_gpio.h>     //General Purpose I/O
#include <efm32_timer.h>    //Low Energy Timer
#include <efm32_wdog.h>     //Watchdog
#include <efm32_vcmp.h>     //Voltage Comparator

#include <rfid.h>
#include <globals_regvars.h>
#include <interrupts.h>

/******COMPONENT INIT SETTINGS******/
/* Declare VCMP Init struct */
#define VOLTAGE_LEVEL 1.8
#define TRIGGER_LEVEL (uint32_t)((VOLTAGE_LEVEL - (float) 1.667) / (float) 0.034)


/******PIN CONFIGURATION******/
//This pin configuration is defined for the EFM32G890F128. Other chips can be configured.
//Cap Sense Port D14
#define CAP_SENSE_PORT gpioPortD
#define CAP_SENSE_PIN 14
//RX Port C14 **NOTE** Chosen as Timer1 CC0 output Loc0 pin
#define RX_PORT gpioPortC
#define RX_PIN 14
//RX Enable Port C5
#define RX_EN_PORT gpioPortC
#define RX_EN_PIN 5
//TX Port D1 **NOTE** Chosen as Timer0 CC0 output Loc3 pin
#define TX_PORT gpioPortD
#define TX_PIN 1

/******STATES AND FLAGS******/
#define DISABLE 0
#define ENABLE  1

#define STATE_READY               0
#define STATE_ARBITRATE           1
#define STATE_REPLY               2
#define STATE_ACKNOWLEDGED        3
#define STATE_OPEN                4
#define STATE_SECURED             5
#define STATE_KILLED              6
#define STATE_READ_SENSOR         7

#define BIT0 1
#define BIT1 2
#define BIT2 8
#define BIT3 16
#define BIT4 32
#define BIT6 64
#define BIT7 128
#define BIT8 256

/******PROCESSES******/
void mcu_clearTimer();
void mcu_enterLowPowerState();
void mcu_init();
void mcu_initPins();
void mcu_initReceiveClock();
void mcu_initSendClock();
void mcu_resetTimer();
void mcu_setReceiveTimers();
void mcu_setReceiveInterrupts();
void mcu_setSendTimers();
void mcu_sleep();
void mcu_statusGlobalInterrupt(char set);
void mcu_statusRX(char set);
void mcu_statusSendTimer(char set);
void mcu_statusTimerInterrupt(char set);

/******FUNCTIONS******/
inline void crc16_ccitt_readReply(unsigned int numDataBytes);
int mcu_powerIsGood();
int mcu_getTimerValue();

/******TOOLS******/
unsigned short mcu_swapBytes(unsigned short val);

/****MASSIVE FUNCTIONS****/ //For functions that are just too long to place in middle
void mcu_sendToReader(volatile unsigned char *data, unsigned char numOfBits);