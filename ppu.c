#include <stdlib.h>
#include <string.h>
#include "ppu.h"
#include "cartridge.h"

// Currently the PPU code is almost all in emu.c

void loadPalette(struct Color *palette) {
  // colours from http://www.thealmightyguru.com/Games/Hacking/Wiki/index.php/NES_Palette
  // TODO: read from a PAL file?
  struct Color colors[] = {
    {124,124,124},{0,0,252},{0,0,188},{68,40,188},{148,0,132},{168,0,32},{168,16,0},{136,20,0},
    {80,48,0},{0,120,0},{0,104,0},{0,88,0},{0,64,88},{0,0,0},{0,0,0},{0,0,0},
    {188,188,188},{0,120,248},{0,88,248},{104,68,252},{216,0,204},{228,0,88},{248,56,0},{228,92,16},
    {172,124,0},{0,184,0},{0,168,0},{0,168,68},{0,136,136},{0,0,0},{0,0,0},{0,0,0},
    {248,248,248},{60,188,252},{104,136,252},{152,120,248},{248,120,248},{248,88,152},{248,120,88},{252,160,68},
    {248,184,0},{184,248,24},{88,216,84},{88,248,152},{0,232,216},{120,120,120},{0,0,0},{0,0,0},
    {252,252,252},{164,228,252},{184,184,248},{216,184,248},{248,184,248},{248,164,192},{240,208,176},{252,224,168},
    {248,216,120},{216,248,120},{184,248,184},{184,248,216},{0,252,252},{248,216,248},{0,0,0},{0,0,0}
  };

  for (int i = 0; i < 64; i++) {
    palette[i] = colors[i];
  }
}

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

  // TODO: check if calloc succeeded
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

