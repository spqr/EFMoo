
#include <mcu.h>
#include <rfid.h>
#include <globals_regvars.h>

/* EXECUTE RUNMODE METHODS */
void executeRunMode();
void setup_to_receive();

/* CHIP STATE HANDLING FUNCTIONS */
void handleTimeout();
void handleReadyState();
void handleArbitrateState();
void handleReplyState();
void handleAcknowledgedState();
void handleOpenState();
void handleReadSensorState();