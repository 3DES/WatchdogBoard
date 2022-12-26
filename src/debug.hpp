#if not defined DEBUG_H
#define DEBUG_H


#include <Arduino.h>
#include <pins_arduino.h>


#if defined D1
    #define P1(...) printf(__VA_ARGS__)
#else
    #define P1(...)
#endif


#if defined D2
    #define P2(...) printf(__VA_ARGS__)
#else
    #define P2(...)
#endif


#if defined D3
    #define P3(...) printf(__VA_ARGS__)
#else
    #define P3(...)
#endif


enum
{
    eDEBUG_PIN_1 = A3,
    eDEBUG_PIN_2 = A4,
    eDEBUG_PIN_3 = A5,
};


static inline void debug_setup(void)
{
    pinMode(eDEBUG_PIN_1, OUTPUT);
    pinMode(eDEBUG_PIN_2, OUTPUT);
    pinMode(eDEBUG_PIN_3, OUTPUT);
}


static inline void debug_pin1(uint8_t value)
{
    digitalWrite(eDEBUG_PIN_1, value);
}


static inline void debug_pin2(uint8_t value)
{
    digitalWrite(eDEBUG_PIN_2, value);
}


static inline void debug_pin3(uint8_t value)
{
    digitalWrite(eDEBUG_PIN_3, value);
}


#endif
