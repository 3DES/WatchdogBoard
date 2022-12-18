#if not defined TIMER_H
#define TIMER_H


enum
{
    eTICK_TIME  = 35,       // 35ms
    eTICK_VALUE = 8770,     // 35ms ~ 28.503021320259947Hz (16000000/((8770+1)*64))
};


void timer_setup(void);


#endif

