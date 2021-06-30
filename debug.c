#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// TODO: How do I make this use the windows.h definition when appropriate and this definition otherwise?
void OutputDebugString(char *str) {
}

void print(const char *format, ...) {
  va_list arguments;
  va_start(arguments, format);

  char str[200];
  vsprintf(str, format, arguments);
  va_end(arguments);

  printf("%s", str);
  OutputDebugString(str);
}

void sprintBitsUint8(char *str, uint8_t val) {
  for (int i = 7; 0 <= i; i--) {
    sprintf(str + strlen(str), "%c", (val & (1 << i)) ? '1' : '0');
  }
}

void sprintBitsUint16(char *str, uint16_t val) {
  for (int i = 15; 0 <= i; i--) {
    sprintf(str + strlen(str), "%c", (val & (1 << i)) ? '1' : '0');
  }
}

void printBitsUint8(uint8_t val) {
  for (int i = 7; 0 <= i; i--) {
    print("%c", (val & (1 << i)) ? '1' : '0');
  }
  print("\n");
}

