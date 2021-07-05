#include <stdlib.h>
#include <string.h>
#include "ppu.h"
#include "cartridge.h"

// Currently the PPU code is almost all in emu.c

/**
 * 
 * Errors:
 * 1: Could not allocate PPU memory.
 * 2: Could not allocate OAM memory.
 *
 */
int createPPU(struct PPU **ppu, struct Cartridge *cartridge) {
  uint8_t *ppuMemory = (unsigned char *) calloc(1, 0x3FFF);
  if (!ppuMemory) {
    return 1;
  }

  uint8_t *oam = (uint8_t *) calloc(256, sizeof(uint8_t));
  if (!oam) {
    free(ppuMemory);
    return 2;
  }

  // TODO: should I just make ppuMemory point to chrRom?
  memcpy(ppuMemory, cartridge->chrRom, cartridge->sizeOfChrRomInBytes);

  *ppu = (struct PPU*) calloc(1, sizeof(struct PPU));
  (**ppu).memory = ppuMemory;
  (**ppu).oam = oam;
  (**ppu).scanline = -1;
  (**ppu).mapperNumber = cartridge->mapperNumber;
  (**ppu).sprites = (**ppu).sprites0;
  (**ppu).followingSprites = (**ppu).sprites1;
  (**ppu).wRegister = false;

  return 0;
}

