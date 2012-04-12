#include <rfid.h>
#include <globals_regvars.h>

/******STATES AND FLAGS******/
#define DISABLE 0
#define ENABLE  1

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
void mcu_sleep();
void mcu_statusGlobalInterrupt(char set);
void mcu_statusRX(char set);
void mcu_statusTimerInterrupt(char set);

/******FUNCTIONS******/
inline void crc16_ccitt_readReply(unsigned int numDataBytes);
int mcu_powerIsGood();
int mcu_getTimerValue();

/******TOOLS******/
unsigned short mcu_swapBytes(unsigned short val);

/****MASSIVE FUNCTIONS****/ //For functions that are just too long to place in middle
void mcu_sendToReader(volatile unsigned char *data, unsigned char numOfBits);