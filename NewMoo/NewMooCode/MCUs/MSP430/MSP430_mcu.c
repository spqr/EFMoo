#include <MSP430_mcu.h>

/******PROCESSES******/
void mcu_clearTimer(){
	TACTL = 0;
}

void mcu_enterLowPowerState(){
	_BIS_SR(LPM4_bits | GIE);
}

void mcu_init(){
	WDTCTL = WDTPW + WDTHOLD; // Stop Watchdog Timer
}

void mcu_initPins(){
	P1SEL = 0;
	P2SEL = 0;

	P1IE = 0;	P1IFG = 0;
	P2IE = 0;	P2IFG = 0;

	P1OUT = 0;	P2OUT = 0;	P3OUT = 0;	P4OUT = 0;	P5OUT = 0;	P6OUT = 0;	P8OUT = 0;
	
	P1DIR = TEMP_POWER | ACCEL_POWER | TX_PIN | RX_EN_PIN;
	P4DIR = CAP_SENSE | LED_POWER | VSENSE_POWER;
	P5DIR = FLASH_CE | FLASH_SIMO | FLASH_SCK;
	P8DIR = CRYSTAL_OUT;
}

void mcu_initReceiveClock(){
	BCSCTL1 = XT2OFF + RSEL3 + RSEL1 + RSEL0;
	DCOCTL = 0;
	BCSCTL2 = 0; // Rext = ON
}

void mcu_initSendClock(){
	BCSCTL1 = XT2OFF + RSEL3 + RSEL0;
	DCOCTL = DCO2 + DCO1;
}

void mcu_resetTimer(){
	TAR = 0;
}

void mcu_setReceiveTimers(){
	// setup port interrupt on pin 1.2
	P1SEL &= ~BIT2;  //Disable TimerA2, so port interrupt can be used
	
	// Setup timer.
	TACTL = 0;
	mcu_resetTimer();
	TACCR0 = 0xFFFF;    // Set up TimerA0 register as Max
	TACCTL0 = 0;
	TACCTL1 = SCS + CAP;   //Synchronize capture source and capture mode
	TACTL = TASSEL1 + MC1 + TAIE;  // SMCLK and continuous mode and Timer_A interrupt enabled.
	return;
}

void mcu_setReceiveInterrupts(){
	P1IE = 0;
	P1IES &= ~RX_PIN; // Make positive edge for port interrupt to detect start of delimeter
	P1IFG = 0;  // Clear interrupt flag
	P1IE |= RX_PIN; // Enable Port1 interrupt
	return;
}

void mcu_sleep(){
	P1OUT &= ~RX_EN_PIN;
	// enable port interrupt for voltage supervisor
	P2IES = 0;
	P2IFG = 0;
	P2IE |= VOLTAGE_SV_PIN;
	P1IE = 0;
	P1IFG = 0;
	TACTL = 0;

	mcu_statusGlobalInterrupt(DISABLE); // temporarily disable GIE so we can sleep and enable interrupts at the same time
	P2IE |= VOLTAGE_SV_PIN; // Enable Port 2 interrupt

	//Throw interrupt if power happens to be good before going into low power mode
	if (mcu_powerIsGood())
		P2IFG = VOLTAGE_SV_PIN;

	//Enter low power mode. Also renables interrupts.
	mcu_enterLowPowerState();
}

void mcu_statusGlobalInterrupt(char set){
	if (set == ENABLE){
		//enable interrupts
		_BIS_SR(GIE);
	} else if (set == DISABLE) {
		//disable interrupts
		_BIC_SR(GIE);
	}
}

void mcu_statusRX(char set){
	if (set == ENABLE){
		//enable RX Pin
		P1OUT |= RX_EN_PIN;
	} else if (set == DISABLE) {
		//disable RX Pin
		P1OUT &= ~RX_EN_PIN;
	}
}

void mcu_statusTimerInterrupt(char set){
	if (set == ENABLE){
		TACCTL1 |= CCIE;
	} else if (set == DISABLE) {
		TACCTL1 &= ~CCIE;
	}
}

/******FUNCTIONS******/
inline void crc16_ccitt_readReply(unsigned int numDataBytes)
{

	// shift everything over by 1 to accomodate leading "0" bit.
	// first, grab address of beginning of array
	readReply[numDataBytes + 2] = 0; // clear out this spot for the loner bit of handle
	readReply[numDataBytes + 4] = 0; // clear out this spot for the loner bit of crc
	bits = (unsigned short) &readReply[0];
	// shift all bytes and later use only data + handle
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	asm("RRC.b @R5+");
	// store loner bit in array[numDataBytes+2] position
	asm("RRC.b @R5+");
	// make first bit 0
	readReply[0] &= 0x7f;

	// compute crc on data + handle bytes
	readReplyCRC = crc16_ccitt(&readReply[0], numDataBytes + 2);
	readReply[numDataBytes + 4] = readReply[numDataBytes + 2];
	// XOR the MSB of CRC with loner bit.
	readReply[numDataBytes + 4] ^= mcu_swapBytes(readReplyCRC); // XOR happens with MSB of lower nibble
	// Just take the resulting bit, not the whole byte
	readReply[numDataBytes + 4] &= 0x80;

	unsigned short mask = mcu_swapBytes(readReply[numDataBytes + 4]);
	mask >>= 3;
	mask |= (mask >> 7);
	mask ^= 0x1020;
	mask >>= 1;  // this is because the loner bit pushes the CRC to the left by 1
	// but we don't shift the crc because it should get pushed out by 1 anyway
	readReplyCRC ^= mask;

	readReply[numDataBytes + 3] = (unsigned char) readReplyCRC;
	readReply[numDataBytes + 2] |= (unsigned char) (mcu_swapBytes(readReplyCRC) & 0x7F);
}

int mcu_powerIsGood(){
	return P2IN & VOLTAGE_SV_PIN;
}

int mcu_getTimerValue(){
	return TAR;
}

/******TOOLS******/
unsigned short mcu_swapBytes(unsigned short val){
	return __swap_bytes(val);
}


/****Massive SendToReader Function****/

/******************************************************************************
*   Pin Set up
*   P1.1 - communication output
*******************************************************************************/
void mcu_sendToReader(volatile unsigned char *data, unsigned char numOfBits) {

	mcu_initSendClock();

	TACTL &= ~TAIE;
	TAR = 0;
	// assign data address to dest
	dest = data;
	// Setup timer
	P1SEL |= TX_PIN; //  select TIMER_A0
	TACTL |= TACLR;   //reset timer A
	TACTL = TASSEL1 + MC0;     // up mode

	TACCR0 = 5;  // this is 1 us period( 3 is 430x12x1)

	TAR = 0;
	TACCTL0 = OUTMOD2; // RESET MODE

	#if MILLER_4_ENCODING
		BCSCTL2 |= DIVM_1;
	#endif

	/*******************************************************************************
	*   The starting of the transmitting code. Transmitting code must send 4 or 16
	*   of M/LF, then send 010111 preamble before sending data package. TRext
	*   determines how many M/LFs are sent.
	*
	*   Used Register
	*   R4 = CMD address, R5 = bits, R6 = counting 16 bits, R7 = 1 Word data, R9 =
	*   temp value for loop R10 = temp value for the loop, R13 = 16 bits compare,
	*   R14 = timer_value for 11, R15 = timer_value for 5
	*******************************************************************************/


	//<-------- The below code will initiate some set up ---------------------->//
	//asm("MOV #05h, R14");
	//asm("MOV #02h, R15");
	bits = TRext;           // 5 cycles
	asm("NOP");             // 1 cycles
	asm("CMP #0001h, R5");  // 1 cycles
	asm("JEQ TRextIs_1");   // 2 cycles
	asm("MOV #0004h, R9");  // 1 cycles
	asm("JMP otherSetup");  // 2 cycles

	// initialize loop for 16 M/LF
	asm("TRextIs_1:");
	asm("MOV #000fh, R9");    // 2 cycles    *** this will chagne to right value
	asm("NOP");

	//
	asm("otherSetup:");
	bits = numOfBits;         // (2 cycles).  This value will be adjusted. if
	                 // numOfBit is constant, it takes 1 cycles
	asm("NOP");               // (1 cycles), zhangh 0316

	asm("MOV #0bh, R14");     // (2 cycles) R14 is used as timer value 11, it
	                 // will be 2 us in 3 MHz
	asm("MOV #05h, R15");     // (2 cycles) R15 is used as tiemr value 5, it
	                 // will be 1 us in 3 MHz
	asm("MOV @R4+, R7");      // (2 cycles) Assign data to R7
	asm("MOV #0010h, R13");   // (2 cycles) Assign decimal 16 to R13, so it will
	                 // reduce the 1 cycle from below code
	asm("MOV R13, R6");       // (1 cycle)
	asm("SWPB R7");           // (1 cycle)    Swap Hi-byte and Low byte
	asm("NOP");
	asm("NOP");
	// new timing needs 11 cycles
	asm("NOP");
	//asm("NOP");       // up to here, it make 1 to 0 transition.
	//<----------------2 us --------------------------------
	asm("NOP");   // 1
	asm("NOP");   // 2
	asm("NOP");   // 3
	asm("NOP");   // 4
	asm("NOP");   // 5
	asm("NOP");   // 6
	//asm("NOP");   // 7
	//asm("NOP");   // 8
	//asm("NOP");   // 9
	// <---------- End of 1 us ------------------------------
	// The below code will create the number of M/LF.  According to the spec,
	// if the TRext is 0, there are 4 M/LF.  If the TRext is 1, there are 16
	// M/LF
	// The upper code executed 1 M/LF, so the count(R9) should be number of M/LF
	// - 1
	//asm("MOV #000fh, R9");    // 2 cycles  *** this will chagne to right value
	asm("MOV #0001h, R10");   // 1 cycles, zhangh?
	// The below code will create the number base encoding waveform., so the
	// number of count(R9) should be times of M
	// For example, if M = 2 and TRext are 1(16, the number of count should be
	// 32.
	asm("M_LF_Count:");
	//16 NOPS
	asm("NOP");	asm("NOP");	asm("NOP"); asm("NOP");
	asm("NOP");	asm("NOP");	asm("NOP"); asm("NOP");
	asm("NOP");	asm("NOP");	asm("NOP"); asm("NOP");
	asm("NOP");	asm("NOP");	asm("NOP"); asm("NOP");
	// asm("NOP");   // 17

	asm("CMP R10, R9");       // 1 cycle
	asm("JEQ M_LF_Count_End"); // 2 cycles
	asm("INC R10");           // 1 cycle
	asm("NOP");   // 22
	asm("JMP M_LF_Count");      // 2 cycles

	asm("M_LF_Count_End:");
	// this code is preamble for 010111 , but for the loop, it will only send
	// 01011
	asm("MOV #5c00h, R9");      // 2 cycles
	asm("MOV #0006h, R10");     // 2 cycles

	asm("NOP");                   // 1 cycle zhangh 0316, 2

	// this should be counted as 0. Therefore, Assembly DEC line should be 1
	// after executing
	asm("Preamble_Loop:");
	asm("DEC R10");               // 1 cycle
	asm("JZ last_preamble_set");          // 2 cycle
	asm("RLC R9");                // 1 cycle
	asm("JNC preamble_Zero");     // 2 cycle      .. up to 6
	// this is 1 case for preamble
	asm("NOP");
	asm("NOP");                   // 1 cycle
	asm("MOV R14, TACCR0");       // 3 cycle      .. 10
	asm("NOP");
	asm("NOP");
	asm("NOP");
	asm("NOP");                   // 1 cycle
	asm("MOV R15, TACCR0");       // 3 cycle      .. 18
	asm("NOP");
	asm("NOP");
	asm("NOP");
	asm("NOP");                   // .. 22
	asm("JMP Preamble_Loop");     // 2 cycles   .. 24

	// this is 0 case for preamble
	asm("preamble_Zero:");
	
	asm("NOP");	asm("NOP");	asm("NOP"); asm("NOP");
	asm("NOP");	asm("NOP");	asm("NOP"); asm("NOP"); //16 NOPS
	asm("NOP");	asm("NOP");	asm("NOP"); asm("NOP");
	asm("NOP");	asm("NOP");	asm("NOP"); asm("NOP");


	asm("JMP Preamble_Loop");     // 2 cycles .. 24

	asm("last_preamble_set:");
	asm("NOP");			// 4
	asm("NOP");
	asm("NOP");    // TURN ON
	asm("NOP");
	asm("NOP");             // 1 cycle
	asm("MOV.B R14, TACCR0");// 3 cycles
	asm("NOP");
	asm("NOP");
	asm("NOP");
	asm("NOP");
	asm("MOV.B R15, TACCR0");
	asm("NOP");
	asm("NOP");
	asm("NOP");
	asm("NOP");
	asm("NOP");
	// Here are another 4 cycles. But 3~5 cycles might also work, need to try.
	asm("NOP");
	asm("NOP");
	asm("NOP");
	asm("NOP");
	//    asm("NOP");


	//<------------- end of initial set up

	/***********************************************************************
	*   The main loop code for transmitting data in 3 MHz.  This will transmit data
	*   in real time.
	*   R5(bits) and R6(word count) must be 1 bigger than desired value.
	*   Ex) if you want to send 16 bits, you have to store 17 to R5.
	************************************************************************/

	// this is starting of loop
	asm("LOOPAGAIN:");
	asm("DEC R5");                              // 1 cycle
	asm("JEQ Three_Cycle_Loop_End");            // 2 cycle
	//<--------------loop condition ------------
	//    asm("NOP");                                 // 1 cycle
	asm("RLC R7");                              // 1 cycle
	asm("JNC bit_is_zero");	                // 2 cycles  ..6

	// bit is 1
	asm("bit_is_one:");
	//    asm("NOP");                               // 1 cycle
	asm("MOV R14, TACCR0");                   // 3 cycles   ..9

	asm("DEC R6");                              // 1 cycle  ..10
	asm("JNZ bit_Count_Is_Not_16");              // 2 cycle    .. 12
	// This code will assign new data from reply and then swap bytes.  After
	// that, update R6 with 16 bits
	//asm("MOV @R4+, R7");
	#if USE_2618
	asm("MOV R15, TACCR0");                   // 3 cycles   .. 15
	#else
	asm("MOV R15, TACCR0");                   // 3 cycles   .. 15
	#endif

	asm("MOV R13, R6");                         // 1 cycle   .. 16
	asm("MOV @R4+, R7");                        // 2 cycles  .. 18

	asm("SWPB R7");                             // 1 cycle    .. 19
	//asm("MOV R13, R6");                         // 1 cycle
	// End of assigning data byte
	asm("NOP");
	asm("NOP");
	asm("NOP");
	asm("JMP LOOPAGAIN");                       // 2 cycle    .. 24

	asm("seq_zero:");
	#if USE_2618
	asm("MOV R15, TACCR0");         // 3 cycles       ..3
	#else
	asm("MOV R15, TACCR0");         // 3 cycles       ..3
	#endif
	asm("NOP");
	asm("NOP");
	asm("NOP");                     // 1 cycles .. 6


	// bit is 0, so it will check that next bit is 0 or not
	asm("bit_is_zero:");				// up to 6 cycles
	asm("DEC R6");                      // 1 cycle   .. 7
	asm("JNE bit_Count_Is_Not_16_From0");           // 2 cycles  .. 9
	// bit count is 16
	asm("DEC R5");                      // 1 cycle   .. 10
	asm("JEQ Thirteen_Cycle_Loop_End");     // 2 cycle   .. 12
	// This code will assign new data from reply and then swap bytes.  After
	// that, update R6 with 16 bits
	asm("MOV @R4+,R7");                 // 2 cycles     14
	asm("SWPB R7");                     // 1 cycle      15
	asm("MOV R13, R6");                 // 1 cycles     16
	// End of assigning new data byte
	asm("RLC R7");		        // 1 cycles     17
	asm("JC nextBitIs1");	        // 2 cycles  .. 19
	// bit is 0
	asm("MOV R14, TACCR0");             // 3 cycles  .. 22
	// Next bit is 0 , it is 00 case
	asm("JMP seq_zero");                // 2 cycles .. 24

	// <---------this code is 00 case with no 16 bits.
	asm("bit_Count_Is_Not_16_From0:");                  // up to 9 cycles
	asm("DEC R5");                          // 1 cycle      10
	asm("JEQ Thirteen_Cycle_Loop_End");     // 2 cycle    ..12
	asm("NOP");         	            // 1 cycles    ..13
	asm("NOP");                             // 1 cycles    ..14
	asm("NOP");                             // 1 cycles    ..15
	asm("RLC R7");	                    // 1 cycle     .. 16
	asm("JC nextBitIs1");	            // 2 cycles    ..18

	asm("MOV R14, TACCR0");               // 3 cycles   .. 21
	asm("NOP");                         // 1 cycle   .. 22
	asm("JMP seq_zero");        // 2 cycles    .. 24

	// whenever current bit is 0, then next bit is 1
	asm("nextBitIs1:");     // 18
	asm("NOP");
	asm("NOP");
	asm("NOP");
	asm("NOP");
	asm("NOP");
	asm("NOP");       // 24

	asm("NOP");
	asm("NOP");
	asm("NOP");
	asm("NOP");
	asm("JMP bit_is_one");  // end of bit 0 .. 6

	asm("bit_Count_Is_Not_16:");       // up to here 14
	asm("MOV R15, TACCR0");             // 3 cycles   .. 15

	asm("NOP");                               // 1 cycle .. 16
	asm("NOP");                               // 1 cycle .. 17
	asm("NOP");                               // 1 cycle .. 18
	asm("NOP");                               // 1 cycle .. 19
	asm("NOP");                               // 1 cycle .. 20
	asm("NOP");                               // 1 cycle .. 21
	asm("NOP");                               // 1 cycle .. 22
	asm("JMP LOOPAGAIN");     // 2 cycle          .. 24

	// below code is the end of loop code
	asm("Three_Cycle_Loop_End:");
	asm("JMP lastBit");     // 2 cycles   .. 5

	asm("Thirteen_Cycle_Loop_End:");  // up to 12
	//14 NOPS
	asm("NOP");	asm("NOP");	asm("NOP"); asm("NOP");
	asm("NOP");	asm("NOP");	asm("NOP"); asm("NOP");
	asm("NOP");	asm("NOP");	asm("NOP"); asm("NOP");
	asm("NOP");	asm("NOP");
	asm("JMP lastBit"); // 16
	/***********************************************************************
	*   End of main loop
	************************************************************************/
	// this is last data 1 bit which is dummy data
	asm("lastBit:");  // up to 4
	asm("NOP");       // 5
	asm("NOP");
	asm("MOV.B R14, TACCR0");// 3 cycles
	asm("NOP");
	asm("NOP");
	asm("NOP");
	asm("MOV.B R15, TACCR0");
	asm("NOP");
	asm("NOP");
	// experiment

	asm("NOP");

	//TACCR0 = 0;

	TACCTL0 = 0;  // DON'T NEED THIS NOP
	mcu_initReceiveClock();
}
