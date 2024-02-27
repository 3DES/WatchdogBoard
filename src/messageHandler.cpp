#include <Arduino.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "debug.hpp"
#include "messageHandler.hpp"
#include "crc16X25.hpp"
#include "ioHandler.hpp"
#include "watchdog.hpp"
#include "version.hpp"
#include "errorAndDiagnosis.hpp"

#define MAGIC {'M','H','S','W','M','H','S','W'}     // 4D4853574D485357

enum
{
    eMESSAGE_ERROR_NONE = 0,
    eMESSAGE_ERROR_UNKNOWN_COMMAND = 1,
    eMESSAGE_ERROR_UNKNOWN_STATE = 2,
    eMESSAGE_ERROR_INVALID_FRAME_NUMBER = 3,
    eMESSAGE_ERROR_UNEXPECTED_FRAME_NUMBER = 4,
    eMESSAGE_ERROR_INVALID_VALUE = 5,
    eMESSAGE_ERROR_INVALID_INDEX = 6,
    eMESSAGE_ERROR_INVALID_CRC = 7,
    eMESSAGE_ERROR_OVERFLOW = 8,
    eMESSAGE_ERROR_INVALID_STARTUP = 9,             // before watchdog can be set version has to be requested!
};

// request/response transmit definitions
#define MAX_REQUEST_LENGTH  (20)
#define MAX_RESPONSE_LENGTH (3 * (MAX_REQUEST_LENGTH))
enum
{
    eMAX_REQUEST_LENGTH = MAX_REQUEST_LENGTH,
    eMAX_RESPONSE_LENGTH = MAX_RESPONSE_LENGTH,
};
char request[eMAX_REQUEST_LENGTH + 1] = "";
uint16_t requestIndex = 0;
char response[eMAX_RESPONSE_LENGTH + 2] = "";
static bool versionReadCommandReceived;         // before any 'W' commands are accepted the version has to be read with 'V'!

// version information
#if MAX_REQUEST_LENGTH != 20
#   error MAX_REQUEST_LENGTH has to be 20 to avoid buffer overflow in answer string!!!
#endif
static const struct __attribute__((packed)) {
    char leadIn[8];
    char version[MAX_REQUEST_LENGTH];
    char leadOut[8];
} VERSION_FIELD = {
    MAGIC,
#if defined DEBUG && defined ALWAYS_RUNNING
    "T_"         // hard coded leading 'T_' in case of trigger always version!
#elif defined DEBUG
    "D_"         // hard coded leading 'D_' in case of debug version!
#endif

    VERSION,
    MAGIC
};

// put a semicolon and a string end character to the given string to prepare string for next token
static inline uint16_t finalizeToken(char *string, uint16_t index)
{
    string[index++] = ';';
    string[index] = '\0';

    return index;
}

// add an integer at the end of the string, concatenate a ';' and finally a string end character '\0'
static uint16_t addInteger(char *string, uint16_t index, uint16_t value)
{
    if (value == 0)
    {
        // necessary since algorithm cannot handle 0 value!
        string[index++] = '0';
    }
    else if (value == 1)
    {
        // to speed up since a lot of values are just 1, e.g. input/output states
        string[index++] = '1';
    }
    else
    {
        bool digitAdded = false; // as soon as a digit has been added all positions containing a '0' have to be added, too, otherwise e.g. a 102030 will become 123
        uint16_t divider = 10000;  // uint16_t is always smaller than 100.000 so the decimal divider to be used to get the most left decimal digit is 10.000
        while (divider)
        {
            if (value >= divider || digitAdded)
            {
                uint8_t newDigit = (uint8_t)(value / divider);
                string[index++] = newDigit + '0';
                value -= divider * newDigit;
                digitAdded = true; // digit added so ensure all positions containing a '0' will also be added
            }
            else if (digitAdded)
            {
                string[index++] = '0';
            }
            divider /= 10;
        }
    }
    return finalizeToken(string, index);
}

// add an character at the end of the string, concatenate a ';' and finally a string end character '\0'
static uint16_t addChar(char *string, uint16_t index, char character)
{
    string[index++] = character;
    return finalizeToken(string, index);
}

// add a string (only ASCII characters 0x20-0x7E) as a single token by including it into open and closing squared brackets
static uint16_t addString(char *string, uint16_t index, const char *const concatString)
{
    for (uint16_t sourceIndex = 0; concatString[sourceIndex] >= '\x20' && concatString[sourceIndex] <= '\x7E'; sourceIndex++, index++)
    {
        string[index] = concatString[sourceIndex];
    }
    return index;
}

// add a request as a single token by including it into open and closing squared brackets
static uint16_t addRequest(char *string, uint16_t index, const char *const request)
{
    string[index++] = '[';
    index = addString(string, index, request);
    string[index++] = ']';
    return finalizeToken(string, index);
}

// calculate a decimal value that is created number by number from highest to lowest
static inline bool createDecimal(uint16_t *value, char add)
{
    bool error = true;

    uint16_t increment = add - '0';

    // only add valid decimal values
    if (increment < 10)
    {
        // check for overflow
        uint16_t validate = *value * 10;
        if ((validate / 10) == *value)
        {
            // check for overflow
            if (uint16_t(validate + increment) >= validate)
            {
                validate += (add - '0');
                *value = validate;
                error = false;
            }
        }
    }

    // return error checking result
    return error;
}

// error variable for currently received request
static uint16_t receiveError;

// set error if not already set
static void setMessageError(uint16_t newError)
{
    // only remember the first detected error
    if (!receiveError)
    {
        receiveError = newError;
    }
}

// clear set error for next request
static inline void clearMessageError()
{
    receiveError = eMESSAGE_ERROR_NONE;
}

static inline uint16_t getMessageError()
{
    return receiveError;
}

// handle received request
static void handleRequest(char *received)
{
    static uint16_t nextExpectedFrameNumber = 0;
    uint16_t crc = eCRC16_X25_INIT;

    enum
    {
        eCOMMAND_WATCHDOG = 'W',        // value for "watchdog" command
        eCOMMAND_SET_OUTPUT = 'S',      // value for "set output" command
        eCOMMAND_READ_INPUT = 'R',      // value for "read input" command
        eCOMMAND_GET_VERSION = 'V',     // value for "get version" command
        eCOMMAND_EXECUTE_TEST = 'T',    // value for "execute test" command
        eCOMMAND_GET_DIAGNOSES = 'D',   // value for "get diagnoses" command

        eCOMMAND_NACK = 'E',            // value for NACK (only sent, never received!)
    };

    if (received != NULL)
    {
        P2(">>>>%s\n", received);

        enum
        {
            eKEY_INDEX_FRAME_NUMBER = 0,            // first entry is the frame number
            eKEY_INDEX_COMMAND = 1,                 // second entry is the command
            eKEY_INDEX_EMPTY_COMMAND = 2,           // if command token was empty we will reach this invalid state

            eKEY_INDEX_WATCHDOG = 100,              // steps for "watchdog" command handling...
            eKEY_INDEX_WATCHDOG_VALUE = 101,        // handle received watchdog value (1 will re-trigger the watchdog, 0 will clear the watchdog immediately)
            eKEY_INDEX_WATCHDOG_CRC = 102,          // process received CRC
            eKEY_INDEX_WATCHDOG_END = 103,          // watchdog end state

            eKEY_INDEX_SET_OUTPUT = 200,            // steps for "set output" command handling...
            eKEY_INDEX_SET_OUTPUT_INDEX = 201,      // index of output to be set
            eKEY_INDEX_SET_OUTPUT_VALUE = 202,      // value output will be set to
            eKEY_INDEX_SET_OUTPUT_CRC = 203,        // process received CRC
            eKEY_INDEX_SET_OUTPUT_END = 204,        // "set output" end state

            eKEY_INDEX_GET_INPUT = 300,             // steps for "read input" command handling...
            eKEY_INDEX_GET_INPUT_INDEX = 301,       // index of input to be read
            eKEY_INDEX_GET_INPUT_CRC = 302,         // process received CRC
            eKEY_INDEX_GET_INPUT_END = 303,         // "read input" end state

            eKEY_INDEX_GET_VERSION = 400,           // steps for "get version" command handling...
            eKEY_INDEX_GET_VERSION_CRC = 401,       // process received CRC
            eKEY_INDEX_GET_VERSION_END = 402,       // "get version" end state

            eKEY_INDEX_GET_DIAGNOSES = 500,         // steps for "get diagnoses" command handling...
            eKEY_INDEX_GET_DIAGNOSES_CRC = 501,     // process received CRC
            eKEY_INDEX_GET_DIAGNOSES_END = 502,     // "get diagnoses" end state

            eKEY_INDEX_EXECUTE_TEST = 600,          // steps for "execute test" command handling...
            eKEY_INDEX_EXECUTE_TEST_CRC = 601,      // process received CRC
            eKEY_INDEX_EXECUTE_TEST_END = 602,      // "execute test" end state
        };

        enum
        {
            eCRC_CALCULATION_ENABLED,
            eCRC_CALCULATION_TO_DISABLE,
            eCRC_CALCULATION_DISABLED,
        };

        uint16_t keyIndex = 0;

        uint16_t frameNumber = 0;
        char command = ' ';
        uint16_t commandIndex = 0;
        uint16_t commandValue = 0;

        uint16_t calculateCrc = eCRC_CALCULATION_ENABLED;
        uint16_t receivedCrc = 0;

        clearMessageError();

        for (uint16_t index = 0; received[index] > '\x0A' && !getMessageError(); index++)
        {
            if (calculateCrc != eCRC_CALCULATION_DISABLED)
            {
                crc = crc16X25Step(received[index], crc);
                P3("{%c}", received[index]);
            }

            //Serial.print('[');
            //Serial.print(keyIndex, DEC);
            //Serial.print(']');

            if (received[index] == ';')
            {
                keyIndex++;
                if (calculateCrc == eCRC_CALCULATION_TO_DISABLE)
                {
                    calculateCrc = eCRC_CALCULATION_DISABLED;
                }
                continue;
            }

            switch (keyIndex)
            {
                // handle framen number
                case eKEY_INDEX_FRAME_NUMBER:
                    P3("F[");
                    if (createDecimal(&frameNumber, received[index]))
                    {
                        setMessageError(eMESSAGE_ERROR_INVALID_FRAME_NUMBER);
                    }
                    P3("%d]", commandValue);
                    break;

                // handle command and switch to command sub states
                case eKEY_INDEX_COMMAND:
                    P3("C[");
                    // remember received command
                    command = received[index];
                    P3("%c]", command);

                    // switch to command's sub states
                    switch (received[index])
                    {
                    case eCOMMAND_WATCHDOG:
                        keyIndex = eKEY_INDEX_WATCHDOG;
                        break;

                    case eCOMMAND_SET_OUTPUT:
                        keyIndex = eKEY_INDEX_SET_OUTPUT;
                        break;

                    case eCOMMAND_READ_INPUT:
                        keyIndex = eKEY_INDEX_GET_INPUT;
                        break;

                    case eCOMMAND_GET_VERSION:
                        keyIndex = eKEY_INDEX_GET_VERSION;
                        calculateCrc = eCRC_CALCULATION_TO_DISABLE;         // "get version" has no parameters so we have to stop CRC calculation right here
                        P3("X");
                        break;

                    case eCOMMAND_GET_DIAGNOSES:
                        keyIndex = eKEY_INDEX_GET_DIAGNOSES;
                        calculateCrc = eCRC_CALCULATION_TO_DISABLE;         // "get diagnoses" has no parameters so we have to stop CRC calculation right here
                        P3("X");
                        break;

                    case eCOMMAND_EXECUTE_TEST:
                        keyIndex = eKEY_INDEX_EXECUTE_TEST;
                        calculateCrc = eCRC_CALCULATION_TO_DISABLE;         // "execute test" has no parameters so we have to stop CRC calculation right here
                        P3("X");
                        break;

                    default:
                        setMessageError(eMESSAGE_ERROR_UNKNOWN_COMMAND);
                        break;
                    }
                    break;

                // if command token was empty (e.g. 1;;1;1;) or initial command stage (e.g. 1;WW;1;1;) is reached than command is invalid
                case eKEY_INDEX_EMPTY_COMMAND:
                case eKEY_INDEX_WATCHDOG:
                case eKEY_INDEX_SET_OUTPUT:
                case eKEY_INDEX_GET_INPUT:
                case eKEY_INDEX_GET_VERSION:
                case eKEY_INDEX_GET_DIAGNOSES:
                case eKEY_INDEX_EXECUTE_TEST:
                    setMessageError(eMESSAGE_ERROR_UNKNOWN_COMMAND);
                    break;

                // handle command's value part
                case eKEY_INDEX_WATCHDOG_VALUE:
                case eKEY_INDEX_SET_OUTPUT_VALUE:
                    P3("V[");
                    if (createDecimal(&commandValue, received[index]))
                    {
                        setMessageError(eMESSAGE_ERROR_INVALID_VALUE);
                    }
                    P3("%d]", commandValue);
                    calculateCrc = eCRC_CALCULATION_TO_DISABLE;     // last CRC protected element in message so stop CRC calculation now!
                    P3("X");
                    break;

                // handle command's index part
                case eKEY_INDEX_SET_OUTPUT_INDEX:
                    P3("I[");
                    if (createDecimal(&commandIndex, received[index]))
                    {
                        setMessageError(eMESSAGE_ERROR_INVALID_INDEX);
                    }
                    P3("%d]", commandIndex);
                    break;

                // handle command's index part
                case eKEY_INDEX_GET_INPUT_INDEX:
                    P3("I[");
                    if (createDecimal(&commandIndex, received[index]))
                    {
                        setMessageError(eMESSAGE_ERROR_INVALID_INDEX);
                    }
                    P3("%d]", commandIndex);
                    calculateCrc = eCRC_CALCULATION_TO_DISABLE;     // last CRC protected element in message so stop CRC calculation now!
                    P3("X");
                    break;

                // handle command's CRC part
                case eKEY_INDEX_WATCHDOG_CRC:
                case eKEY_INDEX_SET_OUTPUT_CRC:
                case eKEY_INDEX_GET_INPUT_CRC:
                case eKEY_INDEX_GET_VERSION_CRC:
                case eKEY_INDEX_GET_DIAGNOSES_CRC:
                case eKEY_INDEX_EXECUTE_TEST_CRC:
                    P3("S[");
                    if (createDecimal(&receivedCrc, received[index]))
                    {
                        setMessageError(eMESSAGE_ERROR_INVALID_CRC);
                    }
                    P3("%u]", receivedCrc);
                    break;

                case eKEY_INDEX_WATCHDOG_END:
                case eKEY_INDEX_SET_OUTPUT_END:
                case eKEY_INDEX_GET_INPUT_END:
                case eKEY_INDEX_GET_VERSION_END:
                case eKEY_INDEX_GET_DIAGNOSES_END:
                case eKEY_INDEX_EXECUTE_TEST_END:
                    // nth. to do here
                    break;

                // unknown situation (probably too many fields in the received command)
                default:
                    P3("E(%d)", keyIndex);
                    setMessageError(eMESSAGE_ERROR_UNKNOWN_STATE);
                    break;
            }
        }

        // crc check
        crc = crc16X25Xor(crc);
        P3("\nCRCs: [%u] =?= [%u]\n", crc, receivedCrc);
        if ((crc != receivedCrc) && !IGNORE_CRC)
        {
            setMessageError(eMESSAGE_ERROR_INVALID_CRC);
        }
        else
        {
            // frame number validation
            if ((frameNumber != uint16_t(nextExpectedFrameNumber)) && !IGNORE_FRAME_NUMBER)
            {
                P3("[%d]!=[%d+1]", frameNumber, nextExpectedFrameNumber);
                setMessageError(eMESSAGE_ERROR_UNEXPECTED_FRAME_NUMBER);
            }

            // validate command parameter(s)
            switch (command)
            {
                case eCOMMAND_WATCHDOG:
                    // watchdog command has only a value but no index
                    if (commandValue > 1)   // only 0 and 1 are allowed values for the watchdog state!
                    {
                        setMessageError(eMESSAGE_ERROR_INVALID_VALUE);
                    }
                    if (!versionReadCommandReceived)   // only 0 and 1 are allowed values for the watchdog state!
                    {
                        setMessageError(eMESSAGE_ERROR_INVALID_STARTUP);
                    }
                    break;

                case eCOMMAND_SET_OUTPUT:
                    // set output command has an index and a value
                    if (commandIndex >= eSUPPORTED_OUTPUTS)     // this avoids that watchdog is changed with "set output" command, since eSUPPORTED_OUTPUTS is at least one smaller than number of existing outputs!
                    {
                        setMessageError(eMESSAGE_ERROR_INVALID_INDEX);
                    }
                    else if (commandValue > 1)  // only 0 and 1 are allowed values for the output state!
                    {
                        setMessageError(eMESSAGE_ERROR_INVALID_VALUE);
                    }
                    break;

                case eCOMMAND_READ_INPUT:
                    // get input command has only an index but no value
                    if (commandIndex >= eSUPPORTED_INPUTS)
                    {
                        setMessageError(eMESSAGE_ERROR_INVALID_INDEX);
                    }
                    break;
            }
        }

        // prepare response and execute command
        uint16_t index = 0;
        index = addInteger(response, index, nextExpectedFrameNumber);
        if (getMessageError())
        {
            index = addChar(response, index, eCOMMAND_NACK);
            index = addInteger(response, index, getMessageError());
            index = addRequest(response, index, received);
            index = addInteger(response, index, crc);
        }
        else
        {
            index = addChar(response, index, command);
            switch (command)
            {
                case eCOMMAND_WATCHDOG:
                    index = addInteger(response, index, watchdog_readWatchdog());
                    watchdog_setWatchdog(commandValue);
                    index = addInteger(response, index, watchdog_readWatchdog());
                    index = addInteger(response, index, watchdog_resetPortMustBeLocked() ? 1: 0);
                    break;

                case eCOMMAND_SET_OUTPUT:
                    index = addInteger(response, index, commandIndex);
                    index = addInteger(response, index, ioHandler_getOutput(commandIndex));
                    ioHandler_setOutput(commandIndex, commandValue);
                    index = addInteger(response, index, ioHandler_getOutput(commandIndex));
                    break;

                case eCOMMAND_READ_INPUT:
                    index = addInteger(response, index, commandIndex);
                    index = addInteger(response, index, ioHandler_getInput(commandIndex));
                    break;

                case eCOMMAND_GET_VERSION:
                    index = addString(response, index, VERSION_FIELD.version);
                    index = finalizeToken(response, index);
                    versionReadCommandReceived = true;         // remember that version has been requested, therefore, watchdog can be switched ON now
                    break;

                case eCOMMAND_GET_DIAGNOSES:
                    index = addInteger(response, index, errorAndDiagnosis_getDiagnoses());
                    index = addInteger(response, index, errorAndDiagnosis_getErrorNumber());
                    index = addInteger(response, index, errorAndDiagnosis_getExecutedTests());
                    break;

                case eCOMMAND_EXECUTE_TEST:
                    index = addInteger(response, index, watchdog_requestSelfTest());
                    break;

                default:
                    // not possible since error handling would be active in that case and we wouldn't be here!
                    break;
            }

            // no error so increment frame number since for the current frame number a valid message has been received
            nextExpectedFrameNumber++;
        }
        index = addInteger(response, index, crc16X25(response, index));
    }
    else
    {
        P2(">>>>overflow\n");
        uint16_t index = 0;
        index = addInteger(response, index, nextExpectedFrameNumber);
        index = addChar(response, index, eCOMMAND_NACK);
        index = addInteger(response, index, eMESSAGE_ERROR_OVERFLOW);
        index = addInteger(response, index, crc16X25(response, index));
    }
    P2("<<<<%s\n\n", response);
    Serial.println(response);
}

// processes received byte
void messageHandler_receivedChar(char byte)
{
    if (requestIndex >= eMAX_REQUEST_LENGTH)
    {
        // synchronize to next NL (or NUL)
        if ((byte == '\n') || (byte == '\0'))
        {
            // throw all received data away...
            handleRequest(NULL);
            requestIndex = 0;
        }
    }
    else if ((byte != '\n') && (byte != '\0'))
    {
        request[requestIndex++] = byte;
    }
    else
    {
        request[requestIndex] = '\0';
        handleRequest(request);
        requestIndex = 0;
    }
}
