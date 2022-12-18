#include <Arduino.h>
#include "ioHandler.hpp"
#include "timer.hpp"
#include "messageHandler.hpp"


void setup() {
    timer_setup();

    Serial.begin(9600);
}


void loop() {
    if (Serial.available() > 0)
    {
        messageHandler_receivedChar(Serial.read());
    }
}

