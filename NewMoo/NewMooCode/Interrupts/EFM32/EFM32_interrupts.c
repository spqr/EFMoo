#include <EFM32_interrupts.h>

//Voltage Comparator Interrupt Request Handler
//Resets interrupts and timers, enters ready state
void VCMP_IRQHandler(){
    //disable all vcmp interrupts
    VCMP_IntClear(_VCMP_IF_MASK);
    VCMP_IntDisable(_VCMP_IEN_MASK);
    
    //disable all port interrupts
    GPIO_IntClear(_GPIO_IF_MASK);
    GPIO_IntDisable(_GPIO_IEN_MASK);
    
    //Restart timer
    mcu_resetTimer();
    mcu_clearTimer();
    
    state = STATE_READY;
    EMU_EnterEM1();
}

//Timer Interrupt Request Handler
//Resets timer when underflows.
void TIMER0_IRQHandler(void){
    //Restart Timer
    mcu_resetTimer();
    mcu_clearTimer();
    EMU_EnterEM1();
    bits = 0;
}

//Port Interrupt Request Handler
//Initiates bit receive
void TIMER1_IRQHandler(void){/*TODO THIS IS NOT TIMER1*/
    /*MSP asm("MOV TAR R7"); //Move TAR to R7*/
    TIMER1->IFC = _TIMER_IF_MASK;
    TIMER1->CNT = 0;
    EMU_EnterEM1();
    
    /*assuming i get timer value into R7*/
    /*and assuming i get bits into R5*/
    /*and assuming i get dest into R4*/
    
    //clear all port interrupts
    GPIO_IntClear(_GPIO_IF_MASK);
    
    //if bits == 0
    asm("CMP R5, #0000h\n"); //1 Cycle
    asm("BEQ bit_is_zero_in_port_int\n"); //1 cycle if not taken, 3 if taken

    asm("MOVS R5, #0000h\n"); //1 cycle

    //if R7 > 0x40
    asm("CMP R7, #0040h\n");
    asm("BHI delimeter_value_is_wrong\n");
    
    //if R7 < 0x10
    asm("CMP R7, #0010h\n");
    asm("BLO delimeter_value_is_wrong\n");
    
    /*
    Disable Port1Interrupt
    Set Timer
    Enable Timer
    Enable Timer Port
    */
    
    
    
    asm("delimeter_value_is_wrong:");
        /*
        P1 Interrupt Edge to Positive
        */
        asm("MOVS R5, #0000h\n");
        delimiterNotFound = 1;
                
    
        
    asm("bit_is_zero_in_port_int:");
        /*
        Timer = 0
        P1 Interrupt Edge to Negative
        */
        asm("ADD R5, R5, #1\n"); //bits++
        
    
}