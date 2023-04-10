#include <stdint.h>
#include "debug.hpp"
#include "watchdog.hpp"
#include "timer.hpp"
#include "errorAndDiagnosis.hpp"


// watchdog (re-)trigger time
#define WATCHDOG_VALUE_CLEAR 0
enum
{
    eWATCHDOG_VALUE_TRIGGER = 60000 / eTICK_TIME,   // eWATCHDOG_VALUE should be ~ 60 seconds, eTICK_TIME value is given in ms units
    eWATCHDOG_VALUE_CLEAR   = WATCHDOG_VALUE_CLEAR, // value to be set in error case (also Pi can set this value, e.g. in case it panics)
};


// reset lock release time after error, this time has to be long enough to ensure that the external battery switch off circuit has definitely switched OFF!
enum
{
    eLOCK_RESET   = 30000 / eTICK_TIME,   // when watchdog switches to error state the reset lock should be hold for several seconds (10000 = 10000ms)!
    eUNLOCK_RESET = 0,                    // reset can be unlocked again
};


enum
{
    eWATCHDOG_TEST_INITIAL,                 // initial test is running, the initial test is different from the repeated one, it only checks that readback is 0
    eWATCHDOG_TEST_REPEATED_EXPECT_ON,      // repeated test is running, the repeated test checks if readback is 1, then switches of the output and waits until the readback becomes 0
    eWATCHDOG_TEST_REPEATED_EXPECT_OFF,     // repeated test is running, the repeated test checks if readback is 1, then switches of the output and waits until the readback becomes 0

    eWATCHDOG_TEST_PASSED,                  // watchdog self test passed (wait until eWATCHDOG_TEST_REPEAT_TIME is over)
    eWATCHDOG_TEST_FAILED,                  // repeated test failed (it's a final state and will never be left again!)
};


static uint16_t watchdog                  = eWATCHDOG_VALUE_CLEAR;          // watchdog counter, during startup it's ok if the value is 0 but whenever it has been started it's not allowed to reach 0 again!
static uint8_t  watchDogState             = eWATCHDOG_STATE_INIT;           // curent watchdog state to decide if action is accepted or ignored
static uint16_t resetLockCounter          = eUNLOCK_RESET;                  // initially the reset port is not locked, when the watchdog is triggered reset port should be locked, when watchdog is cleared again the reset port should stay locked for a while


static bool selfTestConfirmation = false;                       // only if self test sets this to TRUE a selfTestApproval() will result in TRUE and the watchdog output is allowed to be switched ON
static uint16_t waitingTimeout = eWATCHDOG_TEST_TIMEOUT;        // waiting timeout if necessary (initially wait for 0 will last forever)


enum
{
    eSELF_TEST_POLLING = 0,
    eSELF_TEST_TIMEOUT = 1,
    eSELF_TEST_OK      = 2,
};
/**
 * @brief Checks if readback port has expected state
 *
 * @param expectedReadbackState     state the readback port should have
 * @param readbackValue             state the readback port currently has
 * @return eSELF_TEST_POLLING       readback port not as expected but some polling time left
 * @return eSELF_TEST_OK            readback has expected state, polling finished
 * @return eSELF_TEST_TIMEOUT       timeout since readback didn't assume the expected state within time
 */
static uint16_t readBackPortPolling(bool expectedReadbackState, uint8_t readbackValue)
{
    uint16_t result = eSELF_TEST_POLLING;

    // check if expectedReadbackState and readbackValue are identical (both OFF or both ON)
    if ((!readbackValue && !expectedReadbackState) || (readbackValue && expectedReadbackState))
    {
        // readback as expected
        result = eSELF_TEST_OK;
    }
    else
    {
        // give readback some more time to switch to expected state
        if (waitingTimeout)
        {
            // still polling...
            waitingTimeout--;
        }
        else
        {
            // timeout happened
            result = eSELF_TEST_TIMEOUT;
        }
    }

    return result;
}


/**
 * @brief To switch wachdog into error state
 *
 */
static void switchWatchdogIntoErrorState(void)
{
    // stop watch dog even it's already been stopped
    noInterrupts();
    watchDogState = eWATCHDOG_STATE_ERROR;
    watchdog = eWATCHDOG_VALUE_CLEAR;
    interrupts();
    debug_pin2(LOW);

    // in ERROR case lock the reset pin for a while, so external timing relay can be switched OFF and even after a reset the battery cannot be started again without pressing a button manually!
    if (resetLockCounter != eUNLOCK_RESET)
    {
        resetLockCounter--;
    }
}


/**
 * @brief Watchdog self test method has to be called always before watchdog state can be read otherwise watchdog state will always result to FALSE
 *
 * @param readbackValue     current value of readback input
 *
 * @return true     in case test was OK or test state needs watchdog to be switched ON
 * @return false    in case test failed or test state needs watchdog to be switched OFF
 */
void watchdog_selfTest(uint8_t readbackValue)
{
    static uint32_t watchDogTestRemainingTime = 0UL;                // remaining time until next test will be executed (initially immediately when watchdog will be switched on, repeated test after eWATCHDOG_TEST_REPEAT_TIME ms)
    static uint16_t watchDogTestState = eWATCHDOG_TEST_INITIAL;     // initial state after start up

    selfTestConfirmation = false;           // ensure watchdog cannot be switched ON except the following code decides that self test state is OK

    // execute watchdog test only if watchdog is not OFF (when watchdog is startet for the first time after startup it's frozen to zero until initial test has been finished)
    if (watchdog_readWatchdog())
    {
        switch (watchDogTestState)
        {
            // initially ensure watchdog is switched OFF (what means it can be switched off!)
            case eWATCHDOG_TEST_INITIAL:
                // wait for read back becomes OFF
                switch (readBackPortPolling(false, readbackValue))
                {
                    case eSELF_TEST_TIMEOUT:
                        // timeout during initial self test
                        errorAndDiagnosis_setError(eERROR_INITIAL_SELF_TEST_ERROR);
                        watchDogTestState = eWATCHDOG_TEST_FAILED;
                        break;

                    case eSELF_TEST_OK:
                        // initial self test passed
                        errorAndDiagnosis_setExecutedTest(eEXECUTED_TEST_SELF_TEST);
                        selfTestConfirmation = true;            // self test confirms that watchdog output can be switched ON (test was successful)
                        watchDogTestRemainingTime = eWATCHDOG_TEST_REPEAT_TIME;
                        watchDogTestState = eWATCHDOG_TEST_PASSED;
                        break;

                    // ignore the rest
                    default:
                        break;
                }
                break;

            case eWATCHDOG_TEST_REPEATED_EXPECT_ON:
                selfTestConfirmation = true;            // self test confirms that watchdog output can be switched ON (what is the expected state here)
                // wait for read back becomes ON
                switch (readBackPortPolling(true, readbackValue))
                {
                    case eSELF_TEST_TIMEOUT:
                        // timeout during repeated self test, watchdog is not OK, watchdog port is OFF
                        errorAndDiagnosis_setError(eERROR_REPEATED_SELF_TEST_ON_ERROR);
                        watchDogTestState = eWATCHDOG_TEST_FAILED;
                        break;

                    case eSELF_TEST_OK:
                        // first stage of repeated self test passed, watchdog is not in error state and watchdog port is ON!
                        waitingTimeout = eWATCHDOG_TEST_TIMEOUT;                    // reset timeout time again
                        watchDogTestState = eWATCHDOG_TEST_REPEATED_EXPECT_OFF;     // switch to next test state
                        break;

                    // ignore the rest
                    default:
                        break;
                }
                break;

            case eWATCHDOG_TEST_REPEATED_EXPECT_OFF:
                // wait for read back becomes OFF
                switch (readBackPortPolling(true, readbackValue))
                {
                    case eSELF_TEST_TIMEOUT:
                        // timeout during repeated self test, watchdog output couldn't be switched OFF
                        watchDogTestState = eWATCHDOG_TEST_FAILED;
                        errorAndDiagnosis_setError(eERROR_REPEATED_SELF_TEST_OFF_ERROR);
                        break;

                    case eSELF_TEST_OK:
                        // second stage of repeated self test passed, watchdog output could be switched OFF
                        errorAndDiagnosis_setExecutedTest(eEXECUTED_TEST_SELF_TEST);
                        selfTestConfirmation = true;                                // self test confirms that watchdog output can be switched ON (test was successful)
                        watchDogTestRemainingTime = eWATCHDOG_TEST_REPEAT_TIME;     // reset time for next self test (100 hours)
                        watchDogTestState = eWATCHDOG_TEST_PASSED;                  // finish test
                        break;

                    // ignore the rest
                    default:
                        break;
                }
                break;

            // last self test passed so wait for next one
            case eWATCHDOG_TEST_PASSED:
                selfTestConfirmation = true;            // self test confirms that watchdog output can be switched ON (last test was successful and there is still some time until it has to be executed for the next time)
                if (watchDogTestRemainingTime)
                {
                    // still some time left since self test has been executed the last time
                    watchDogTestRemainingTime--;
                }
                else
                {
                    // time since last self test is over, initiate another self test
                    waitingTimeout = eWATCHDOG_TEST_TIMEOUT;                    // reset timeout time again
                    watchDogTestState = eWATCHDOG_TEST_REPEATED_EXPECT_ON;      // switch to next test state
                }
                break;

            // watchdog cannot be switched ON again (since selfTestConfirmation will never become TRUE if we are here), a reset is necessary!
            case eWATCHDOG_TEST_FAILED:
                switchWatchdogIntoErrorState();
                break;
        }
    }
}


/**
 * @brief Check if self test approval is available but approval will be cleared after checked once and has to be requested again by calling watchdog_selfTest()
 *
 * @return true     self test approval is available
 * @return false    self test approval is NOT available
 */
static inline bool selfTestApproval(void)
{
    bool selfTestConfirmationTemp = selfTestConfirmation;
    selfTestConfirmation = false;

    return selfTestConfirmationTemp;
}


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
     *            |   INIT    |     OK     |   ERROR
     * -----------|-----------|------------|-----------
     * value = 0  | value = 0 | value = 0  | ignore
     * -----------|-----------|------------|-----------
     * value != 0 | value = n | value = n  | ignore
     */
    if (watchDogState != eWATCHDOG_STATE_ERROR)
    {
        if (value)
        {
            // set watch dog values
            noInterrupts();
            watchdog  = eWATCHDOG_VALUE_TRIGGER;
            resetLockCounter = eLOCK_RESET;             // lock reset port as soon as watchdog has been started
            watchDogState = eWATCHDOG_STATE_OK;
            interrupts();
            debug_pin2(HIGH);
        }
        else
        {
            // value == 0 is only OK during init phase but afterwards it must be an error detected by someone externally that switches the watchdog OFF now
            if (watchDogState != eWATCHDOG_STATE_INIT)
            {
                switchWatchdogIntoErrorState();         // clearing the watchdog is also a watchdog error and retriggering is not possible anymore

                // watchdog cleared via command
                errorAndDiagnosis_setError(eERROR_WATCHDOG_CLEARED);
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

#if WATCHDOG_VALUE_CLEAR != 0
#   error WATCHDOG_VALUE_CLEAR is expected to be defined as 0 otherwise the following algorithm will not work
#endif

    // decrement until eWATCHDOG_VALUE_CLEAR will be reached
    if (watchdog_readWatchdog())
    {
        noInterrupts();
        watchdog--;       // this expects that watchdog == 0 and watchdog == WATCHDOG_VALUE_CLEAR means the same!
        interrupts();

        // watchdog counter became zero
        if (!watchdog_readWatchdog())
        {
            errorAndDiagnosis_setError(eERROR_WATCHDOG_NOT_TRIGGERED);
            switchWatchdogIntoErrorState();
        }
    }

    // since this method is called periodically do an overall check if watchdog is in ERROR state (defensive programming... yes we want this here!)
    if (!watchdog_readWatchdog() && (watchDogState != eWATCHDOG_STATE_ERROR))
    {
        errorAndDiagnosis_setError(eERROR_WATCHDOG_STOPPED_UNEXPECTEDLY);

        // watchdog became zero during last turn or was set to zero via command
        switchWatchdogIntoErrorState();
    }

    // decide if watchdog port has to be switched ON or OFF (OFF is not always an error since even during self test it has to be switched OFF for a short period of time)
    return selfTestApproval() && watchdog_readWatchdog();
}


/**
 * @brief get current watchdog state
 *
 * @return 0 = watchdog output is OFF (not yet set or still OFF after reset)
 * @return 1 = watchdog output is ON (= ok)
 */
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
bool watchdog_resetPortMustBeLocked(void)
{
    return (resetLockCounter != eUNLOCK_RESET);        // lock until resetLockCounter variable reaches eUNLOCK_RESET value
}

