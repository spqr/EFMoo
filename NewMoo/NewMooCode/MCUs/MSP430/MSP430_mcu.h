#include <msp430x26x.h>
#include <rfid.h>
#include <globals_regvars.h>

#define USE_2618  1

// Port 1
#define TEMP_POWER     BIT0       // output
#define TX_PIN         BIT1       // output
#define RX_PIN         BIT2       // input
#define RX_EN_PIN      BIT3       // output
#define ACCEL_POWER    BIT4       // output
#define DEBUG_1_5      BIT5       // output/input
#define DEBUG_1_6      BIT6       // output/input
#define DEBUG_1_7      BIT7       // output/input

// Port 2
//Supervisor in: PCB is designed to P6.7, should wire connect P6.7 and P2.0
#define VOLTAGE_SV_PIN      BIT0       // output/input
#define DEBUG_2_1      BIT1       // output/input
#define DEBUG_2_2      BIT2       // output/input
#define IDLE_2_3       BIT3       // idle pin
#define IDLE_2_4       BIT4       // idle pin
#define IDLE_2_5       BIT5       // idle pin
#define IDLE_2_6       BIT6       // idle pin
#define IDLE_2_7       BIT7       // idle pin

// Port 3
#define CLK_A          BIT0       // output unless externally driven
#define SDA_B          BIT1       // input (connected to 10k pullup res)
#define SCL_B          BIT2       // input (connected to 10k pullup res)
#define CLK_B          BIT3       // output unless externally driven
#define TX_A           BIT4       // output unless externally driven
#define RX_A           BIT5       // output unless externally driven
#define DEBUG_3_6      BIT6       // output/input
#define DEBUG_3_7      BIT7       // output/input

// Port 4
#define DEBUG_4_0      BIT0       // connect to SV_IN by 0 ohm
#define CAP_SENSE      BIT1       // output/input
#define LED_POWER      BIT2       // output
#define VSENSE_POWER   BIT3       // output
#define DEBUG_4_4      BIT4       // output/input
#define DEBUG_4_5      BIT5       // output/input
#define DEBUG_4_6      BIT6       // output/input
#define DEBUG_4_7      BIT7       // output/input

// Port 5
#define FLASH_CE       BIT0       // output
#define FLASH_SIMO     BIT1       // output
#define FLASH_SOMI     BIT2       // input
#define FLASH_SCK      BIT3       // output
#define DEBUG_5_4      BIT4       // output unless externally driven
#define DEBUG_5_5      BIT5       // output unless externally driven
#define IDLE_5_6       BIT6       // idle pin
#define DEBUG_5_7      BIT7       // output unless externally driven

// Port 6
#define ACCEL_Z        BIT0       // input
#define ACCEL_Y        BIT1       // input
#define ACCEL_X        BIT2       // input
#define TEMP_EXT_IN    BIT3       // input
#define VSENSE_IN      BIT4       // input
#define DEBUG_6_5      BIT5       // output unless externally driven
#define DEBUG_6_6      BIT6       // output unless externally driven
#define DEBUG_6_7      BIT7       // input

// Port 8: Zhangh, need to reconfirm
#define CRYSTAL_IN     BIT7       // input
#define CRYSTAL_OUT    BIT6       // output

// Analog Inputs (ADC In Channel)
#define INCH_ACCEL_Z     INCH_0
#define INCH_ACCEL_Y     INCH_1
#define INCH_ACCEL_X     INCH_2
#define INCH_TEMP_EXT_IN INCH_3
#define INCH_VSENSE_IN   INCH_4
#define INCH_DEBUG_6_5   INCH_5
#define INCH_DEBUG_6_6   INCH_6

#define STATE_READY               0
#define STATE_ARBITRATE           1
#define STATE_REPLY               2
#define STATE_ACKNOWLEDGED        3
#define STATE_OPEN                4
#define STATE_SECURED             5
#define STATE_KILLED              6
#define STATE_READ_SENSOR         7

#define DEBUG_PINS_ENABLED        0
#if DEBUG_PINS_ENABLED
	#define DEBUG_PIN5_HIGH               P3OUT |= BIT5;
	#define DEBUG_PIN5_LOW                P3OUT &= ~BIT5;
#else
	#define DEBUG_PIN5_HIGH
	#define DEBUG_PIN5_LOW
#endif


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