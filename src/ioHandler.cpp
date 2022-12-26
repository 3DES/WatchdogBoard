#include <Arduino.h>
#include <pins_arduino.h>
#include <stdint.h>
#include <stdbool.h>
#include "ioHandler.hpp"
#include "timer.hpp"
#include "watchdog.hpp"
#include "debug.hpp"


#if SUPPORTED_OUTPUTS != 7
#error exactly 7 outputs are supported
#endif

#if SUPPORTED_INPUTS != 4
#error exactly 4 inputs are supported
#endif

#define ADDITIONAL_OUTPUTS 2

static bool outputs[eSUPPORTED_OUTPUTS];    // output states,    false per default
static bool inputs[eSUPPORTED_INPUTS];      // input states,     false per default

static bool highCycle = false;              // switch all outputs synchronized, in highCycle phase switch all active outputs ON, in !highCycle phase switch all active outputs OFF

static const uint8_t inputPorts[eSUPPORTED_INPUTS] = {D2, D3, D4, D5};
static const uint8_t outputPorts[eSUPPORTED_OUTPUTS + ADDITIONAL_OUTPUTS] = {D7, D8, D9, D11, D12, A1, A2, /* special outputs... */ D6, A0};
static const uint8_t ledPin = D13;

enum
{
    eWATCH_DOG_INDEX  = eSUPPORTED_OUTPUTS,         // second last entry in outputPorts is the watchdog port!!!
    eLOCK_RESET_INDEX = eSUPPORTED_OUTPUTS + 1,     // last entry in outputPorts is the reset lock port!!!
};

static const uint8_t watchDogPort  = outputPorts[eWATCH_DOG_INDEX];
static const uint8_t lockResetPort = outputPorts[eLOCK_RESET_INDEX];


// set output port to 1 means toggle it every time this method has been called
static void setOutputPort(uint8_t outputNumber)
{
    if (outputNumber < sizeof(outputPorts))
    {
        // toggle port, for this check if it's currently LOW or HIGH
        if (highCycle)
        {
            digitalWrite(outputPorts[outputNumber], HIGH);
        }
        else
        {
            digitalWrite(outputPorts[outputNumber], LOW);
        }
    }
}


// switch output port off
static void clearOutputPort(uint8_t outputNumber)
{
    if (outputNumber < sizeof(outputPorts))
    {
        digitalWrite(outputPorts[outputNumber], LOW);
    }
}


// read input pin and return current state as boolean
static bool getInputPort(uint8_t inputNumber)
{
    bool value = false;
    if (inputNumber < eSUPPORTED_INPUTS)
    {
        value = (digitalRead(inputPorts[inputNumber]) != 0);
    }

    return value;
}


// set watchdog port to 1 (OK)
static inline void setWatchdogPort(void)
{
    setOutputPort(eWATCH_DOG_INDEX);
}


// set watchdog port to 0 (ERROR)
static inline void clearWatchdogPort(void)
{
    clearOutputPort(eWATCH_DOG_INDEX);
}


// lock reset pin to prevent reset during UART connection
static inline void lockReset(void)
{
    setOutputPort(eLOCK_RESET_INDEX);
}


// unlock reset pin, reset during UART connection is possible and e.g. new firmware can be installed
static inline void unlockReset(void)
{
    clearOutputPort(eLOCK_RESET_INDEX);
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

    // arduino nano LED used for diagnosis
    pinMode(ledPin, OUTPUT);
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


// just toggle the led on ledPin
static void ledToggle(void)
{
    if (digitalRead(ledPin))
    {
        // LED was ON so switch it OFF now
        digitalWrite(ledPin, LOW);
    }
    else
    {
        // LED was OFF so switch it ON now
        digitalWrite(ledPin, HIGH);
    }
}


// set LED mode and toggle LED if necessary
static inline void handleLed(void)
{
    // supported blink modes
    enum
    {
        eLED_TOGGLE_SLOW = 2000 / eTICK_TIME,
        eLED_TOGGLE_FAST =  200 / eTICK_TIME,
    };
    static uint16_t ledToggleCounter = 0;

    // when counter reaches zero decide what's to do now
    if (!ledToggleCounter)
    {
        switch (watchdog_getState())
        {
            case eWATCHDOG_STATE_INIT:
                // set LED only once in setup doesn't work, don't know why?!?!
                digitalWrite(ledPin, HIGH);
                break;

            case eWATCHDOG_STATE_OK:
                // toggle led and set slow blink mode
                ledToggle();
                ledToggleCounter = eLED_TOGGLE_SLOW;
                break;

            case eWATCHDOG_STATE_ERROR:
                // toggle led and set fast blink mode
                ledToggle();
                ledToggleCounter = eLED_TOGGLE_FAST;
                break;
        }
    }
    else
    {
        // counter hasn't reached zero yet
        ledToggleCounter--;
    }
}


// cyclic io handler since outputs need to be pulsed, has to be called cyclically!
void ioHandler_cyclicTask()
{
    debug_pin3(HIGH);

    bool watchDogIsRunning = false;     // remember watchdog state and switch common outputs only if watchdog is running

    highCycle = !highCycle;             // switch between highCycle and !highCycle phase

    // set watchdog output periodically so handler can toggle it!
    if (watchdog_getWatchdog())         // this executes the cyclic watchdog thread!
    {
        setWatchdogPort();              // WD is running
        watchDogIsRunning = true;       // remember watchdog state
    }
    else
    {
        clearWatchdogPort();            // WD is not running
        watchDogIsRunning = false;      // remember watchdog state
    }

    // ask watchdog if external reset is allowed or not
    if (watchdog_lockResetPort())
    {
        debug_pin1(HIGH);
        lockReset();                    // no reset via UART allowed
    }
    else
    {
        debug_pin1(LOW);
        unlockReset();                  // reset via UART is allowed (e.g. for firmware update)
    }

    // set outputs periodically so handler can toggle it!
    for (uint8_t index = 0; index < eSUPPORTED_OUTPUTS; index++)
    {
        // switch output ON if it is set to ON and there is no watchdog ERROR, otherwise switch it OFF
        if (outputs[index] && watchDogIsRunning)
        {
            setOutputPort(index);       // if watchdog is running and output is set to ON switch the referring port ON
        }
        else
        {
            clearOutputPort(index);     // if watchdog is not running or output is set to OFF switch the referring port OFF
        }
    }

    // read input
    for (uint8_t index = 0; index < eSUPPORTED_INPUTS; index++)
    {
        inputs[index] = getInputPort(index);
    }

    // handle status LED
    handleLed();

    debug_pin3(LOW);
}

