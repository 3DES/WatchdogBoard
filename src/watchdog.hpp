#if not defined WATCHDOG_H
#define WATCHDOG_H


#include <stdint.h>
#include <stdbool.h>


enum
{
    eWATCHDOG_STATE_INIT,       // WD can be started at any time, eWATCHDOG_VALUE_CLEAR is a valid value in this state!
    eWATCHDOG_STATE_OK,         // WD has been startet, it's not allowed to reach eWATCHDOG_VALUE_CLEAR ever again!
    eWATCHDOG_STATE_ERROR,      // WD reached eWATCHDOG_VALUE_CLEAR after it has been startet, that means ERROR
};


enum
{
    eWATCHDOG_TEST_READBACK = 0,                                // input to be used as watchdog readback
    eWATCHDOG_TEST_REPEAT_TIME = 100UL * 60 * 60 * 1000,        // every 100h the output will be switched off what will be checked by monitoring the readback input
    eWATCHDOG_TEST_TIMEOUT = 10U * 1000,                        // time until readback has to become 1 during initial test / become 0 during repeated test
};


void watchdog_setWatchdog(uint16_t value);
bool watchdog_getWatchdog(void);
bool watchdog_readWatchdog(void);
bool watchdog_resetPortMustBeLocked(void);

void watchdog_selfTest(uint8_t readbackValue);

uint8_t watchdog_getState(void);


#endif

