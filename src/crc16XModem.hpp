#if not defined CRC16_XMODEM_H
#define CRC16_XMODEM_H


#include <stdint.h>
#include "debug.hpp"


enum { eCRC16_XMODEM_INIT = 0 };


uint16_t crc16XModemStep(char data, uint16_t crcSum);
uint16_t crc16XModem(char * package, uint16_t length);


#endif