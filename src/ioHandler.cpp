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

#define ADDITIONAL_OUTPUTS 1

static bool outputs[eSUPPORTED_OUTPUTS];    // output states,    false per default
static bool inputs[eSUPPORTED_INPUTS];      // input states,     false per default

static bool highCycle = false;              // switch all outputs synchronized, in highCycle phase switch all active outputs ON, in !highCycle phase switch all active outputs OFF

#define WATCHDOG_OUTPUT D6

static const uint8_t inputPorts[eSUPPORTED_INPUTS] = {D2, D3, D4, D5};
static const uint8_t outputPorts[eSUPPORTED_OUTPUTS + ADDITIONAL_OUTPUTS]    = {D7, D8, D9, D11, D12, A1, A2, /* watchdog output... */ WATCHDOG_OUTPUT};        // all output ports including the watchdog output port that has to be the last given one!!!
static const bool    pulsedPorts[eSUPPORTED_OUTPUTS + ADDITIONAL_OUTPUTS]    = {1,  1,  1,  0,   0,   0,  0,  /* watchdog output... */ 1};         // a value > 0 means output has to be pulsed, last value relates to the watchdog port and is ignored (pulsed always)!!! This is not intended to save energy!
static const uint8_t ledPin = D13;
static const uint8_t resetLockPin = A0;         // needs to be switched between ON and hi-Z

enum
{
    eWATCH_DOG_INDEX  = eSUPPORTED_OUTPUTS,         // second last entry in outputPorts is the watchdog port!!!
};

static const uint8_t watchDogPort  = outputPorts[eWATCH_DOG_INDEX];


// set output port to 1 means toggle it every time this method has been called (outputs and watchdog can be handled, the caller has to ensure that the right output is set!)
static void setOutputPort(uint8_t outputNumber)
{
    if (outputNumber < sizeof(outputPorts))
    {
        // toggle watchdog port and pulsed port but switch ON not-pulsed port
        if (highCycle || (!pulsedPorts[outputNumber] && (outputNumber != eWATCH_DOG_INDEX)))
        {
            digitalWrite(outputPorts[outputNumber], HIGH);
        }
        else
        {
            digitalWrite(outputPorts[outputNumber], LOW);
        }
    }
}


// switch output port off (outputs and watchdog can be handled, the caller has to ensure that the right output is cleared!)
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
static inline void lockResetPort(void)
{
    // set pin to HIGH
    digitalWrite(resetLockPin, HIGH);       // ensure it's set to HIGH as soon as it will be configured as output (since it's currently an input the pull-up will be switched now, what is OK)
    pinMode(resetLockPin, OUTPUT);
}


// unlock reset pin, reset during UART connection is possible and e.g. new firmware can be installed
static inline void unlockResetPort(void)
{
    // set pin to hi-Z
    pinMode(resetLockPin, INPUT);           // meanwhile set it to hi-Z
    digitalWrite(resetLockPin, LOW);        // disable pullup because "HIGH" in input mode means pullup is enabled
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
        eLED_TOGGLE_FAST =  100 / eTICK_TIME,
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


/**
 * @brief Toggles watchdog output or switches it off
 * To be called only ONCE per executed cyclic task otherwise watchdog counter will count down too fast, what is not a safety problem but a availability one
 */
static inline void handleWatchdog(void)
{
    watchdog_selfTestHandler(ioHandler_getInput(eWATCHDOG_TEST_READBACK));

    // set watchdog output periodically so handler can toggle it!
    if (watchdog_trigger())         // this executes the cyclic watchdog thread!
    {
        setWatchdogPort();              // WD is running, set output (or toggle it if it is a pulsed one)
    }
    else
    {
        clearWatchdogPort();            // WD is not running, clear output
    }
}


static inline void handleResetLock(void)
{
    // ask watchdog if external reset is allowed or not
    static bool resetPinAlreadyLocked = false;  // initialize with FALSE since during startup a reset is allowed
    if (watchdog_resetPortMustBeLocked())
    {
        if (!resetPinAlreadyLocked)
        {
            debug_pin1(HIGH);
            lockResetPort();                // no reset via UART allowed
            resetPinAlreadyLocked = true;   // do this only once per lock state change and not repeatedly
        }
    }
    else
    {
        if (resetPinAlreadyLocked)
        {
            debug_pin1(LOW);
            unlockResetPort();              // reset via UART is allowed (e.g. for firmware update)
            resetPinAlreadyLocked = false;  // do this only once per lock state change and not repeatedly
        }
    }
}


static inline void handleOutputs(void)
{
    // set outputs periodically so handler can toggle it!
    for (uint8_t index = 0; index < eSUPPORTED_OUTPUTS; index++)
    {
        // switch output ON if it is set to ON and there is no watchdog ERROR, otherwise switch it OFF
        if (outputs[index] && watchdog_running())
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


/**
 * @brief Retriggers the watchdog output for arround 300us at a very high frequency to bring it (back) to ON state as fast as possible
 */
uint8_t ioHandler_watchdogStopAndRetrigger(void)
{
    #if WATCHDOG_OUTPUT != D6
    #   error WATCHDOG_OUTPUT must be D6, if watchdog output has been changed the following code has to be checked!
    #endif

    debug_pin2(HIGH);
    // prepare port values
    uint8_t portOn  = PORTD | (1 << WATCHDOG_OUTPUT);    // if watchdog output has been changed ensure that a shift with the referring define really works!!!
    uint8_t portOff = PORTD & ~(1 << WATCHDOG_OUTPUT);   // if watchdog output has been changed ensure that a shift with the referring define really works!!!

    uint16_t timeoutCounter = 10000;        // 10 seconds for high -> low
    static uint8_t lowCounter = 5;

    // want to see low level lowCounter times sice a single occurrence could be an EMC interference
    while (lowCounter-- && timeoutCounter)
    {
        while(getInputPort(eWATCHDOG_TEST_READBACK) && timeoutCounter)
        {
            // timer interrupt (1ms) occurred?
            if (timer_interruptSet())
            {
                timeoutCounter--;
                timer_interruptClear();
            }
        }
    }
    debug_pin2(LOW);

    if (!timeoutCounter)
    {
        // timeout occurred
        return eSTOP_AND_RETRIGGER_STOP_FAILED;
    }

    timeoutCounter = 10000;              // 10 seconds for high -> low
    static uint16_t highCounter = 500;   // even if high level has been seen again don't stop fast triggering just to ensure the relay stays active when the normal 1ms trigger period is reactivated!

    debug_pin2(HIGH);
    // want to see high level highCounter times sice a single occurrence could be an EMC interference
    while (highCounter && timeoutCounter)
    {
        // retrigger relay as fast as possible
        PORTD = portOff;
        PORTD = portOn;
        PORTD = portOff;
        PORTD = portOn;
        PORTD = portOff;
        PORTD = portOn;
        PORTD = portOff;
        PORTD = portOn;
        PORTD = portOff;
        PORTD = portOn;
        PORTD = portOff;
        PORTD = portOn;
        PORTD = portOff;
        PORTD = portOn;
        PORTD = portOff;
        PORTD = portOn;
        PORTD = portOff;
        PORTD = portOn;
        PORTD = portOff;
        PORTD = portOn;

        if (timer_interruptSet())
        {
            // timer interrupt (1ms) occurred, so decrement counter
            timeoutCounter--;
            timer_interruptClear();
        }

        if (getInputPort(eWATCHDOG_TEST_READBACK))
        {
            // correct state seen once, so decrement counter
            highCounter--;
        }
    }
    debug_pin2(LOW);

    if (!timeoutCounter)
    {
        // timeout occurred
        return eSTOP_AND_RETRIGGER_RETRIGGER_FAILED;
    }

    return eSTOP_AND_RETRIGGER_PASSED;
}


// cyclic io handler since outputs need to be pulsed, has to be called cyclically!
void ioHandler_cyclicTask()
{
    debug_pin3(HIGH);

    // switch between highCycle and !highCycle phase
    highCycle = !highCycle;

    // handle watchdog (will toggle it and switch it ON and OFF if necessary)
    handleWatchdog();

    // lock or unlock reset pin
    handleResetLock();

    // set outputs periodically so handler can toggle it!
    handleOutputs();

    // read inputs
    handleInputs();

    // handle status LED
    handleLed();

    debug_pin3(LOW);
}

