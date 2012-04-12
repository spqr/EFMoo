/* See license.txt for license information. */

#include <mcu.h>
#include <rfid.h>
#include <mymoo.h>

unsigned short Q = 0;
unsigned short slot_counter = 0;
unsigned short shift = 0;
unsigned int read_counter = 0;
unsigned int sensor_counter = 0;
unsigned char delimiterNotFound = 0;
unsigned char TRext = 0;
unsigned short divideRatio = 0;
unsigned char subcarrierNum = 0;
unsigned char timeToSample = 0;
unsigned short inInventoryRound = 0;
volatile short state;
volatile unsigned char cmd[CMD_BUFFER_SIZE+1]; // stored command from reader

volatile unsigned char queryReply[]= { 0x00, 0x03, 0x00, 0x00};

// ackReply:  First two bytes are the preamble.  Last two bytes are the crc.
volatile unsigned char ackReply[] = { 0x30, 0x00, EPC, 0x00, 0x00};

unsigned short queryReplyCRC, ackReplyCRC, readReplyCRC;

// first 8 bits are the EPCGlobal identifier, followed by a 12-bit tag designer
// identifer (made up), followed by a 12-bit model number
volatile unsigned char tid[] = { 0xE2, TID_DESIGNER_ID_AND_MODEL_NUMBER };

// just a one byte placeholder for now
volatile unsigned char usermem[] = { 0x00 };

volatile unsigned char readReply[] = {
    // header - 1 bit - 0 if successful, 1 if error code follows memory words - hardcoded to 16 bits of 0xffff for now
    // rn - 16 bits - hardcoded to 0xf00f for now
    // crc-16 - 16 bits - precomputed as 0x06 0x72
    // filler - 15 bits of nothing (don't send)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x08, 0x09, 0x10, 0x11,
    0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19};


void sendToReader(volatile unsigned char *data, unsigned char numOfBits) {
	mcu_sendToReader(data, numOfBits);
}

/**
 * This code comes from the Open Tag Systems Protocol Reference Guide version
 * 1.1 dated 3/23/2004.
 * (http://www.opentagsystems.com/pdfs/downloads/OTS_Protocol_v11.pdf)
 * No licensing information accompanied the code snippet.
 **/
unsigned short crc16_ccitt(volatile unsigned char *data, unsigned short n) {
	register unsigned short i, j;
	register unsigned short crc_16;

	crc_16 = 0xFFFF; // Equivalent Preset to 0x1D0F
	for (i=0; i<n; i++) {
		crc_16^=data[i] << 8;
		for (j=0;j<8;j++) {
			if (crc_16&0x8000) {
				crc_16 <<= 1;
				crc_16 ^= 0x1021; // (CCITT) x16 + x12 + x5 + 1
			}
			else {
				crc_16 <<= 1;
			}
		}
	}
	return(crc_16^0xffff);
}

void handle_query(volatile short nextState)
{
	mcu_resetTimer();
	#if (!ENABLE_SLOTS)  && (!ENABLE_SESSIONS)
		while ( mcu_getTimerValue() < 90 ); // if bit test is 22
		//mcu_statusRX(DISABLE);   // turn off comparator
		mcu_statusTimerInterrupt(DISABLE);     // Disable capturing and comparing interrupt
		mcu_resetTimer();
	#elif (!ENABLE_SLOTS) && ENABLE_SESSIONS
		while ( mcu_getTimerValue() < 160 ); // if bit test is 22
		//mcu_statusRX(DISABLE);   // turn off comparator
		mcu_statusTimerInterrupt(DISABLE);     // Disable capturing and comparing interrupt
		mcu_resetTimer();
	#else
		//mcu_statusRX(DISABLE);   // turn off comparator
		mcu_statusTimerInterrupt(DISABLE);     // Disable capturing and comparing interrupt
		mcu_resetTimer();
	#endif

	// set up for TRcal
	if ( cmd[0] & BIT3)
	{
		divideRatio = 21; //64/3;
	} else {
		divideRatio = 8;
	}
	
	// set up for subcarrier symbol
	subcarrierNum = cmd[0] & (BIT2 | BIT1);
	if (subcarrierNum == 0) {
		subcarrierNum = 1;
	} else if ( subcarrierNum == 2 ) {
		subcarrierNum = 2;
	} else if (subcarrierNum == 4) {
		subcarrierNum = 4;
	} else{
		subcarrierNum = 8;
	}

	// set up for TRext
	if (cmd[0] & BIT0) {
		TRext = 1;
	} else {
		TRext = 0;
	}

	#if ENABLE_SESSIONS
		// command-specific bit masks
		#define QUERY_SEL_MASK		0xC0
		#define QUERY_SESSION_MASK	0x30
		#define QUERY_TARGET_MASK	0x08

		// command-specific bit flags
		#define QUERY_SEL_SL 		0xC0
		#define QUERY_SEL_NOTSL 	0x80

		unsigned short sel = cmd[1] & QUERY_SEL_MASK;
		unsigned short session = (cmd[1] & QUERY_SESSION_MASK) >> 4;
		unsigned short target = cmd[1] & QUERY_TARGET_MASK;
		unsigned short match = 0;

		#define TARGETISEQUAL(t,s) ((t == 0x00 && s == SESSION_STATE_A) || (t == 0x08  && s == SESSION_STATE_B))

		// if we are already in an inventory round and the session matches the
		// previous session, invert the session inventory flag
		if ( state == STATE_ACKNOWLEDGED || state == STATE_OPEN || state == STATE_SECURED ) {
			if ( session == previous_session ) {
				// invert session's inventory flag
				if ( session_table[session] == SESSION_STATE_A )
					session_table[session] = SESSION_STATE_B;
				else
					session_table[session] = SESSION_STATE_A;
			}
		}

		// now figure out if the SL and session flags match. Let's look at the SL flag
		// first.
		if ( sel < QUERY_SEL_NOTSL || sel == QUERY_SEL_SL && SL == SL_ASSERTED || sel == QUERY_SEL_NOTSL && SL == SL_NOT_ASSERTED ) {
			// SL flag matches -- now how about the session flag?
			if ( TARGETISEQUAL(target,session_table[session]) ) {
				// session match too
				match = 1;
			}
		}

		if ( ! match )
		{
			// no matching SL/session flags. don't respond and transistion to READY
			// state.
			mcu_statusTimerInterrupt(DISABLE);     // Disable capturing and comparing interrupt
			mcu_resetTimer();
			state = STATE_READY;
			return;
		}

		// OK, got matching SL and session inventory flags, so we're in this inventory
		// round. save the session to the previous_session variable in case this is a
		// new session.
		previous_session = session;
	#else
		// we don't care about SL or session inventory flags at all. just proceed
		// as if we have matched on them.
	#endif

	#if ENABLE_SLOTS

		// next step is to built a slot counter

		// parse for Q number and choose a Q value randomly
		Q = (cmd[1] & 0x07)<<1;
		if ((cmd[2] & 0x80) == 0x80)
		Q += 0x01;

		// pick Q randomly
		slot_counter = Q >> shift;

		// HACK ALERT: the Impinj reader seems to output at least two Queries before
		// it sends along an Ack. If I followed the spec, I might well reply to the
		// first Query but not the second, owing to a nonzero slot counter. In this
		// case the reader would just send along QueryReps until the slot counter got
		// to zero, and I'd emit another response. Problem is, we're a
		// power-constrained device and we don't necessarily have the ability to
		// respond to a list of QueryReps. Therefore, I will check to see if I'm in an
		// inventory round -- that is, I've already seen a query -- and if I am,
		// pretend my slot counter is zero and respond right away.

		// slot counter built and it's 0. we can send a reply!
		if ( inInventoryRound == 1 || slot_counter == 0) {

			// compute a RN16
			loadRN16();

			// compute the CRC
			queryReplyCRC = crc16_ccitt(&queryReply[0],2);
			queryReply[3] = (unsigned char)queryReplyCRC;
			queryReply[2] = (unsigned char)mcu_swapBytes(queryReplyCRC);

			mcu_statusTimerInterrupt(DISABLE);     // Disable capturing and comparing interrupt
			while ( mcu_getTimerValue() < 140 );
			mcu_resetTimer();

			// send out the packet, and transition to STATE_REPLY
			sendToReader(&queryReply[0], 17);
			state = nextState;

			// mix up the RN16 table a bit for next time
			mixupRN16();
		}	
		// slot counter isn't 0, so we don't send a reply. We wait for a
		// followup QueryRep or QueryAdjust command. In the meantime,
		// we transition to STATE_ARBITRATE.
		else {
			mcu_statusTimerInterrupt(DISABLE);     // Disable capturing and comparing interrupt
			mcu_resetTimer();
			state = STATE_ARBITRATE;
		}

		inInventoryRound = 1;

	#else

		// we don't care about slots, so just send the packet and go to STATE_REPLY.
		sendToReader(&queryReply[0], 17);
		state = nextState;

	#endif

}

void handle_queryrep(volatile short nextState)
{

  	mcu_resetTimer();
	#if (!ENABLE_SESSIONS)
		while ( mcu_getTimerValue() < 150 );
	#endif
	
	//mcu_statusRX(DISABLE);   // turn off comparator
	mcu_statusTimerInterrupt(DISABLE);
	mcu_resetTimer();

	#if ENABLE_SESSIONS

		// command-specific bit masks
		#define QUERYREP_UPDNB0_MASK	0x01
		#define QUERYREP_UPDNB1_MASK	0x80

		// fyi, unless i'm missing something, the impinj reader doesn't always send
		// correct session values. for now i will comment this section out, until
		// i can test against a different reader firmware rev
		#if 0
			unsigned char session = (cmd[0] & QUERYREP_UPDNB0_MASK) << 1;
			unsigned char tmp = (cmd[1] & QUERYREP_UPDNB1_MASK) >> 7;
			session |= tmp;

			if ( session != previous_session )
			{
			// drop the packet
			return;
			}
		#else
			// hack hack hack
			unsigned char session = previous_session;
		#endif

		if ( state == STATE_ACKNOWLEDGED || state == STATE_OPEN || state == STATE_SECURED ) {
			// invert session's inventory flag
			if ( session_table[session] == SESSION_STATE_A )
				session_table[session] = SESSION_STATE_B;
			else
				session_table[session] = SESSION_STATE_A;
			state = STATE_READY;
			return;
		}
	#endif

	#if ENABLE_SLOTS
		slot_counter -= 1;
		if ( slot_counter != 0 ) {
			state = STATE_ARBITRATE;
			return;
		}
	#endif
	
	sendToReader(&queryReply[0], 17);
	state = nextState;
}

void handle_queryadjust(volatile short nextState)
{

	mcu_resetTimer();
	#if !(ENABLE_SLOTS) && !(ENABLE_SESSIONS)
		while ( mcu_getTimerValue() < 300 );
		//mcu_statusRX(DISABLE);   // turn off comparator
		mcu_statusTimerInterrupt(DISABLE);
		mcu_resetTimer();
	#endif

	#if ENABLE_SESSIONS

		// command-specific bit masks
		#define QUERYADJ_SESSION_MASK	0x0C

		unsigned short session = (cmd[0] & QUERYADJ_SESSION_MASK) >> 1;

		if ( session != previous_session ) {
			// drop the packet, but stay in the same state
			mcu_statusTimerInterrupt(DISABLE);     // Disable capturing and comparing interrupt
			mcu_resetTimer();
			return;
		}

		// if we got this packet in ACKNOWLEDGED, OPEN, or SECURED states, invert the
		// session flag and transition to STATE_READY to take myself out of this
		// inventory round.
		if ( state == STATE_ACKNOWLEDGED || state == STATE_OPEN || state == STATE_SECURED ) {
			// invert session's inventory flag
			if ( session_table[session] == SESSION_STATE_A )
				session_table[session] = SESSION_STATE_B;
			else
				session_table[session] = SESSION_STATE_A;
			state = STATE_READY;
			mcu_statusTimerInterrupt(DISABLE);     // Disable capturing and comparing interrupt
			mcu_resetTimer();
			return;
		}

	#endif

	#if ENABLE_SLOTS

		#define QUERYADJ_UPDNB0_MASK	0x01
		#define QUERYADJ_UPDNB1_MASK	0xC0

		unsigned char updn = (cmd[0] & QUERYADJ_UPDNB0_MASK) << 2;
		unsigned char tmp = (cmd[1] & QUERYADJ_UPDNB1_MASK) >> 6;
		updn |= tmp;

		if ( Q == 0xf && updn == 0x3 )
			updn = 0x0;
		if ( Q == 0x0 && updn == 0x1 )
			updn = 0x0;

		if ( updn == 0x6 )
			Q += 1;
		else if ( updn == 0x3 )
			Q -= 1;
		else if ( updn != 0x0 )
		{
			// impinj likes to send me plenty of updn values of 0x01, which isn't
			// valid. Spec says to ignore the command in these cases.
			return;
		}

		// pick Q randomly
		slot_counter = Q >> shift;

		// slot counter built and it's 0. we can send a reply!
		if (slot_counter == 0) {
			// compute a RN16
			loadRN16();

			// compute a CRC
			queryReplyCRC = crc16_ccitt(&queryReply[0],2);
			queryReply[3] = (unsigned char)queryReplyCRC;
			queryReply[2] = (unsigned char)mcu_swapBytes(queryReplyCRC);

			mcu_statusTimerInterrupt(DISABLE);     // Disable capturing and comparing interrupt
			mcu_resetTimer();

			// send out the packet, and transition to STATE_REPLY
			sendToReader(&queryReply[0], 17);
			state = nextState;

			// mix up the RN16 table a bit for next time
			mixupRN16();
		} else {
			// slot counter isn't 0, so we don't send a reply. We wait for a
			// followup QueryRep or QueryAdjust command. In the meantime,
			// we transition to STATE_ARBITRATE.
			state = STATE_ARBITRATE;
		}

	#else
		// we don't care about slots, so just send the packet and go to STATE_REPLY.
		sendToReader(&queryReply[0], 17);
		state = nextState;
	#endif
}

// Word to the wise: I've been testing this code against the Impinj RFIDemo,
// using the InventoryFilter page. It appears to me that it only sends the first
// three bytes pattern fields (aka the mask field in the spec) correctly. So
// don't expect it to be able to look for (say) entire EPCs correctly. Also,
// when testing this code out in RFIDDemo, put your mask data in hex in the
// leftmost part of the pattern field.
void handle_select(volatile short nextState)
{
	do_nothing();

	#if ENABLE_SESSIONS
		// command-specific bit masks
		#define SELECT_TARGET_MASK				0x0E
		#define SELECT_ACTIONB0_MASK			0x01
		#define SELECT_ACTIONB1_MASK			0xC0
		#define SELECT_MEMBANK_MASK             0x30
		#define SELECT_POINTERB0_MASK           0x0F
		#define SELECT_POINTERB1_MASK           0xF0
		#define SELECT_LENGTHB0_MASK            0x0F
		#define SELECT_LENGTHB1_MASK            0xF0
		#define SELECT_MASK_MASK                0x0F

		// command-specific bit flags
		#define SELECT_TARGET_SL				0x04

		unsigned short target = (cmd[0] & SELECT_TARGET_MASK) >> 1;
		unsigned short action = (cmd[0] & SELECT_ACTIONB0_MASK) << 2;
		unsigned short action2 = (cmd[1] & SELECT_ACTIONB1_MASK) >> 6;
		action |= action2;
		
		unsigned short membank = (cmd[1] & SELECT_MEMBANK_MASK) >> 4;
		unsigned short pointer = (cmd[1] & SELECT_POINTERB0_MASK) << 4;
		unsigned short pointer2 = (cmd[2] & SELECT_POINTERB1_MASK) >> 4;
		pointer |= pointer2;
		
		unsigned short length = (cmd[2] & SELECT_LENGTHB0_MASK) << 4;
		unsigned short length2 = (cmd[3] & SELECT_LENGTHB1_MASK) >> 4;
		length |= length2;

		// can only handle length fields > 0 and membanks == 0 are invalid
		if ( length <= 0 || membank == 0x00 )
			return;

		unsigned char *mask = (unsigned char *)&cmd[3];
		unsigned short maskbit = 3; // start match attempt at leftmost bit of mask field

		unsigned char *sourcebyte = (unsigned char *)0;
		
		// OK, this is a little confusing. The pointer parameter is an offset
		// into a buffer, with a value of 0 meaning "the leftmost bit" and
		// a value of 7 meaning "the rightmost bit of the first byte." But my
		// bitCompare function thinks 7 is the leftmost bit of the first byte
		// and 0 is the rightmost bit of that byte. So I'll need to do a little
		// mapping here.
		unsigned short sourcebyteoffset = ( pointer/8 );
		unsigned short sourcebit = (7 - (pointer % 8)); // pointer->bitCompare
		                           // translation

		if ( membank == 0x01 ) {
			// match on epc
			if ( sourcebyteoffset >= 8 )
				return;
			sourcebyte = (unsigned char *) &ackReply[2] + sourcebyteoffset;
		} else if ( membank == 0x02 ) {
			// matching on tid.
			if ( sourcebyteoffset >= 3 )
				return;
			sourcebyte = (unsigned char *) &tid[0] + sourcebyteoffset;
		} else {
			// matching on user field. for now this is just one byte long, so
			// mess with the params to make sure the values are sane in this context
			sourcebyte = (unsigned char *) usermem;
			sourcebit = 7;
			if ( length > 8 )
				length = 8;
		}

		unsigned short matching = 0;

		if ( bitCompare(sourcebyte,sourcebit,mask,maskbit,length) == 1 ) {
			matching = 1;
		}

		#define ASSERT(t) { \
			if (t == SELECT_TARGET_SL) \
				SL = SL_ASSERTED; \
			else session_table[t] = SESSION_STATE_A; \
		}

		#define DEASSERT(t) { \
			if (t == SELECT_TARGET_SL) \
				SL = SL_NOT_ASSERTED; \
			else \
				session_table[t] = SESSION_STATE_B; \
		}

		#define NEGATE(t) { \
			if (t == SELECT_TARGET_SL) \
				if ( SL == SL_ASSERTED ) \
					SL = SL_NOT_ASSERTED; \
				else \
					SL = SL_ASSERTED; \
			else \
				if ( session_table[t] == SESSION_STATE_A ) \
					session_table[t] = SESSION_STATE_B; \
				else \
					session_table[t] = SESSION_STATE_A; \
		}

		switch ( action ) {
			case 0x0:
				if ( matching )
					ASSERT(target)
				else
					DEASSERT(target);
				break;
			case 0x1:
				if ( matching )
					ASSERT(target);
				break;
			case 0x2:
				if ( ! matching )
					DEASSERT(target);
				break;
			case 0x3:
				if ( matching )
					NEGATE(target);
				break;
			case 0x4:
				if ( matching )
					DEASSERT(target)
				else
					ASSERT(target);
				break;
			case 0x5:
				if ( matching )
					DEASSERT(target);
				break;
			case 0x6:
				if (!  matching )
					ASSERT(target);
				break;
			case 0x7:
				if (!  matching )
					NEGATE(target);
				break;
			default:
				break;
		}

	#endif

	state = nextState;
}

void handle_ack(volatile short nextState)
{
	mcu_statusTimerInterrupt(DISABLE);
	mcu_resetTimer();
	if ( NUM_ACK_BITS == 20 )
		while ( mcu_getTimerValue() < 90 );
	else
		while ( mcu_getTimerValue() < 400 );          // on the nose for 3.5MHz
	mcu_resetTimer();

	#if ENABLE_HANDLE_CHECKING
		//unsigned char ack_b0 = ((last_handle_b0 & 0xFC) >> 2) ;
		//ack_b0 |= 0x40;
		//unsigned char ack_b1 = ((last_handle_b1 & 0xFC) >> 2);
		//ack_b1 = ack_b1 || (last_handle_b0 & 0x03) << 6;
		//unsigned char ack_b2 = ((last_handle_b1 & 0x03) << 6);

		//if ( ack_b0 != cmd[0] || ack_b1 != cmd[1] || ack_b2 != (cmd[2] & 0xC0) )
		//    return;
	#endif
	
	//mcu_statusRX(DISABLE);   // turn off comparator
	// after that sends tagResponse
	sendToReader(&ackReply[0], 129);
	state = nextState;
}

void handle_request_rn(volatile short nextState)
{
	mcu_statusTimerInterrupt(DISABLE);
	mcu_resetTimer();
	// FIXME FIXME
	// here's a mystery: if I enable this line below, I clobber the follow-up read
	// command. specifically, the read command's cmd[0] shows up as 0xFF.  if i
	// leave this line commented out, everything's fine.  theory #1 was that the
	// bit_in_enable line doesn't switch around fast enough. but i do the same
	// thing for all the other commands, and i don't have any problems. also, the
	// time space between request_rn and the read is *much larger* than betwen the
	// other commands. theory #1 disproved.  theory #2 was that i had fallen
	// asleep and wasn't processing the incoming bits. but it turns out that i am
	// wide awake. theory #2 disproven.  theory #3. generally when i see 0xff in
	// the cmd[0] field it means that the bit buffer got overwritten. as far as I
	// can tell, it hasn't, and there's plenty of room in the receiving buffer.
	// theory #3 disproven.  hmmm.
	//mcu_statusRX(DISABLE);   // turn off comparator
	if ( NUM_REQRN_BITS == 42 )
		while ( mcu_getTimerValue() < 80 );
	else if ( NUM_REQRN_BITS == 41 )
		while ( mcu_getTimerValue() < 170 );
	mcu_resetTimer();
	sendToReader(&queryReply[0], 33);
	if ( read_counter == 0xffff )
		read_counter = 0;
	else
		read_counter++;
	state = nextState;
}

void handle_read(volatile short nextState)
{

	#if SENSOR_DATA_IN_READ_COMMAND

		//mcu_statusRX(DISABLE);   // turn off comparator
		mcu_statusTimerInterrupt(DISABLE);
		mcu_resetTimer();

		readReply[DATA_LENGTH_IN_BYTES] = queryReply[0]; // remember to restore correct RN before doing crc()
		readReply[DATA_LENGTH_IN_BYTES+1] = queryReply[1]; // because crc() will shift bits to add
		crc16_ccitt_readReply(DATA_LENGTH_IN_BYTES);    // leading "0" bit.

		// DATA_LENGTH_IN_BYTES*8 bits for data + 16 bits for the handle + 16 bits for
		// the CRC + leading 0 + add one to number of bits for xmit code
		sendToReader(&readReply[0], ((DATA_LENGTH_IN_BYTES*8)+16+16+1+1));
		state = nextState;
		delimiterNotFound = 1;

	#elif SIMPLE_READ_COMMAND

		//mcu_statusRX(DISABLE);   // turn off comparator
		mcu_statusTimerInterrupt(DISABLE);
		mcu_resetTimer();

		#define USE_COUNTER 1
		#if USE_COUNTER
			readReply[0] = mcu_swapBytes(read_counter);
			readReply[1] = read_counter;
		#else
			readReply[0] = 0x03;
			readReply[1] = 0x04;
		#endif
		
		readReply[2] = queryReply[0]; // remember to restore correct RN before doing crc()
		readReply[3] = queryReply[1];        // because crc() will shift bits to add
		crc16_ccitt_readReply(2);    // leading "0" bit.

		// after that sends tagResponse
		// 16 bits for data + 16 bits for the handle + 16 bits for the CRC + leading 0
		// + add one to number of bits for seong's xmit code
		sendToReader(&readReply[0], 50);
		state = nextState;
		delimiterNotFound = 1; // reset
	#endif
}

void handle_nak(volatile short nextState)
{
	mcu_statusTimerInterrupt(DISABLE);
	mcu_resetTimer();
	state = nextState;
}

void do_nothing()
{
	mcu_statusTimerInterrupt(DISABLE);
	mcu_resetTimer();
	//mcu_statusRX(DISABLE);   // turn off comparator
}
