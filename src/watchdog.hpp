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
};


void watchdog_setWatchdog(uint16_t value);
bool watchdog_trigger(void);
bool watchdog_readWatchdog(void);
bool watchdog_resetPortMustBeLocked(void);

bool watchdog_requestSelfTest(void);
void watchdog_selfTestHandler(uint8_t readbackValue);

uint8_t watchdog_getState(void);


static inline bool watchdog_running(void)
{
    return watchdog_getState() == eWATCHDOG_STATE_OK;
}


#endif

