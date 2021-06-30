#ifndef FILE_DEBUG_H_SEEN
#define FILE_DEBUG_H_SEEN

#include <stdint.h>

void print(const char *format, ...);
void sprintBitsUint8(char *str, uint8_t val);
void sprintBitsUint16(char *str, uint16_t val);
void printBitsUint8(uint8_t val);

#endif /* !FILE_DEBUG_H_SEEN */
