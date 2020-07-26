#ifndef FILE_PPU_H_SEEN
#define FILE_PPU_H_SEEN

// https://wiki.nesdev.com/w/index.php/PPU_memory_map
struct PPU
{ 
  unsigned char *memory;
  unsigned int ppuAddr;

  unsigned char ppuAddrGateHigh;
  unsigned char ppuAddrGateLow;
  bool ppuAddrSetLow;

  // https://wiki.nesdev.com/w/index.php/PPU_registers
  unsigned char control; // mapped to CPU address $2000
  unsigned char mask;    // mapped to CPU address $2001 
  unsigned char status;  // mapped to CPU address $2002

  int scanlineClockCycle;  // 0 to 340
  int scanline; // 262 per frame; each lasts for 341 PPU clock cycles; -1 to 260
};

struct PPUClosure
{
  struct PPU *ppu;
  void (*onMemoryWrite)(unsigned int memoryAddress, unsigned char value, struct Computer *state);
  unsigned char (*onMemoryRead)(unsigned int memoryAddress, struct Computer *state, bool *shouldOverride);
};

#endif /* !FILE_PPU_H_SEEN */
