#include <Arduino.h>
#include "debug.hpp"

// if DEBUG is not enabled ensure all critical DEBUG stuff is disabled!
#if defined DEBUG


#   if defined DEBUG1 || defined DEBUG2 || defined DEBUG3
// only needed if at least one of the debug prints is enabled
char debugPrintBuffer[100];
#   endif


#endif

