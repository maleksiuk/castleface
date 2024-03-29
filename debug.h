#ifndef FILE_DEBUG_H_SEEN
#define FILE_DEBUG_H_SEEN

#include <stdint.h>
#include <stdio.h>

void print(const char *format, ...);
void sprintBitsUint8(char *str, uint8_t val);
void sprintBitsUint16(char *str, uint16_t val);
void printBitsUint8(uint8_t val);
void dumpOam(int num, uint8_t *oam);

#ifndef _WIN32
void OutputDebugStringA(char *str);
int fopen_s(FILE **f, const char *name, const char *mode);
#endif

#endif /* !FILE_DEBUG_H_SEEN */
