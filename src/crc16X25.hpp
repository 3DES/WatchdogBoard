#if not defined CRC16_X25_H
#define CRC16_X25_H


#include <stdint.h>
#include "debug.hpp"


enum { eCRC16_X25_INIT = 0xFFFF };


uint16_t crc16X25Step(char data, uint16_t crcSum);
uint16_t crc16X25(char * package, uint16_t length);


#endif