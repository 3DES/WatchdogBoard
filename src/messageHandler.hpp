#if not defined MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H


#include <stdint.h>
#include "debug.hpp"
#include "ioHandler.hpp"


/**
    GENERAL:    "<fno>;<cmd>;<payload>;<crc>;\n"

    WATCHDOG:
        request:  "<fno>;W;<state>;<crc>;\n"
        response: "<fno>;W;<oldState>;<newState>;<lockState>;<crc>;\n"

    SET OUTPUT:
        request:  "<fno>;S;<output>;<state>;<crc>;\n"
        response: "<fno>;S;<output>;<oldState>;<newState>;<crc>;\n"

    GET INPUT:
        request:  "<fno>;R;<input>;<crc>;\n"
        response: "<fno>;R;<input>;<state>;<crc>;\n"

    GET VERSION:
        request:  "<fno>;V;<crc>;\n"
        response: "<fno>;V;<version>;<crc>;\n"

    GET DIAGNOSES:
        request:  "<fno>;D;<crc>;\n"
        response: "<fno>;D;<diagnosis>;<firstError>;<executedTests>;<crc>;\n"

    EXECUTE TEST:
        request:  "<fno>;T;<crc>;\n"
        response: "<fno>;T;<requestAccepted>;<crc>;\n"

    ERROR:
        request:  "<damaged>;\n"
        response: "<expectedFNo>;E;<err>;[<request>];<crc>;\n"
                  if the '\n' was damaged then the error response will be sent as soon as a '\n' has been detected and first n characters of damaged request are responded

    fno ............. 0..255 is the frame number that has to be incremented with each telegram
    output .......... 0..6 (watchdog is not an output!)
    input ........... 0..3
    state ........... 0,1
    diagnosis ....... 16 bit diagnosis collected since last "get diagnosis" command
    firstError ...... first detected error since last "get diagnosis" command
    executedTests ... executed self tests since last "get diagnosis" command
    crc ............. CRC16 X25
    err ............. error number
    damaged ......... damaged request or maybe even more than one request if '\n' was damaged
    expectedFNo ..... error response sends the frame number back that would have been expected, so next valid command should use this frame number

    semicolon in front of CRC is included in CRC but the CRC and the following semicolon is not but it's expected and, therefore, also protected!

    to test either set IGNORE_CRC validation in debug.hpp or use a page for proper calculation of CRC16-X25, e.g. https://crccalc.com

    examples:
        > 0;V;5971;\n                       # get version
        < 0;V;1.0_4xUNPULSED;63918;\n       # returns version information
        > 1;W;1;43612;\n                    # trigger watchdog
        < 1;W;0;1;17361;\n                  # OK, watchdog state switched from 0 to 1
        > 2;W;1;42529;\n                    # re-trigger watchdog
        < 2;W;1;1;54714;\n                  # OK, watchdog state stayed at 1
        > 3;W;0;48082;\n                    # clear watchdog
        < 3;W;1;0;19933;\n                  # OK, watchdog state switched from 1 to 0
        > 4;W;1;48859;\n                    # re-trigger watchdog
        < 4;W;0;0;52584;\n                  # OK, re-triggering is not possible, watchdog state stayed at 0
        -- reset watchdog now, please --
        > 0;V;5971;\n                       # get version
        < 0;V;1.0_4xUNPULSED;63918;\n       # returns version information
        > 1;W;1;43612;\n                    # trigger watchdog
        < 1;W;0;1;17361;\n                  # OK, watchdog state switched from 0 to 1
        > 2;W;0;1;333;\n                    # simulate communication error
        < 2;E;2;[2;W;0;1;333;];44598;\n     # OK, error responded
        > 2;W;1;42529;\n                    # re-trigger watchdog
        < 2;W;1;1;54714;\n                  # OK, watchdog state stayed at 1
        > 3;S;0;1;22546;\n                  # switch output 0 to ON
        < 3;S;0;0;1;19258;\n                # OK, output 0 was 0 and changed to 1
        > 4;S;1;1;55463;\n                  # switch output 1 to ON
        < 4;S;1;0;1;35812;\n                # OK, output 1 was 0 and changed to 1
        > 5;W;1;47856;\n                    # re-trigger watchdog
        < 5;W;1;1;18868;\n                  # OK, watchdog state stayed at 1
        > 6;R;0;49410;\n                    # read input 0
        < 6;R;0;0;53888;\n                  # OK, input 0 is 0
        -- switch ON input 0 now --
        > 7;R;0;50473;\n                    # read input 0
        < 7;R;0;1;19175;\n                  # OK, input 0 is 1 now
        > 8;S;1;0;64029;\n                  # switch output 1 to OFF again
        < 8;S;1;1;0;22322;\n                # OK, output 1 was 1 and changed to 0


0;V;5971;
1;W;1;43612;
2;S;1;1;1;

*/


void messageHandler_receivedChar(char byte);


#endif
