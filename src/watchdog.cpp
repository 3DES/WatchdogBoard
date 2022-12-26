#include <stdint.h>
#include "debug.hpp"
#include "watchdog.hpp"
#include "timer.hpp"


enum
{
    eWATCHDOG_VALUE_TRIGGER = 60000 / eTICK_TIME,   // eWATCHDOG_VALUE should be ~ 60 seconds, eTICK_TIME value is given in ms units
    eWATCHDOG_VALUE_CLEAR   = 0,                    // value to be set in error case (also Pi can set this value, e.g. in case it panics)
};


enum
{
    eLOCK_RESET   = 10000 / eTICK_TIME,   // when watchdog switches to error state the reset lock should be hold for several seconds (10000 = 10000ms)!
    eUNLOCK_RESET = 0,                    // reset can be unlocked again
};


static uint16_t watchdog     = eWATCHDOG_VALUE_CLEAR;   // watchdog counter, during startup it's ok if the value is 0 but whenever it has been started it's not allowed to reach 0 again!
static uint8_t watchDogState = eWATCHDOG_STATE_INIT;    // curent watchdog state to decide if action is accepted or ignored
static uint16_t lockReset    = eUNLOCK_RESET;           // initially the reset port is not locked, when the watchdog is triggered reset port should be locked, when watchdog is cleared again the reset port should stay locked for a while


/**
 * @brief Set watchdog state
 *
 * @param value != 0    triggers watchdog if watchdog is not yet in ERROR state
 * @param value == 0    stops watchdog and switches watchdog into ERROR state
 */
void watchdog_setWatchdog(uint16_t value)
{
    /*
     * Watchdog states and state changes:
     *
     *            |   INIT    ||     OK    |    ERROR
     * -----------|-----------||-----------|------------
     * value == 0 | value = 0 || value = 0 |  value = 0
     *            | -> INIT   || -> ERROR  |  -> ERROR
     * -----------|-----------|============||-----------
     * value != 0 | value = n |  value = n || value = 0
     *            | -> OK     |  -> OK     || -> ERROR
     */
    if ((watchDogState != eWATCHDOG_STATE_ERROR) && (value != eWATCHDOG_VALUE_CLEAR))
    {
        // set watch dog values
        noInterrupts();
        watchdog  = eWATCHDOG_VALUE_TRIGGER;
        lockReset = eLOCK_RESET;
        watchDogState = eWATCHDOG_STATE_OK;
        interrupts();
        debug_pin2(LOW);
    }
    else
    {
        // only in case of eWATCHDOG_STATE_INIT a value of eWATCHDOG_VALUE_CLEAR is OK, since that's usual during startup while no other value has been set!
        if (watchDogState != eWATCHDOG_STATE_INIT)
        {
            // stop watch dog even it's already been stopped
            noInterrupts();
            watchDogState = eWATCHDOG_STATE_ERROR;
            watchdog = eWATCHDOG_VALUE_CLEAR;
            interrupts();
            debug_pin2(HIGH);


            // in ERROR case lock the reset pin for a while, so external timing relay can be switched OFF and even after a reset the battery cannot be started again without pressing a button manually!
            if (lockReset != eUNLOCK_RESET)
            {
                lockReset--;
            }
        }
    }
}


/**
 * @brief read current watch dog state
 *
 * @return true     watchdog is running
 * @return false    watchdog is not running, watchdog can be in ERROR or in INIT state
 */
bool watchdog_readWatchdog(void)
{
    return (watchdog != eWATCHDOG_VALUE_CLEAR);
}


/**
 * @brief get watchdog state, each get call decrements the watchdog, has to be called cyclically!
 *
 * @return true     watchdog is still running, eth. is OK, watchdog relay should be switched ON
 * @return false    watchdog has stopped, this means ERROR, watchdog relay has to be switched OFF immediately
 */
bool watchdog_getWatchdog(void)
{
    // decrement until eWATCHDOG_VALUE_CLEAR will be reached
    if (watchdog > eWATCHDOG_VALUE_CLEAR + 1)
    {
        noInterrupts();
        watchdog--;
        interrupts();
    }
    else
    {
        // set eWATCHDOG_VALUE_CLEAR state and let watchdog_setWatchdog() decide if that's an error of if it's ok
        watchdog_setWatchdog(eWATCHDOG_VALUE_CLEAR);
    }

    return watchdog_readWatchdog();
}


uint8_t watchdog_getState(void)
{
    return watchDogState;
}

/**
 * @brief To check if reset port has to be locked or not
 *
 * @return true     reset port has to be locked, no reset is allowed
 * @return false    reset port can be unlocked, reset is allowed
 */
bool watchdog_lockResetPort(void)
{
    return (lockReset != eUNLOCK_RESET);        // lock until lockReset variable reaches eUNLOCK_RESET value
}

