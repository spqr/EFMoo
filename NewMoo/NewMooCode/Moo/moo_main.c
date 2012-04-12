
#include <moo_main.h>

//Define Globals
unsigned short TRcal = 0;
volatile unsigned char* destorig = &cmd[0];

/* 
 * Main Method
 *  - Initializes chip
 *  - Shutsdown chip if insufficient power
 *  - Enters run mode otherwise
 */
int main(){
	
	//Startup the MCU
	mcu_init();
	mcu_initPins();
	
	//If we don't have enough power, then shutdown.
	if (!mcu_powerIsGood())
		mcu_sleep();
	
	mcu_initReceiveClock();
	
	initRunMode();
	executeRunMode();
	
	return 0;
}

/************************/
/***** INIT RUNMODE *****/
/************************/

void initRunMode() {
	#if ENABLE_SLOTS
		setupSlots();
	#endif
	
	mcu_clearTimer();
	
	//TODO: Figure out that dest = destorig shit
	dest = destorig;
	
	#if !(ENABLE_SLOTS)
		queryReplyCRC = crc16_ccitt(&queryReply[0],2);
		queryReply[3] = (unsigned char) queryReplyCRC;
		queryReply[2] = (unsigned char) mcu_swapBytes(queryReplyCRC);
	#endif

	#if SENSOR_DATA_IN_ID
		// this branch is for sensor data in the id
		ackReply[2] = SENSOR_DATA_TYPE_ID;
		state = STATE_READ_SENSOR;
		timeToSample++;
	#else
		ackReplyCRC = crc16_ccitt(&ackReply[0], 14);
		ackReply[15] = (unsigned char) ackReplyCRC;
		ackReply[14] = (unsigned char) mcu_swapBytes(ackReplyCRC);
	#endif
	
	#if ENABLE_SESSIONS
		initialize_sessions();
	#endif

	state = STATE_READY;
}

void setupSlots() {
	// setup int epc
	epc = ackReply[2]<<8;
	epc |= ackReply[3];

	// calculate RN16_1 table
	for (Q = 0; Q < 16; Q++)
	{
		rn16 = epc^Q;
		lfsr();

		if (Q > 8) {
			RN16[(Q<<1)-9] = mcu_swapBytes(rn16);
			RN16[(Q<<1)-8] = rn16;
		} else {
			RN16[Q] = rn16;
		}
	}
}

void lfsr() {
	// calculate LFSR
	rn16 = (rn16 << 1) | (((rn16 >> 15) ^ (rn16 >> 13) ^ (rn16 >> 9) ^ (rn16 >> 8)) & 1);
	rn16 = rn16 & 0xFFFF;

	// fit 2^Q-1
	rn16 = rn16>>(15-Q);
}

