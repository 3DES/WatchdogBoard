#if not defined IO_HANDLER_H
#define IO_HANDLER_H


#define SUPPORTED_OUTPUTS 7
#define SUPPORTED_INPUTS  4


enum
{
    D0  = 0,
    D1  = 1,
    D2  = 2,
    D3  = 3,
    D4  = 4,
    D5  = 5,
    D6  = 6,
    D7  = 7,
    D8  = 8,
    D9  = 9,
    D10 = 10,
    D11 = 11,
    D12 = 12,
    D13 = 13,
};


enum
{
    eSUPPORTED_OUTPUTS = SUPPORTED_OUTPUTS,     // watchdog is not an output, so there are only 7 of them!
    eSUPPORTED_INPUTS  = SUPPORTED_INPUTS,      // 4 inputs are available!
};


void ioHandler_setup(void);

void ioHandler_setOutput(uint16_t index, uint8_t value);
bool ioHandler_getOutput(uint16_t index);
bool ioHandler_getInput(uint16_t index);

void ioHandler_cyclicTask();


#endif
