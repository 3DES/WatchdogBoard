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
static const uint8_t outputPorts[eSUPPORTED_OUTPUTS + ADDITIONAL_OUTPUTS] = {D7, D8, D9, D11, D12, A1, A2, /* watchdog output... */ D6};        // all output ports that have to be toggled cyclically
static const uint8_t ledPin = D13;
static const uint8_t resetLockPin = A0;         // needs to be switched between ON and hi-Z

enum
{
    eWATCH_DOG_INDEX  = eSUPPORTED_OUTPUTS,         // second last entry in outputPorts is the watchdog port!!!
};

static const uint8_t watchDogPort  = outputPorts[eWATCH_DOG_INDEX];


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
    // set pin to HIGH
    digitalWrite(resetLockPin, HIGH);       // ensure it's set to HIGH as soon as it will be configured as output
    pinMode(resetLockPin, OUTPUT);
}


// unlock reset pin, reset during UART connection is possible and e.g. new firmware can be installed
static inline void unlockReset(void)
{
    // set pin to hi-Z
    digitalWrite(resetLockPin, LOW);        // disable pullup because "HIGH" in input mode means pullup is enabled
    pinMode(resetLockPin, INPUT);           // meanwhile set it to hi-Z
}


// setup used io ports
void ioHandler_setup(void)
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
    digitalWrite(ledPin, HIGH);

    // setup reset lock pin (default behavior, so it's not necessary)
    //digitalWrite(resetLockPin, LOW);        // ensure pullup is disabled
    //pinMode(resetLockPin, INPUT);           // meanwhile set it to hi-Z
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
            //case eWATCHDOG_STATE_INIT:
            // led is switched ON in setup, so no further action is necessary here
            //    break;

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


static inline bool handleWatchdog(void)
{
    bool watchDogIsRunning = false;

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

    return watchDogIsRunning;
}


static inline void handleResetLock(void)
{
    // ask watchdog if external reset is allowed or not
    static bool resetPinLocked = false;  // initialize with FALSE since during startup a reset is allowed
    if (watchdog_lockResetPort())
    {
        if (!resetPinLocked)
        {
            debug_pin1(HIGH);
            lockReset();                    // no reset via UART allowed
            resetPinLocked = true;          // do this only once per lock state change
        }
    }
    else
    {
        if (resetPinLocked)
        {
            debug_pin1(LOW);
            unlockReset();                  // reset via UART is allowed (e.g. for firmware update)
            resetPinLocked = false;         // do this only once per lock state change
        }
    }
}


static inline void handleOutputs(bool watchDogIsRunning)
{
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
}


static inline void handleInputs(void)
{
    // read input
    for (uint8_t index = 0; index < eSUPPORTED_INPUTS; index++)
    {
        inputs[index] = getInputPort(index);
    }
}


// cyclic io handler since outputs need to be pulsed, has to be called cyclically!
void ioHandler_cyclicTask()
{
    debug_pin3(HIGH);

    highCycle = !highCycle;                         // switch between highCycle and !highCycle phase

    bool watchDogIsRunning = handleWatchdog();      // remember watchdog state and switch common outputs only if watchdog is running

    // lock or unlock reset pin
    handleResetLock();

    // set outputs periodically so handler can toggle it!
    handleOutputs(watchDogIsRunning);

    // read inputs
    handleInputs();

    // handle status LED
    handleLed();

    debug_pin3(LOW);
}

