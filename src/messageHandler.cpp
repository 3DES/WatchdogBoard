#include <Arduino.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "debug.hpp"
#include "messageHandler.hpp"
#include "crc16X25.hpp"
#include "ioHandler.hpp"
#include "watchdog.hpp"


#define IGNORE_CRC 0            // set to 1 for debugging but don't forget to set back!
#define IGNORE_FRAME_NUMBER 0   // set to 1 for debugging but don't forget to set back!

enum
{
    eERROR_NO_ERROR = 0,
    eERROR_UNKNOWN_COMMAND = 1,
    eERROR_UNKNOWN_STATE = 2,
    eERROR_INVALID_FRAME_NUMBER = 3,
    eERROR_UNEXPECTED_FRAME_NUMBER = 4,
    eERROR_INVALID_VALUE = 5,
    eERROR_INVALID_INDEX = 6,
    eERROR_INVALID_CRC = 7,
    eERROR_OVERFLOW = 8,
};

enum
{
    eMAX_REQUEST_LENGTH = 20,
    eMAX_RESPONSE_LENGTH = 2 * eMAX_REQUEST_LENGTH,
};
char request[eMAX_REQUEST_LENGTH + 1] = "";
uint16_t requestIndex = 0;
char response[eMAX_RESPONSE_LENGTH + 2] = "";

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
        uint16_t divider = 10000;
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

// adds a request as a single token by including it into open and closing squared brackets
static uint16_t addRequest(char *string, uint16_t index, const char *const request)
{
    string[index++] = '[';
    for (uint16_t sourceIndex = 0; request[sourceIndex] >= ' '; sourceIndex++, index++)
    {
        string[index] = request[sourceIndex];
    }
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
static uint16_t setError(uint16_t newError)
{
    // set error only if it's the first detected one
    if (!receiveError)
    {
        receiveError = newError;
    }
    return receiveError;
}

// clear set error for next request
static void clearError()
{
    receiveError = eERROR_NO_ERROR;
}

static uint16_t getError()
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
        eCOMMAND_WATCHDOG = 'W',   // value for "watchdog" command
        eCOMMAND_SET_OUTPUT = 'S', // value for "set output" command
        eCOMMAND_READ_INPUT = 'R', // value for "read input" command
        eCOMMAND_NACK = 'E',       // value for NACK (only sent, never received!)
    };

    if (received != NULL)
    {
        P2(">>>>%s\n", received);

        enum
        {
            eKEY_INDEX_FRAME_NUMBER = 0,  // first entry is the frame number
            eKEY_INDEX_COMMAND = 1,       // second entry is the command
            eKEY_INDEX_EMPTY_COMMAND = 2, // if command token was empty we will reach this invalid state

            eKEY_INDEX_WATCHDOG = 100,       // steps for "watchdog" command handling...
            eKEY_INDEX_WATCHDOG_VALUE = 101, // handle received watchdog value (1 will re-trigger the watchdog, 0 will clear the watchdog immediately)
            eKEY_INDEX_WATCHDOG_CRC = 102,   // process received CRC
            eKEY_INDEX_WATCHDOG_END = 103,   // watchdog end state

            eKEY_INDEX_SET_OUTPUT = 200,       // steps for "set output" command handling...
            eKEY_INDEX_SET_OUTPUT_INDEX = 201, // index of output to be set
            eKEY_INDEX_SET_OUTPUT_VALUE = 202, // value output will be set to
            eKEY_INDEX_SET_OUTPUT_CRC = 203,   // process received CRC
            eKEY_INDEX_SET_OUTPUT_END = 204,   // "set output" end state

            eKEY_INDEX_GET_INPUT = 300,       // steps for "read input" command handling...
            eKEY_INDEX_GET_INPUT_INDEX = 301, // index of input to be read
            eKEY_INDEX_GET_INPUT_CRC = 302,   // process received CRC
            eKEY_INDEX_GET_INPUT_END = 303,   // "read input"" end state
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

        clearError();

        for (uint16_t index = 0; received[index] > '\x0A' && !getError(); index++)
        {
            if (calculateCrc != eCRC_CALCULATION_DISABLED)
            {
                crc = crc16X25Step(received[index], crc);
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
                    setError(eERROR_INVALID_FRAME_NUMBER);
                }
                P3("%d]", commandValue);
                break;

            // handle command and switch to command sub states
            case eKEY_INDEX_COMMAND:
                P3("C");
                // remember received command
                command = received[index];

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

                default:
                    setError(eERROR_UNKNOWN_COMMAND);
                    break;
                }
                break;

            // if command token was empty (e.g. 1;;1;1;) or initial command stage (e.g. 1;WW;1;1;) is reached than command is invalid
            case eKEY_INDEX_EMPTY_COMMAND:
            case eKEY_INDEX_WATCHDOG:
            case eKEY_INDEX_SET_OUTPUT:
            case eKEY_INDEX_GET_INPUT:
                setError(eERROR_UNKNOWN_COMMAND);
                break;

            // handle command's value part
            case eKEY_INDEX_WATCHDOG_VALUE:
            case eKEY_INDEX_SET_OUTPUT_VALUE:
                P3("V[");
                if (createDecimal(&commandValue, received[index]))
                {
                    setError(eERROR_INVALID_VALUE);
                }
                P3("%d]", commandValue);
                calculateCrc = eCRC_CALCULATION_TO_DISABLE;
                break;

            // handle command's index part
            case eKEY_INDEX_SET_OUTPUT_INDEX:
                P3("I[");
                if (createDecimal(&commandIndex, received[index]))
                {
                    setError(eERROR_INVALID_INDEX);
                }
                P3("%d]", commandIndex);
                break;

            // handle command's index part
            case eKEY_INDEX_GET_INPUT_INDEX:
                P3("I[");
                if (createDecimal(&commandIndex, received[index]))
                {
                    setError(eERROR_INVALID_INDEX);
                }
                P3("%d]", commandIndex);
                calculateCrc = eCRC_CALCULATION_TO_DISABLE;
                break;

            // handle command's CRC part
            case eKEY_INDEX_WATCHDOG_CRC:
            case eKEY_INDEX_SET_OUTPUT_CRC:
            case eKEY_INDEX_GET_INPUT_CRC:
                P3("S[");
                if (createDecimal(&receivedCrc, received[index]))
                {
                    setError(eERROR_INVALID_CRC);
                }
                P3("%d]", receivedCrc);
                break;

            case eKEY_INDEX_WATCHDOG_END:
            case eKEY_INDEX_SET_OUTPUT_END:
            case eKEY_INDEX_GET_INPUT_END:
                // nth. to do here
                break;

            // unknown situation (probably too many fields in the received command)
            default:
                P3("E(%d)", keyIndex);
                setError(eERROR_UNKNOWN_STATE);
                break;
            }
        }

        // crc check
        P3("\nCRCs: [%d] =?= [%d]\n", crc, receivedCrc);
        if ((crc != receivedCrc) && !IGNORE_CRC)
        {
            setError(eERROR_INVALID_CRC);
        }
        else
        {
            // frame number validation
            if ((frameNumber != uint16_t(nextExpectedFrameNumber)) && !IGNORE_FRAME_NUMBER)
            {
                P3("[%d]!=[%d+1]", frameNumber, nextExpectedFrameNumber);
                setError(eERROR_UNEXPECTED_FRAME_NUMBER);
            }

            // validate command parameter(s)
            switch (command)
            {
            case eCOMMAND_WATCHDOG:
                // watchdog command has only a value but no index
                if (commandValue > 1)
                {
                    setError(eERROR_INVALID_VALUE);
                }
                break;

            case eCOMMAND_SET_OUTPUT:
                // set output command has an index and a value
                if (commandIndex >= eSUPPORTED_OUTPUTS)
                {
                    setError(eERROR_INVALID_INDEX);
                }
                else if (commandValue > 1)
                {
                    setError(eERROR_INVALID_VALUE);
                }
                break;

            case eCOMMAND_READ_INPUT:
                // get input command has only an index but no value
                if (commandIndex >= eSUPPORTED_INPUTS)
                {
                    setError(eERROR_INVALID_INDEX);
                }
                break;
            }
        }

        // prepare response and execute command
        uint16_t index = 0;
        index = addInteger(response, index, nextExpectedFrameNumber);
        if (getError())
        {
            index = addChar(response, index, eCOMMAND_NACK);
            index = addInteger(response, index, getError());
            index = addRequest(response, index, received);
        }
        else
        {
            index = addChar(response, index, command);
            switch (command)
            {
            case eCOMMAND_WATCHDOG:
                watchdog_setWatchdog(commandValue);
                index = addInteger(response, index, watchdog_readWatchdog());
                break;

            case eCOMMAND_SET_OUTPUT:
                ioHandler_setOutput(commandIndex, commandValue);
                index = addInteger(response, index, commandIndex);
                index = addInteger(response, index, ioHandler_getOutput(commandIndex));
                break;

            case eCOMMAND_READ_INPUT:
                index = addInteger(response, index, commandIndex);
                index = addInteger(response, index, ioHandler_getInput(commandIndex));
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
        index = addInteger(response, index, eERROR_OVERFLOW);
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
