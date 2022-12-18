#include <Arduino.h>
#include "ioHandler.hpp"
#include "timer.hpp"


ISR(TIMER1_COMPA_vect)
{
    ioHandler_cyclicTask();
}


void timer_setup(void)
{
    // https://www.arduinoslovakia.eu/application/timer-calculator
    noInterrupts();

    // Clear registers
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;

    // counter value
    OCR1A = eTICK_VALUE;
    // CTC
    TCCR1B |= (1 << WGM12);
    // Prescaler 64
    TCCR1B |= (1 << CS11) | (1 << CS10);
    // Output Compare Match A Interrupt Enable
    TIMSK1 |= (1 << OCIE1A);

    interrupts();
}

