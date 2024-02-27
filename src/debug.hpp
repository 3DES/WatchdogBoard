#if not defined DEBUG_H
#define DEBUG_H


#include <Arduino.h>
#include <pins_arduino.h>


//#define DEBUG              // firmware "D_xxx"
//#define ALWAYS_RUNNING     // firmware "T_xxx", watchdog will never be cleared, that's only accepted in DEBUG case and to be used with care!!!
#if not defined DEBUG
// defines to disable critical DEBUG behavior
#   define P1(...)
#   define P2(...)
#   define P3(...)
#   define IGNORE_CRC 0            // set to 1 for debugging but don't forget to set back!
#   define IGNORE_FRAME_NUMBER 0   // set to 1 for debugging but don't forget to set back!
#   if defined ALWAYS_RUNNING
#       undef ALWAYS_RUNNING       // defensive programming!
#   endif
#else
// all critical DEBUG stuff has to be put into this define block!
#   define IGNORE_CRC 1            // set to 1 for debugging but don't forget to set back!
#   define IGNORE_FRAME_NUMBER 1   // set to 1 for debugging but don't forget to set back!

// enable debug prints if necessary
//#   define DEBUG1
//#   define DEBUG2
#   define DEBUG3


#   if defined DEBUG1 || defined DEBUG2 || defined DEBUG3
extern char debugPrintBuffer[100];
#   endif


#   if defined DEBUG1
#       define P1(...) do { sprintf(debugPrintBuffer, __VA_ARGS__); Serial.print(debugPrintBuffer); } while (0)
#   else
#       define P1(...)
#   endif


#   if defined DEBUG2
#       define P2(...) do { sprintf(debugPrintBuffer, __VA_ARGS__); Serial.print(debugPrintBuffer); } while (0)
#   else
#       define P2(...)
#   endif


#   if defined DEBUG3
#       define P3(...) do { sprintf(debugPrintBuffer, __VA_ARGS__); Serial.print(debugPrintBuffer); } while (0)
#   else
#       define P3(...)
#   endif


#endif


// debug stuff that is always available independent from DEBUG definition
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
