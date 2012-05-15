#include <EFM32_mcu.h>


/******PROCESSES******/
void mcu_clearTimer(){
    TIMER_Reset(TIMER0);
    TIMER_Reset(TIMER1);
}

void mcu_enterLowPowerState(){
    mcu_statusGlobalInterrupt(ENABLE);
    EMU_EnterEM1();
}

void mcu_init(){
    //Standard chip initializtion
    CHIP_Init();
    
    //Disable Watchdog
    WDOG_Enable(false);
    
    //Set which clocks we'll use
    CMU_ClockSelectSet(cmuClock_HF, cmuSelect_HFRCO);
    CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFXO);
    
    //Enable Clocks
    CMU_ClockEnable(cmuClock_TIMER0, true);
    CMU_ClockEnable(cmuClock_GPIO, true);
    CMU_ClockEnable(cmuClock_VCMP, true);
    
    // Enable HFRCO with waiting for HFRCORDY
    CMU_OscillatorEnable(cmuOsc_HFRCO, true, true);
    
    //Start Voltage Comparator VCMP
        const VCMP_Init_TypeDef vcmpinit =
    {
        .halfBias = true,               // Half bias current
        .biasProg = 0,                  // Bias current configuration
        .irqFalling = true,             // Enable interrupt for falling edge
        .irqRising = true,              // Enable interrupt for rising edge
        .warmup = vcmpWarmTime4Cycles,  // Warm-up time in clock cycles
        .hyst = vcmpHystNone,           // Hysteresis configuration
        .inactive = 0,                  // Inactive comparator output value
        .lowPowerRef = false,           // Enable low power mode
        .triggerLevel = TRIGGER_LEVEL,  // Trigger level
        .enable = true                  // Enable VCMP after configuration
    };
    VCMP_Init(&vcmpinit);
    
    //TIMER0 CC setup
/* Select CC channel parameters */
    const TIMER_InitCC_TypeDef timer0CCInit = 
    {
        .cufoa      = timerOutputActionNone,    //Control Underflow Output Action
        .cofoa      = timerOutputActionToggle,  //Control Overflow Output Action
        .cmoa       = timerOutputActionNone,    //Counter Match Output Action
        .mode       = timerCCModeCompare,       //Compare/Capture Channel Mode
        .filter     = true,                     //Digital Filter Enable (true) or Disable (false)
        .prsInput   = false,                    //Select TimernCCx (false) or PRS input (true)
        .coist      = false,                    //Compare Output Initial State high (true) or low (false)
        .outInvert  = false,                    //Invert output from compare/capture channel
    };
    TIMER_InitCC(TIMER0, 0, &timer0CCInit); //CC Settings Init
    TIMER0->ROUTE |= (TIMER_ROUTE_CC0PEN | TIMER_ROUTE_LOCATION_LOC3); //Set route for output
    
    //TIMER1 CC setup
        const TIMER_InitCC_TypeDef timer1CCInit = 
    {
        .eventCtrl  = timerEventEveryEdge,
        .edge       = timerEdgeRising,
        .prsSel     = timerPRSSELCh0,
        .cufoa      = timerOutputActionNone,
        .cofoa      = timerOutputActionNone,
        .cmoa       = timerOutputActionNone,
        .mode       = timerCCModeCapture,
        .filter     = true,
        .prsInput   = true,
        .coist      = false,
        .outInvert  = false,
    };
    TIMER_InitCC(TIMER1, 0, &timer1CCInit); //CC Settings Init
    TIMER1->ROUTE |= (TIMER_ROUTE_CC1PEN | TIMER_ROUTE_LOCATION_LOC0); //Set route for output
    
    // Enable TIMER interrupt vectors in NVIC
    NVIC_EnableIRQ(TIMER0_IRQn);
    NVIC_EnableIRQ(TIMER1_IRQn);

        return;
}

void mcu_initPins(){
    GPIO_PinModeSet(CAP_SENSE_PORT, CAP_SENSE_PIN, gpioModePushPull, 0);
    GPIO_PinModeSet(RX_PORT, RX_PIN, gpioModeInputPull, 0);
    GPIO_PinModeSet(RX_EN_PORT, RX_EN_PIN, gpioModePushPull, 0);
    GPIO_PinModeSet(TX_PORT, TX_PIN, gpioModePushPull, 0);
}

void mcu_initReceiveClock(){
    /* Set HFRCO band back to 14 Mhz*/
    CMU_HFRCOBandSet(cmuHFRCOBand_14MHz);
    /* Prescale the HFPERCLK -> HF/2 = 14/2 = 7Mhz */
    CMU_ClockDivSet(cmuClock_HFPER, cmuClkDiv_2);
}

void mcu_initSendClock(){
    /* Set HFRCO band back to 7 Mhz*/
    CMU_HFRCOBandSet(cmuHFRCOBand_7MHz);
    /* Prescale the HFPERCLK -> HF/2 = 7/2 = 3.5Mhz */
    CMU_ClockDivSet(cmuClock_HFPER, cmuClkDiv_2);
}

void mcu_resetTimer(){
    TIMER_CounterSet(TIMER0, 0);
    TIMER_CounterSet(TIMER1, 0);
}

void mcu_setReceiveTimers(){
    //Reset timers as if it went through a hardware reset
    TIMER_Reset(TIMER0);
    TIMER_Reset(TIMER1);
    //timer interrupts when underflows from 0xFFFF to 0x0000 or when CC1 is triggered
    TIMER_IntEnable(TIMER1, TIMER_IF_UF | TIMER_IF_CC1);
    //start timer1
    /* Declare Timer Init struct */
    const TIMER_Init_TypeDef timerInit =
    {
        .enable     = true,                  //Start counting when init completed
        .debugRun   = true,                  //Counter shall keep running during debug halt
        .prescale   = timerPrescale1024,     //Prescaling factor, if HFPER clock used
        .clkSel     = timerClkSelHFPerClk,   //Clock selection
        .fallAction = timerInputActionNone,  //Action on falling input edge
        .riseAction = timerInputActionNone,  //Action on rising input edge
        .mode       = timerModeUp,           //Counting mode
        .dmaClrAct  = false,                 //DMA request clear on action
        .quadModeX4 = false,                 //Select X2 or X4 quadrature decode mode (if used).
        .oneShot    = false,                 //Determines if only counting up or down once
        .sync       = false,                 //Timer start/stop/reload by other timers
    };
    TIMER_Init(TIMER1, &timerInit);
    
    return;
}

void mcu_setReceiveInterrupts(){
    //RX Interrupt on Rising Edge (true) but not Falling (false). Enable (true)
    GPIO_IntConfig(RX_PORT, RX_PIN, true, false, true);
    return;
}

void mcu_setSendTimers(){
    //Reset timers as if it went through a hardware reset
    TIMER_Reset(TIMER0);
    TIMER_Reset(TIMER1);
    //timer interrupts when underflows from 0xFFFF to 0x0000 or when CC1 is triggered
    TIMER_IntEnable(TIMER0, TIMER_IF_UF | TIMER_IF_CC0);
    //start timer0
    /* Declare Timer Init struct */
    const TIMER_Init_TypeDef timerInit =
    {
        .enable     = true,                  //Start counting when init completed
        .debugRun   = true,                  //Counter shall keep running during debug halt
        .prescale   = timerPrescale1024,     //Prescaling factor, if HFPER clock used
        .clkSel     = timerClkSelHFPerClk,   //Clock selection
        .fallAction = timerInputActionNone,  //Action on falling input edge
        .riseAction = timerInputActionNone,  //Action on rising input edge
        .mode       = timerModeUp,           //Counting mode
        .dmaClrAct  = false,                 //DMA request clear on action
        .quadModeX4 = false,                 //Select X2 or X4 quadrature decode mode (if used).
        .oneShot    = false,                 //Determines if only counting up or down once
        .sync       = false,                 //Timer start/stop/reload by other timers
    };
    TIMER_Init(TIMER0, &timerInit);

    return;
}

void mcu_sleep(){
    mcu_statusRX(DISABLE);
    
    mcu_clearTimer();

    mcu_statusGlobalInterrupt(DISABLE); // temporarily disable GIE so we can sleep and enable interrupts at the same time
    VCMP_IntEnable(VCMP_IEN_EDGE); //Enable Voltage Comparator Interrupt 
    
    //Throw interrupt if power happens to be good before going into low power mode
    if (mcu_powerIsGood())
        VCMP_IntSet(VCMP_IF_EDGE);

    //Enter low power mode. Also renables interrupts.
    mcu_enterLowPowerState();
}

void mcu_statusGlobalInterrupt(char set){
    if (set == ENABLE){
        //enable TIMER interrupts
        TIMER_IntEnable(TIMER1, TIMER_IF_UF);
        //enable gpio interrupts
        GPIO_IntEnable(_GPIO_IEN_MASK);
    } else if (set == DISABLE) {
        //disable all timer interrupts
        TIMER_IntDisable(TIMER0, _TIMER_IEN_MASK);
        TIMER_IntDisable(TIMER1, _TIMER_IEN_MASK);
        //disable all port interrupts
        GPIO_IntDisable(_GPIO_IEN_MASK);
    }
}

void mcu_statusRX(char set){
    if (set == ENABLE){
        //enable RX Pin
        GPIO_PinModeSet(RX_PORT, RX_PIN, gpioModeInputPull, 0);
    } else if (set == DISABLE) {
        //disable RX Pin
        GPIO_PinModeSet(RX_PORT, RX_PIN, gpioModeDisabled, 0);
    }
}

void mcu_statusSendTimer(char set){
    if (set == ENABLE){
        mcu_setSendTimers();
    } else {
        //disable TIMER0
        TIMER_IntClear(TIMER0, TIMER_IF_UF | TIMER_IF_CC0);
        TIMER_IntDisable(TIMER0, TIMER_IF_UF | TIMER_IF_CC0);
    }
}

void mcu_statusTimerInterrupt(char set){
    if (set == ENABLE){
        //enable timer interrupts
        TIMER_IntEnable(TIMER1, TIMER_IF_UF);
    } else if (set == DISABLE) {
        //disable all timer interrupts
        TIMER_IntDisable(TIMER1, _TIMER_IEN_MASK);
    }
}

/******FUNCTIONS******/
inline void crc16_ccitt_readReply(unsigned int numDataBytes)
{
    char lsb = 0, carry = 0;
    int i;
    // shift everything over by 1 to accomodate leading "0" bit.
    // first, grab address of beginning of array
    readReply[numDataBytes + 2] = 0; // clear out this spot for the loner bit of handle
    readReply[numDataBytes + 4] = 0; // clear out this spot for the loner bit of crc
    // shift all bytes and later use only data + handle
    for (i = 0; i < 19; i++){
        lsb = readReply[i] & 1;
        readReply[i] >>= 1;
        readReply[i] |= (carry << 16);
        carry = lsb;
    }
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
    return (int) VCMP_VDDHigher();
}

int mcu_getTimerValue(){
    return TIMER_CounterGet(TIMER1);
}

/******TOOLS******/
unsigned short mcu_swapBytes(unsigned short val){
    return __REV(val);
}

/****Might work, much easier to understand****/
void mcu_sendToReader(volatile unsigned char *data, unsigned char numOfBits) {
    unsigned short dataToSend[200][8]; //Chose 200 because it's larger than any numOfBits being sent to function.
    char preamble[6] = {0, 1, 0, 1, 1, 1};
    int preambleLength = 6;
    char dataChar;
        //Need to configure these values with clock speed
    int data0time = 7;  //1us @ 7MHz
    int data1time = 14; //2us @ 7MHz
    int i, j;
    
    mcu_resetTimer();
    
    /*Preprocess Databits*/
    for (i = 0; i < numOfBits; i++){
        dataChar = mcu_swapBytes(data[i]);
        for (j = 7; j > 0; j--){
            dataToSend[i][j] = (dataChar >> j) | 1;
        }
    }

    //Start Send Clock and Timer
    mcu_initSendClock();
    mcu_statusSendTimer(ENABLE);
    
    /*Send Preamble*/
    for (i = 0; i < preambleLength; i++){
        TIMER_CounterSet(TIMER0, 0);
        if (preamble[i] == 0){
            //Send 0 bit
            TIMER_TopSet(TIMER0, data0time);
            __WFI();
            TIMER_CounterSet(TIMER0, data0time);
            __WFI();
        } else {
            //Send 1 bit
            TIMER_TopSet(TIMER0, data1time);
            __WFI();
        }
    }
    
    /*Send Data*/
    for (i = 0; i < numOfBits; i++){
        for (j = 7; j > 0; j--){
                        TIMER_CounterSet(TIMER0, 0);
            if (dataToSend[i][j] == 0){
                //Send 0 bit
                TIMER_TopSet(TIMER0, data0time);
                __WFI();
                TIMER_CounterSet(TIMER0, data0time);
                __WFI();
            } else {
                //Send 1 bit
                TIMER_TopSet(TIMER0, data1time);
                __WFI();
            }
        }
    }
    
    //Turn Off Send Timer
    mcu_statusSendTimer(DISABLE);
    
    //Start Receive Clock
    mcu_initReceiveClock();
}

