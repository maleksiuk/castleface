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
};

struct PPUClosure
{
  struct PPU *ppu;
  void (*onMemoryWrite)(unsigned int memoryAddress, unsigned char value, struct Computer *state);
};

#endif /* !FILE_PPU_H_SEEN */
