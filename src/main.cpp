#include <Arduino.h>
#include "ioHandler.hpp"
#include "timer.hpp"
#include "messageHandler.hpp"


void setup() {
    Serial.begin(9600);
    debug_setup();
    ioHandler_setup();
    timer_setup();
}


void loop() {
    if (Serial.available() > 0)
    {
        messageHandler_receivedChar(Serial.read());
    }
}

