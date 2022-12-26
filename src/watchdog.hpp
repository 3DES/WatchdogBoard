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


void watchdog_setWatchdog(uint16_t value);
bool watchdog_getWatchdog(void);
bool watchdog_readWatchdog(void);
bool watchdog_lockResetPort(void);
uint8_t watchdog_getState(void);


#endif

