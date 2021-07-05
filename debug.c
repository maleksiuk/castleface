#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
  void OutputDebugStringA(LPCSTR);
#else
  void OutputDebugStringA(char *str) {
    printf("%s", str);
  }

  #define sprintf_s(buf, ...) snprintf((buf), sizeof(buf), __VA_ARGS__)
#endif

void print(const char *format, ...) {
  va_list arguments;
  va_start(arguments, format);

  char str[200];
  vsprintf_s(str, 200, format, arguments);
  va_end(arguments);

  OutputDebugStringA(str);
}

void sprintBitsUint8(char *str, uint8_t val) {
  for (int i = 7; 0 <= i; i--) {
    sprintf_s(str + strlen(str), 2, "%c", (val & (1 << i)) ? '1' : '0');
  }
}

void sprintBitsUint16(char *str, uint16_t val) {
  for (int i = 15; 0 <= i; i--) {
    sprintf_s(str + strlen(str), 2, "%c", (val & (1 << i)) ? '1' : '0');
  }
}

void printBitsUint8(uint8_t val) {
  for (int i = 7; 0 <= i; i--) {
    print("%c", (val & (1 << i)) ? '1' : '0');
  }
  print("\n");
}

void dumpOam(int num, uint8_t *oam)
{
  FILE *file;
  char filename[30];
  sprintf_s(filename, 30, "oam-%d.dump", num);
  int error = fopen_s(&file, filename, "w+");
  if (error) {
    print("Error opening file %s\n", filename);
    exit(error);
  }

  for (int i = 0; i < 256; i+=4) {
    fprintf(file, "ypos: %02x, tileindex: %02x, attr: %02x, xpos: %02x\n", oam[i], oam[i+1], oam[i+2], oam[i+3]);
  }
  print("done dumping oam to %s\n", filename);

  fclose(file);
}


