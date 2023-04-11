#if not defined ERROR_AND_DIAGNOSIS_H
#define ERROR_AND_DIAGNOSIS_H


#include <stdint.h>
#include "debug.hpp"


enum
{
    eERROR_NONE = 0,

    eERROR_INITIAL_SELF_TEST_ERROR           = 0x0001,   // error number in case of self test error during self test initial phase
    eERROR_REPEATED_SELF_TEST_ON_ERROR       = 0x0002,   // error number in case of self test error while self test has been repeated
    eERROR_REPEATED_SELF_TEST_OFF_ERROR      = 0x0003,   // error number in case of self test error while self test has been repeated
    eERROR_REPEATED_SELF_TEST_REQUEST_MISSED = 0x0004,   // error number in case of self test has not been requested early enough

    eERROR_WATCHDOG_NOT_TRIGGERED            = 0x1000,   // watchdog was already running but it was not triggered anymore
    eERROR_WATCHDOG_CLEARED                  = 0x1001,   // watchdog was already running and has been cleared via command
    eERROR_WATCHDOG_STOPPED_UNEXPECTEDLY     = 0x1002,   // watchdog was already running but now it has been stopped but is not in ERROR state
};


enum
{
    eDIAGNOSIS_NONE = 0,

    eDIAGNOSIS_STARTUP = 1 << 0,

    eDIAGNOSIS_RESERVED1 = 1 << 1,
    eDIAGNOSIS_RESERVED2 = 1 << 2,
    eDIAGNOSIS_RESERVED3 = 1 << 3,
    eDIAGNOSIS_RESERVED4 = 1 << 4,
    eDIAGNOSIS_RESERVED5 = 1 << 5,
    eDIAGNOSIS_RESERVED6 = 1 << 6,
    eDIAGNOSIS_RESERVED7 = 1 << 7,
    eDIAGNOSIS_RESERVED8 = 1 << 8,
    eDIAGNOSIS_RESERVED9 = 1 << 9,
    eDIAGNOSIS_RESERVED10 = 1 << 10,
    eDIAGNOSIS_RESERVED11 = 1 << 11,
    eDIAGNOSIS_RESERVED12 = 1 << 12,
    eDIAGNOSIS_RESERVED13 = 1 << 13,
    eDIAGNOSIS_RESERVED14 = 1 << 14,
    eDIAGNOSIS_RESERVED15 = 1 << 15,

    eDIAGNOSIS_INIT = eDIAGNOSIS_STARTUP,
};


enum
{
    eEXECUTED_TEST_NONE      = 0,

    eEXECUTED_TEST_SELF_TEST = 1 << 0,     // lowest bit is self test indicator

    eEXECUTED_TEST_RESERVED1 = 1 << 1,
    eEXECUTED_TEST_RESERVED2 = 1 << 2,
    eEXECUTED_TEST_RESERVED3 = 1 << 3,
    eEXECUTED_TEST_RESERVED4 = 1 << 4,
    eEXECUTED_TEST_RESERVED5 = 1 << 5,
    eEXECUTED_TEST_RESERVED6 = 1 << 6,
    eEXECUTED_TEST_RESERVED7 = 1 << 7,
    eEXECUTED_TEST_RESERVED8 = 1 << 8,
    eEXECUTED_TEST_RESERVED9 = 1 << 9,
    eEXECUTED_TEST_RESERVED10 = 1 << 10,
    eEXECUTED_TEST_RESERVED11 = 1 << 11,
    eEXECUTED_TEST_RESERVED12 = 1 << 12,
    eEXECUTED_TEST_RESERVED13 = 1 << 13,
    eEXECUTED_TEST_RESERVED14 = 1 << 14,
    eEXECUTED_TEST_RESERVED15 = 1 << 15,
};


void     errorAndDiagnosis_setError(uint16_t errorNumber);
void     errorAndDiagnosis_setDiagnoses(uint16_t diagnosesMask);
void     errorAndDiagnosis_setExecutedTest(uint16_t executedTest);
uint16_t errorAndDiagnosis_getErrorNumber(void);
uint16_t errorAndDiagnosis_getDiagnoses(void);
uint16_t errorAndDiagnosis_getExecutedTests(void);


#endif
