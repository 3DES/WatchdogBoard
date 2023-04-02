#if not defined CRC16_X25_H
#define CRC16_X25_H


#include <stdint.h>
#include "debug.hpp"


enum { eCRC16_X25_INIT = 0xFFFF };

// CRC16 X25 XORs the final result with 0xFFFF
static inline uint16_t crc16X25Xor(uint16_t crcSum)
{
    return crcSum ^ 0xFFFF;
}

uint16_t crc16X25Step(char data, uint16_t crcSum);
uint16_t crc16X25(char * package, uint16_t length);

#endif