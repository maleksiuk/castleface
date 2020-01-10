#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"

// the 6502 has 256 byte pages

// might be helpful for PPU implementation:
// - http://nesdev.com/NESDoc.pdf
// - https://forums.nesdev.com/viewtopic.php?p=157086&sid=67b5e4517ef101b69e0c9d1286eeda16#p157086 
// - https://forums.nesdev.com/viewtopic.php?p=157167#p157167
// - http://nesdev.com/NES%20emulator%20development%20guide.txt
// - https://www.reddit.com/r/EmuDev/comments/7k08b9/not_sure_where_to_start_with_the_nes_ppu/drapgie/
// - https://www.dustmop.io/blog/2015/12/18/nes-graphics-part-3/

void printPatternTile(int index, unsigned char *chrRom)
{
  int address = 0x1000 + index*16;

  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
      int bit = 7 - col;

      unsigned char bit1 = (chrRom[address+8+row] >> bit & 0x01) != 0;
      unsigned char bit0 = (chrRom[address+row] >> bit & 0x01) != 0;

      printf("%d", bit1 << 1 | bit0);
    }
    printf("\n");
  }
}

int main(int argc, char **argv) 
{
  printf("hi there\n");

  unsigned char header[16];
  FILE *file;

  unsigned char test;
  printf("sizeof unsigned char: %lu\n", sizeof(test));

  file = fopen("donkey_kong.nes", "rb");

  fread(header, sizeof(header), 1, file);

  int sizeOfPrgRomInBytes = 16 * 1024 * header[4];
  printf("Size of PRG ROM in 16kb units: %d (and in bytes: %d)\n", header[4], sizeOfPrgRomInBytes);

  int sizeOfChrRomInBytes = 8 * 1024 * header[5];
  printf("Size of CHR ROM in 8 KB units (Value 0 means the board uses CHR RAM): %d (and in bytes: %d)\n", header[5], sizeOfChrRomInBytes);

  if ((header[7]&0x0C)==0x08) {
    printf("NES 2.0 format\n");
  } else {
    printf("Not NES 2.0 format\n");
  }

  if ((header[6] & 0x01) == 0x01) {
    printf("Vertical mirroring\n");
  } else {
    printf("Horizontal mirroring\n");
  }

  if ((header[6] & 2) == 2) {
    printf("Cartridge contains battery-backed PRG RAM ($6000-7FFF) or other persistent memory\n");
  } else {
    printf("Cartridge does not contain battery-packed PRG RAM\n");
  }

  if ((header[6] & 4) == 4) {
    printf("512-byte trainer at $7000-$71FF (stored before PRG data)\n");
  } else {
    printf("No 512-byte trainer at $7000-$71FF (stored before PRG data)\n");
  }

  if ((header[6] & 8) == 8) {
    printf("Ignore mirroring control or above mirroring bit; instead provide four-screen VRAM\n");
  } else {
    printf("Do not ignore mirroring control or above mirroring bit\n");
  }

  int mapperNumber = header[7] << 8 | header[6];
  printf("mapper number: %d\n", mapperNumber);

  // TODO: check return value of malloc
  unsigned char *memory;
  memory = (unsigned char *) malloc(0xFFFF);

  unsigned char *ppuMemory;
  ppuMemory = (unsigned char *) malloc(0x4000);

  unsigned char *prgRom;
  prgRom = (unsigned char *) malloc(sizeOfPrgRomInBytes);
  if (prgRom == 0) 
  {
    printf("Could not allocate memory for prgRom.");
    // TODO: free the other memory I've allocated?
    return(0);
  }

  unsigned char *chrRom;
  chrRom = (unsigned char *) malloc(sizeOfChrRomInBytes);
  if (chrRom == 0)
  {
    printf("Could not allocate memory for chrRom.");
    return(0);
  }

  printf("about to read into prgRom, num of bytes: %d\n", sizeOfPrgRomInBytes);
  fread(prgRom, sizeOfPrgRomInBytes, 1, file);
  fread(chrRom, sizeOfChrRomInBytes, 1, file);
  fclose(file);

  // NOTE to self: This isn't working because 0x2000+ isn't within range of chrrom!
  // chrRom is only 0x2000 bytes long for Donkey Kong.
  /*
  printf("name table:\n");
  int baseNametableAddress = 0x2000; // because of value read from $2000 (bits 0 and 1); but this will change over time I bet
  for (int row = 0; row < 30; row++)
  {
    for (int col = 0; col < 32; col++)
    {
      int addressOfNametableEntry = baseNametableAddress + row*32 + col;
      int indexIntoPatternTable = chrRom[addressOfNametableEntry];
      printf("%02x ", indexIntoPatternTable);
      // printf("%04x(%02x) ", addressOfNametableEntry, indexIntoPatternTable);
    }
    printf("\n");
  }

  printPatternTile(0x25, chrRom);
  printf("\n");
  printPatternTile(0xa0, chrRom);
  printf("\n");
  printPatternTile(0xd8, chrRom);
  printf("\n");
  printPatternTile(0x9d, chrRom);
  printf("\n");
  */
  
  
  // https://wiki.nesdev.com/w/index.php/PPU_pattern_tables
  printf("chr rom from $1000 to $1FFF (background pattern table):\n");
  int tileNum = 0;
  for (int i = 0x1000; i < 0x1FFF; i+=16)
  {
    /*
    printf("tile %d\n", tileNum);
    printf("%02x %02x %02x %02x %02x %02x %02x %02x\n", chrRom[i], chrRom[i+1], chrRom[i+2], chrRom[i+3], chrRom[i+4], chrRom[i+5], chrRom[i+6], chrRom[i+7]);
    printf("%02x %02x %02x %02x %02x %02x %02x %02x", chrRom[i+8], chrRom[i+9], chrRom[i+10], chrRom[i+11], chrRom[i+12], chrRom[i+13], chrRom[i+14], chrRom[i+15]);
    printf("\n\n");
    */

    /*
    for (int row = 0; row < 8; row++)
    {
      for (int col = 0; col < 8; col++)
      {
        int bit = 7 - col;

        unsigned char bit1 = (chrRom[i+8+row] >> bit & 0x01) != 0;
        unsigned char bit0 = (chrRom[i+row] >> bit & 0x01) != 0;
        // chrRom[i+8+row].bit(bit) << 1 | chrRom[i+row].bit(bit);

        printf("%d", bit1 << 1 | bit0);
      }
      printf("\n");
    }
    */

    tileNum++;
  }

  printf("prgRom: %x %x %x %x\n", prgRom[0], prgRom[1], prgRom[2], prgRom[3]);
  printf("chrRom: %x %x %x %x\n", chrRom[0], chrRom[1], chrRom[2], chrRom[3]);

  // printf("1: %d, 2: %d, 3: %d, 4: %d\n", &(memory[0x8000]), memory + 0x8000, &(memory[0xC000]));
  printf("memory[0x8000]: %x  memory: %p  &memory[0x8000]: %p\n", memory[0x8000], memory, &memory[0x8000]);

  // put prgRom into memory at $8000-$BFFF and $C000-$FFFF   https://wiki.nesdev.com/w/index.php/NROM
  // TODO: this shouldn't be a copy, it should be a mirror. It's ROM so it shouldn't matter though.
  memcpy(&memory[0x8000], prgRom, 0x4000);
  memcpy(&memory[0xC000], prgRom, 0x4000);

  memcpy(&ppuMemory[0], chrRom, 0x2000);

  printf("memory[0x8000]: %x %x %x %x\n", memory[0x8000], memory[0x8001], memory[0x8002], memory[0x8003]);
  printf("memory[0xC000]: %x %x %x %x\n", memory[0xC000], memory[0xC001], memory[0xC002], memory[0xC003]);

  unsigned char instr = 0;

  int instructionsExecuted = 0;

  printf("last few bytes of prgRom: %x %x %x %x\n", prgRom[0x3FFD], prgRom[0x3FFE], prgRom[0x3FFF], prgRom[0x4000]);

  printf("memory address to start is: %02x%02x\n", memory[0xFFFD], memory[0xFFFC]);
  int memoryAddressToStartAt = (memory[0xFFFD] << 8) | memory[0xFFFC];

  struct Computer state = { memory, 0, 0, 0, 0, 0, 0, 0, 0 };


  // Donkey Kong is setting 2000 (first ppu reg) to 00010000, indicating that the background table address is $1000 (in the CHR ROM I believe)
  //    https://wiki.nesdev.com/w/index.php/PPU_pattern_tables
  //    https://wiki.nesdev.com/w/index.php/PPU_registers#Controller_.28.242000.29_.3E_write

  // Donkey Kong is waiting for 2002 to have the 7th bit set (i.e. "vertical blank has started")

  printf("\n\nexecution:\n");
  for (state.pc = memoryAddressToStartAt; state.pc < 0xFFFF;)
  {
    instr = state.memory[state.pc];

    executeInstruction(instr, &state);

    printf("PPU registers: %02x %02x %02x %02x %02x %02x %02x %02x\n", state.memory[0x2000], state.memory[0x2001], state.memory[0x2002], state.memory[0x2003], state.memory[0x2004], state.memory[0x2005], state.memory[0x2006], state.memory[0x2007]);

    instructionsExecuted++;

    if (instructionsExecuted > 30) {
      printf("stopping because of instruction limit");
      break;
    }


    /*
    // pretend vertical blank has started just to see what happens next
    if (instructionsExecuted == 5)
    {
      state.memory[0x2002] = 0x80; 
    }
    */
  }

  free(memory);
  free(ppuMemory);
  free(prgRom);
  free(chrRom);

  return(0);
}




