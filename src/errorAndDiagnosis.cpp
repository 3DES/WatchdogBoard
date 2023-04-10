#include "errorAndDiagnosis.hpp"


static uint16_t errorNumber = eERROR_NONE;
static uint16_t diagnoses   = eDIAGNOSIS_INIT;
static uint16_t executedTests = eEXECUTED_TEST_NONE;


/**
 * @brief Sets new error value if there is none stored currently (only first error will be stored since usually that's the important one!)
 * A getErrorNumber() call clears the last error again and a new one can be stored
 *
 * @param newError          error number to be set if there is none stored so far
 */
void errorAndDiagnosis_setError(uint16_t newError)
{
    // only remember first error since that's the most important one
    if (errorNumber == eERROR_NONE)
    {
        errorNumber = newError;
    }
}


/**
 * @brief Sets diagnosis only without setting any error value
 * A getDiagnoses() call clears all the diagnoses and new ones can be set
 *
 * @param diagnosesMask     diagoses bits that have to be set
 */
void errorAndDiagnosis_setDiagnoses(uint16_t diagnosesMask)
{
    diagnoses |= diagnosesMask;
}


/**
 * @brief Sets executed test flag
 * A errorAndDiagnosis_getExecutedTests() call clears all the stored test executions and new ones can be set
 *
 * @param executedTest  flag for executed test
 */
void errorAndDiagnosis_setExecutedTest(uint16_t executedTest)
{
    executedTests |= executedTest;
}


/**
 * @brief Get the currently stored error number, stored error will be cleared afterwards
 *
 * @return currently stored error number
 */
uint16_t errorAndDiagnosis_getErrorNumber(void)
{
    uint16_t errorTemp = errorNumber;
    errorNumber = eERROR_NONE;
    return errorTemp;
}


/**
 * @brief Get the currently stored diagnoses, diganoses will be cleared afterwards
 *
 * @return all currently stored diagnoses bits
 */
uint16_t errorAndDiagnosis_getDiagnoses(void)
{
    uint16_t diagnosesTemp = diagnoses;
    diagnoses = eDIAGNOSIS_NONE;
    return diagnosesTemp;
}


/**
 * @brief Get the tests executed since last call
 *
 * @return all tests executed since this method has been called for the last time
 */
uint16_t errorAndDiagnosis_getExecutedTests(void)
{
    uint16_t executedTestsTemp = executedTests;
    executedTests = eEXECUTED_TEST_NONE;
    return executedTestsTemp;
}

