#if not defined TIMER_H
#define TIMER_H


#include <stdint.h>

enum
{
    eTICK_TIME  = 1,       // 1ms (1ms is the shortest allowed possible tick time, otherwise cyclic io handler task will not work anymore!!!)
    eTICK_VALUE = (uint16_t)(((uint64_t)16*1000000 * eTICK_TIME) / ((uint64_t)64 * 1000)) - 1,   // x = ((16*10^6 * eTICK_TIME) / (64 * 1000)) - 1
};


void timer_setup(void);


#endif

