
#include <moo_runmode.h>

/***************************/
/***** EXECUTE RUNMODE *****/
/***************************/

void executeRunMode(){
    setup_to_receive();
    
    while (1)
	{
        //DO WORK
        if ( mcu_getTimerValue() > 0x256 || delimiterNotFound ){
            handleTimeout();
			setup_to_receive();
        }
		
		switch(state)
		{
			case STATE_READY:
			{
				handleReadyState();
				break;
			}
			case STATE_ARBITRATE:
			{
				handleArbitrateState();
				break;
			}
			case STATE_REPLY:
			{
				handleReplyState();
				break;
			}
			case STATE_ACKNOWLEDGED:
			{
				handleAcknowledgedState();
				break;
			}
			case STATE_OPEN:
			{
				handleOpenState();
				break;
			}
			case STATE_READ_SENSOR:
			{
                                /*Sensors Not Implemented in Version*/
				//handleReadSensorState();
				break;
			}
		} //end switch
    }//end while
}



//************************** SETUP TO RECEIVE  *********************************
// note: port interrupt can also reset, but it doesn't call this function
//      because function call causes PUSH instructions prior to bit read
//       at beginning of interrupt, which screws up timing.  so, remember
//       to change things in both places.
void setup_to_receive() {
    //Disable interrupts as we setup to receive
    mcu_statusGlobalInterrupt(DISABLE);
    //Enable the RX pin so we know when to wake up
    mcu_statusRX(ENABLE);
    //Redefine timer behavior for receive state. It has to setup because there is no setup time after done with port1 interrupt.
    mcu_setReceiveTimers();
    //Redefine allowed interrupts for receive state
    mcu_setReceiveInterrupts();
    //Set the chip's run environment to the low power with interrupts
    mcu_enterLowPowerState();
}

/*
 * Logic to handle Timeouts
 */
void handleTimeout(){
    if( !mcu_powerIsGood() ){
        mcu_sleep();
    }
    #if SENSOR_DATA_IN_ID || SENSOR_DATA_IN_READ_COMMAND
        if ( timeToSample++ == 10 ) {
            state = STATE_READ_SENSOR;
            timeToSample = 0;
        }
    #else
        inInventoryRound = 0;
        state = STATE_READY;
    #endif

	#if ENABLE_SESSIONS
		handle_session_timeout();
	#endif

	#if ENABLE_SLOTS
	    if (shift < 4)
	        shift += 1;
	    else
	        shift = 0;
	#endif
}

/*
 * Logic to handle Ready State
 */
void handleReadyState(){
	inInventoryRound = 0;

	// process the QUERY command
	if ( bits == NUM_QUERY_BITS  && ( ( cmd[0] & 0xF0 ) == 0x80 ) )
	{
		handle_query(STATE_REPLY);
		setup_to_receive();
	}

	// process the SELECT command
	// @ short distance has slight impact on performance
	else if ( bits >= 44  && ( ( cmd[0] & 0xF0 ) == 0xA0 ) )
	{
		handle_select(STATE_READY);
		delimiterNotFound = 1;
	} 

	// got >= 22 bits, and it's not the beginning of a select. just reset.
	else if ( bits >= MAX_NUM_QUERY_BITS && ( ( cmd[0] & 0xF0 ) != 0xA0 ) )
	{
		do_nothing();
		state = STATE_READY;
		delimiterNotFound = 1;
	}
}

/*
 * Logic to handle Arbitrate State
 */
void handleArbitrateState(){
	// process the QUERY command
	if ( bits == NUM_QUERY_BITS  && ( ( cmd[0] & 0xF0 ) == 0x80 ) )
	{
		handle_query(STATE_REPLY);
		setup_to_receive();
	}
	
	// got >= 22 bits, and it's not the beginning of a select. just reset.
	else if ( bits >= MAX_NUM_QUERY_BITS && ( ( cmd[0] & 0xF0 ) != 0xA0 ) )
	{
		do_nothing();
		state = STATE_READY;
		delimiterNotFound = 1;
	}
	
	// this state handles query, queryrep, queryadjust, and select commands.
	// process the QUERYREP command
	else if ( bits == NUM_QUERYREP_BITS && ( ( cmd[0] & 0x06 ) == 0x00 ) )
	{
		handle_queryrep(STATE_REPLY);
		delimiterNotFound = 1;
	}
	
	// process the QUERYADJUST command
	else if ( bits == NUM_QUERYADJ_BITS  && ( ( cmd[0] & 0xF8 ) == 0x48 ) )
	{
		handle_queryadjust(STATE_REPLY);
		setup_to_receive();
	}
	
	// process the SELECT command
	// @ short distance has slight impact on performance
	else if ( bits >= 44  && ( ( cmd[0] & 0xF0 ) == 0xA0 ) )
	{
		handle_select(STATE_READY);
		delimiterNotFound = 1;
	}
}

/*
 * Logic to handle Reply State
 */
void handleReplyState(){
	// this state handles query, query adjust, ack, and select commands

	// process the ACK command
	if ( bits == NUM_ACK_BITS  && ( ( cmd[0] & 0xC0 ) == 0x40 ) )
	{
		#if ENABLE_READS
			handle_ack(STATE_ACKNOWLEDGED);
			setup_to_receive();
		#elif SENSOR_DATA_IN_ID
			handle_ack(STATE_ACKNOWLEDGED);
			delimiterNotFound = 1; // reset
		#else
			// this branch for hardcoded query/acks
			handle_ack(STATE_ACKNOWLEDGED);
			setup_to_receive();
		#endif
	}

	// process the QUERY command
	if ( bits == NUM_QUERY_BITS  && ( ( cmd[0] & 0xF0 ) == 0x80 ) )
	{
		// i'm supposed to stay in state_reply when I get this, but if I'm
		// running close to 1.8v then I really need to reset and get in the
		// sleep, which puts me back into state_arbitrate. this is complete
		// a violation of the protocol, but it sure does make everything
		// work better. - polly 8/9/2008
		handle_query(STATE_REPLY);
		setup_to_receive();
	}


	// process the QUERYREP command
	else if ( bits == NUM_QUERYREP_BITS && ( ( cmd[0] & 0x06 ) == 0x00 ) )
	{
		do_nothing();
		state = STATE_ARBITRATE;
		setup_to_receive();
	}

	// process the QUERYADJUST command
	else if ( bits == NUM_QUERYADJ_BITS  && ( ( cmd[0] & 0xF8 ) == 0x48 ) )
	{
		handle_queryadjust(STATE_REPLY);
		delimiterNotFound = 1;
	}

	// process the SELECT command
	else if ( bits >= 44  && ( ( cmd[0] & 0xF0 ) == 0xA0 ) )
	{
		handle_select(STATE_READY);
		delimiterNotFound = 1;
	}
	
	else if ( bits >= MAX_NUM_QUERY_BITS && ( ( cmd[0] & 0xF0 ) != 0xA0 ) && ( ( cmd[0] & 0xF0 ) != 0x80 ) )
	{
		do_nothing();
		state = STATE_READY;
		delimiterNotFound = 1;
	}
}

/*
 * Logic to handle Acknowledged State
 */
void handleAcknowledgedState(){
	// responds to query, ack, request_rn cmds
	// takes action on queryrep, queryadjust, and select cmds
	
	// process the REQUEST_RN command
	if ( bits >= NUM_REQRN_BITS && ( cmd[0] == 0xC1 ) )
	{
		handle_request_rn(STATE_OPEN);
		setup_to_receive();
	}


	// process the QUERY command
	if ( bits == NUM_QUERY_BITS  && ( ( cmd[0] & 0xF0 ) == 0x80 ) )
	{
	handle_query(STATE_REPLY);
	delimiterNotFound = 1;
	}

	// process the ACK command
	// this code doesn't seem to get exercised in the real world. if i ever
	// ran into a reader that generated an ack in an acknowledged state,
	// this code might need some work.
	else if ( bits == NUM_ACK_BITS  && ( ( cmd[0] & 0xC0 ) == 0x40 ) )
	{
		handle_ack(STATE_ACKNOWLEDGED);
		setup_to_receive();
	}

	// process the QUERYREP command
	else if ( bits == NUM_QUERYREP_BITS && ( ( cmd[0] & 0x06 ) == 0x00 ) )
	{
		// in the acknowledged state, rfid chips don't respond to queryrep
		// commands
		do_nothing();
		state = STATE_READY;
		delimiterNotFound = 1;
	} 

	// process the QUERYADJUST command
	else if ( bits == NUM_QUERYADJ_BITS  && ( ( cmd[0] & 0xF8 ) == 0x48 ) )
	{
		do_nothing();
		state = STATE_READY;
		delimiterNotFound = 1;
	} // queryadjust command

	// process the SELECT command
	else if ( bits >= 44  && ( ( cmd[0] & 0xF0 ) == 0xA0 ) )
	{
		handle_select(STATE_READY);
		delimiterNotFound = 1;
	}

	// process the NAK command
	else if ( bits >= 10 && ( cmd[0] == 0xC0 ) )
	{
		do_nothing();
		state = STATE_ARBITRATE;
		delimiterNotFound = 1;
	}

	// process the READ command
	// warning: won't work for read addrs > 127d
	if ( bits == NUM_READ_BITS && ( cmd[0] == 0xC2 ) )
	{
		handle_read(STATE_ARBITRATE);
		state = STATE_ARBITRATE;
		delimiterNotFound = 1;
	}
	
	// FIXME: need write, kill, lock, blockwrite, blockerase
	// process the ACCESS command
	if ( bits >= 56  && ( cmd[0] == 0xC6 ) )
	{
		do_nothing();
		state = STATE_ARBITRATE;
		delimiterNotFound = 1;
	}

	else if ( bits >= MAX_NUM_READ_BITS )
	{
		state = STATE_ARBITRATE;
		delimiterNotFound = 1;
	}
}

/*
 * Logic to handle Open State
 */
void handleOpenState(){
	// responds to query, ack, req_rn, read, write, kill, access,
	// blockwrite, and blockerase cmds
	// processes queryrep, queryadjust, select cmds

	// process the READ command
	// warning: won't work for read addrs > 127d
	if ( bits == NUM_READ_BITS  && ( cmd[0] == 0xC2 ) )
	{
		handle_read(STATE_OPEN);
		// note: setup_to_receive() et al handled in handle_read
	}

	// process the REQUEST_RN command
	else if ( bits >= NUM_REQRN_BITS  && ( cmd[0] == 0xC1 ) )
	{
		handle_request_rn(STATE_OPEN);
		setup_to_receive();
	}

	// process the QUERY command
	if ( bits == NUM_QUERY_BITS  && ( ( cmd[0] & 0xF0 ) == 0x80 ) )
	{
		handle_query(STATE_REPLY);
		delimiterNotFound = 1;
	}

	// process the QUERYREP command
	else if ( bits == NUM_QUERYREP_BITS && ( ( cmd[0] & 0x06 ) == 0x00 ) )
	{
		do_nothing();
		state = STATE_READY;
		setup_to_receive();
	}

	// process the QUERYADJUST command
	else if ( bits == 9  && ( ( cmd[0] & 0xF8 ) == 0x48 ) )
	{
		do_nothing();
		state = STATE_READY;
		delimiterNotFound = 1;
	}
	
	// process the ACK command
	else if ( bits == NUM_ACK_BITS  && ( ( cmd[0] & 0xC0 ) == 0x40 ) )
	{
		handle_ack(STATE_OPEN);
		delimiterNotFound = 1;
	}

	// process the SELECT command
	else if ( bits >= 44  && ( ( cmd[0] & 0xF0 ) == 0xA0 ) )
	{
		handle_select(STATE_READY);
		delimiterNotFound = 1;
	}

	// process the NAK command
	else if ( bits >= 10 && ( cmd[0] == 0xC0 ) )
	{
		handle_nak(STATE_ARBITRATE);
		delimiterNotFound = 1;
	}
}

/*
 * Logic to handle Read Sensor State
 */
void handleReadSensorState(){
	#if SENSOR_DATA_IN_READ_COMMAND
		read_sensor(&readReply[0]);
		// crc is computed in the read state
		mcu_initReceiveClock();
		state = STATE_READY;
		delimiterNotFound = 1; // reset
	#elif SENSOR_DATA_IN_ID
		read_sensor(&ackReply[3]);
		mcu_initReceiveClock();
		ackReplyCRC = crc16_ccitt(&ackReply[0], 14);
		ackReply[15] = (unsigned char) ackReplyCRC;
		ackReply[14] = (unsigned char) mcu_swapBytes(ackReplyCRC);
		state = STATE_READY;
		delimiterNotFound = 1; // reset
	#endif
}




