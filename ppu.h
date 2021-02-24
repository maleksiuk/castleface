#ifndef FILE_PPU_H_SEEN
#define FILE_PPU_H_SEEN

#include <stdint.h>

// https://wiki.nesdev.com/w/index.php/PPU_memory_map
struct PPU
{ 
  unsigned char *memory;
  uint8_t *oam; // 256 bytes (64 sprite info chunks, 4 bytes each)

  uint16_t vRegister;  // current vram address; 15 bits
  uint16_t tRegister;  // temporary vram address; 15 bits
  uint8_t xRegister; // fine X scroll; 3 bits
  bool wRegister; // first or second write toggle

  uint16_t patternTableShiftRegisterLow;
  uint16_t patternTableShiftRegisterHigh;
  uint8_t attributeTableShiftRegisterHigh;
  uint8_t attributeTableShiftRegisterLow;
  uint8_t paletteNumber;  // TODO: temporary

  uint8_t nt;
  uint8_t at;
  uint8_t ptTileLow;
  uint8_t ptTileHigh;

  // https://wiki.nesdev.com/w/index.php/PPU_registers
  unsigned char control; // mapped to CPU address $2000
  uint8_t mask;    // mapped to CPU address $2001 
  unsigned char status;  // mapped to CPU address $2002
  uint8_t oamAddr;       // mapped to CPU address $2003

  int scanlineClockCycle;  // 0 to 340
  int scanline; // 262 per frame; each lasts for 341 PPU clock cycles; -1 to 260

  bool debuggingOn;
};

struct PPUClosure
{
  struct PPU *ppu;
  bool (*onMemoryWrite)(unsigned int memoryAddress, unsigned char value, struct Computer *state);
  unsigned char (*onMemoryRead)(unsigned int memoryAddress, struct Computer *state, bool *shouldOverride);
};

#endif /* !FILE_PPU_H_SEEN */
