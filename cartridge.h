#ifndef FILE_CARTRIDGE_H_SEEN
#define FILE_CARTRIDGE_H_SEEN

#include <stdint.h>

struct Cartridge {
  uint8_t rawHeader[16];
  int mapperNumber;
  uint8_t *prgRom;
  uint8_t *chrRom;
  int sizeOfPrgRomInBytes;
  int sizeOfChrRomInBytes;
  uint8_t numPrgRomUnits;
};

int loadCartridge(struct Cartridge **cartridge, const char *filename);

#endif /* !FILE_CARTRIDGE_H_SEEN */
