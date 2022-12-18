#include <Arduino.h>
#include <pins_arduino.h>
#include <stdbool.h>
#include "ioHandler.hpp"

#if SUPPORTED_OUTPUTS != 7
#error exactly 7 outputs are supported
#endif

#if SUPPORTED_INPUTS != 4
#error exactly 4 inputs are supported
#endif

static bool outputs[eSUPPORTED_OUTPUTS];    // output states,    false per default
static bool inputs[eSUPPORTED_INPUTS];      // input states,     false per default
static uint16_t watchdog;                   // watchdog counter, zero per default, will be counted down if set until it reaches zero

static const uint8_t outputPorts[eSUPPORTED_OUTPUTS + 1] = {D7, D8, D9, D11, D12, A1, A2, D6}; // last one is the watchdog
static const uint8_t inputPorts[eSUPPORTED_INPUTS] = {D2, D3, D4, D5};

enum
{
    eWATCHDOG_VALUE = 1714      // 120 would be ~ 4 seconds
};

enum
{
    eWATCH_DOG_INDEX = sizeof(outputPorts) - 1
}; // last entry in outputPorts is the watchdog port!!!

// set output port to 1 means toggle it every time this method has been called
static void setOutputPort(uint8_t outputNumber)
{
    if (outputNumber < eSUPPORTED_OUTPUTS + 1)
    {
        if (digitalRead(outputPorts[outputNumber]))
        {
            digitalWrite(outputPorts[outputNumber], LOW);
        }
        else
        {
            digitalWrite(outputPorts[outputNumber], HIGH);
        }
    }
}

// switch output port off
static void clearOutputPort(uint8_t outputNumber)
{
    if (outputNumber < eSUPPORTED_OUTPUTS + 1)
    {
        digitalWrite(outputPorts[outputNumber], LOW);
    }
}

static bool getInputPort(uint8_t inputNumber)
{
    bool value = false;
    if (inputNumber < eSUPPORTED_INPUTS)
    {
        value = (digitalRead(inputPorts[inputNumber]) == 1);
    }

    return value;
}

// setup used io ports
void inHandler_setup(void)
{
    for (uint8_t index = 0; index < sizeof(outputPorts); index++)
    {
        pinMode(outputPorts[index], OUTPUT);
    }

    for (uint8_t index = 0; index < sizeof(inputPorts); index++)
    {
        pinMode(inputPorts[index], INPUT);
    }
}

// set watchodg  state
void ioHandler_setWatchdog(uint16_t value)
{
    if (value)
    {
        value = eWATCHDOG_VALUE;
    }

    // set watch dog value
    noInterrupts();
    watchdog = value;
    interrupts();
}

bool ioHandler_readWatchdog(void)
{
    return (watchdog != 0);
}

// get watchdog state, each get call decrements the watchdog, has to be called cyclically!
bool ioHandler_getWatchdog(void)
{
    if (watchdog)
    {
        noInterrupts();
        watchdog--;
        interrupts();
    }

    return ioHandler_readWatchdog();
}

// set output state
void ioHandler_setOutput(uint16_t index, uint8_t value)
{
    if (index < eSUPPORTED_OUTPUTS)
    {
        outputs[index] = (value != 0);
    }
}

// get output state
bool ioHandler_getOutput(uint16_t index)
{
    bool result = false;
    if (index < eSUPPORTED_OUTPUTS)
    {
        result = outputs[index];
    }
    return result;
}

// get input state
bool ioHandler_getInput(uint16_t index)
{
    bool result = false;
    if (index < eSUPPORTED_INPUTS)
    {
        result = inputs[index];
    }
    return result;
}

// cyclic io handler since outputs need to be pulsed, has to be called cyclically!
void ioHandler_cyclicTask()
{
    // set watchdog output periodically so handler can toggle it!
    if (ioHandler_getWatchdog())
    {
        setOutputPort(eWATCH_DOG_INDEX);
    }
    else
    {
        clearOutputPort(eWATCH_DOG_INDEX);
    }

    // set outputs periodically so handler can toggle it!
    for (uint8_t index = 0; index < eSUPPORTED_OUTPUTS; index++)
    {
        if (outputs[index])
        {
            setOutputPort(index);
        }
        else
        {
            clearOutputPort(index);
        }
    }

    // read input
    for (uint8_t index = 0; index < eSUPPORTED_INPUTS; index++)
    {
        inputs[index] = (digitalRead(inputPorts[index]) != 0);
    }
}
