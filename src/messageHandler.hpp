#if not defined MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H


#include <stdint.h>
#include "debug.hpp"
#include "ioHandler.hpp"


/**
    GENERAL:    "<fno>;<cmd>;<payload>;<crc>;\n"

    WATCHDOG:
        request:  "<fno>;W;<state>;<crc>;\n"
        response: "<fno>;W;<state>;<crc>;\n"

    SET OUTPUT:
        request:  "<fno>;S;<output>;<state>;<crc>;\n"
        response: "<fno>;S;<output>;<state>;<crc>;\n"

    GET INPUT:
        request:  "<fno>;R;<input>;<crc>;\n"
        response: "<fno>;R;<input>;<state>;<crc>;\n"

    ERROR:
        request:  "<damaged>;\n"
        response: "<fno>;E;<err>;[<request>];<crc>;\n"
                  if the '\n' was damaged then the error response will be sent as soon as a '\n' has been detected and first n characters of damaged request are responded

    fno ...... 0..255 is the frame number that has to be incremented with each telegram
    output ... 0..6 (watchdog is not an output!)
    input .... 0..3
    state .... 0,1
    crc ...... CRC16 X25
    err ...... error number
    damaged .. damaged request or maybe even more than one request if '\n' was damaged

    examples:
        > 1;W;1;<crc>;\n                      # trigger watchdog
        < 1;W;1;<crc>;\n                      # ACK
        > 2;W;0;<crc>;\n                      # clear watchdog explicitely
        < 2;W;0;<crc>;\n                      # ACK
        < 2;E;errno;<crc>;<receivedStuff>;\n  # NACK in case of error
        > 3;S;0;1;<crc>;\n                    # switch output 0 ON
        < 3;S;0;1;<crc>;\n                    # ACK
        > 4;S;1;1;<crc>;\n                    # switch output 1 ON
        < 4;S;1;1;<crc>;\n                    # ACK
        > 5;R;0;<crc>;\n                      # read input 0
        < 5;R;0;0;<crc>;\n                    # input 0 is OFF
        < 5;R;0;1;<crc>;\n                    # input 0 is ON
*/


void messageHandler_receivedChar(char byte);


#endif
