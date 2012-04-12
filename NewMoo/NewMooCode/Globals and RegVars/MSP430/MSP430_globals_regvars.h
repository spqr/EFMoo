#ifndef GLOBAL_REGVARS
#define GLOBAL_REGVARS

#pragma data_alignment=2

/*******GLOBALS*******/
extern unsigned short TRcal;
extern int i;
extern volatile unsigned char* destorig; // pointer to beginning of cmd


/*******REGVARS*******/

// compiler uses working register 4 as a global variable
// Pointer to &cmd[bits]
volatile __no_init __regvar unsigned char* dest @ 4;

// compiler uses working register 5 as a global variable
// count of bits received from reader
volatile __no_init __regvar unsigned short bits @ 5;

#endif