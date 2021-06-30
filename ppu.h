#ifndef FILE_PPU_H_SEEN
#define FILE_PPU_H_SEEN

#include <stdint.h>
#include <stdbool.h>

#define VIDEO_BUFFER_WIDTH 256
#define VIDEO_BUFFER_HEIGHT 240
#define STARTING_PIXEL 2

struct Computer;

struct Color 
{
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

struct Sprite
{
  uint8_t yPosition;
  uint8_t tileIndex;
  uint8_t attributes;
  uint8_t xPosition;
  uint8_t spriteIndex;
};

// https://wiki.nesdev.com/w/index.php/PPU_memory_map
struct PPU
{ 
  unsigned char *memory;
  uint8_t *oam; // 256 bytes (64 sprite info chunks, 4 bytes each)

  struct Sprite sprites0[8];
  struct Sprite sprites1[8];
  struct Sprite *sprites;
  struct Sprite *followingSprites;

  uint16_t vRegister;  // current vram address; 15 bits
  uint16_t tRegister;  // temporary vram address; 15 bits
  uint8_t xRegister; // fine X scroll; 3 bits
  bool wRegister; // first or second write toggle

  uint16_t patternTableShiftRegisterLow;
  uint16_t patternTableShiftRegisterHigh;
  uint8_t paletteNumberFirst;
  uint8_t paletteNumberSecond;

  uint16_t nt;
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

  int mapperNumber;

  bool debuggingOn;
};

struct PPUClosure
{
  struct PPU *ppu;
  bool (*onMemoryWrite)(unsigned int memoryAddress, unsigned char value, struct Computer *state);
  unsigned char (*onMemoryRead)(unsigned int memoryAddress, struct Computer *state, bool *shouldOverride);
};



#endif /* !FILE_PPU_H_SEEN */
