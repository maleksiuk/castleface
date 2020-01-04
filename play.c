#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the 6502 has 256 byte pages


int main(int argc, char **argv) 
{
  printf("hi there\n");

  // TODO: read header into its own space, then prg rom into another space
  unsigned char header[16];
  FILE *file;

  unsigned char test;
  printf("sizeof unsigned char: %lu\n", sizeof(test));

  file = fopen("donkey_kong.nes", "rb");

  fread(header, sizeof(header), 1, file);

  int sizeOfPrgRomInBytes = 16 * 1024 * header[4];
  printf("Size of PRG ROM in 16kb units: %d (and in bytes: %d)\n", header[4], sizeOfPrgRomInBytes);

  printf("Size of CHR ROM in 8 KB units (Value 0 means the board uses CHR RAM): %d\n", header[5]);

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

  unsigned char *prgRom;
  prgRom = (unsigned char *) malloc(sizeOfPrgRomInBytes);
  if (prgRom == 0) 
  {
    printf("Could not allocate memory for prgRom.");
    // TODO: free the other memory I've allocated0
    return(0);
  }

  printf("about to read into prgRom, num of bytes: %d\n", sizeOfPrgRomInBytes);
  fread(prgRom, sizeOfPrgRomInBytes, 1, file);
  fclose(file);

  /*for (int i = 0; i < sizeOfPrgRomInBytes; i++)*/
  /*{*/
    /*printf("%02x ", prgRom[i]);*/
  /*}*/

  printf("prgRom: %x %x %x %x\n", prgRom[0], prgRom[1], prgRom[2], prgRom[3]);

  // printf("1: %d, 2: %d, 3: %d, 4: %d\n", &(memory[0x8000]), memory + 0x8000, &(memory[0xC000]));
  printf("memory[0x8000]: %x  memory: %p  &memory[0x8000]: %p\n", memory[0x8000], memory, &memory[0x8000]);

  // put prgRom into memory at $8000-$BFFF and $C000-$FFFF   https://wiki.nesdev.com/w/index.php/NROM
  memcpy(&memory[0x8000], prgRom, 0x4000);
  memcpy(&memory[0xC000], prgRom, 0x4000);

  printf("memory[0x8000]: %x %x %x %x\n", memory[0x8000], memory[0x8001], memory[0x8002], memory[0x8003]);
  printf("memory[0xC000]: %x %x %x %x\n", memory[0xC000], memory[0xC001], memory[0xC002], memory[0xC003]);

  unsigned char instr = 0;

  int instructionsExecuted = 0;

  printf("last few bytes of prgRom: %x %x %x %x\n", prgRom[0x3FFD], prgRom[0x3FFE], prgRom[0x3FFF], prgRom[0x4000]);

  // 65532 and 65533 have the 16 bit reset address apparently (FFFC-FFFD)

  printf("memory address to start at might be: %02x%02x\n", memory[0xFFFD], memory[0xFFFC]);
  int memoryAddressToStartAt = (memory[0xFFFD] << 8) | memory[0xFFFC];

  printf("\n\nexecution:\n");
  /*sizeOfPrgRomInBytes = 50000; // TODO: tmp*/
  for (int pc = memoryAddressToStartAt; pc < 0xFFFF;)
  {
    int i = pc;

    // printf("byte %i: %x\n", i, buffer[i]);
    printf("bytes %d to %d: %02x %02x %02x\n", pc, pc+2, memory[i], memory[i+1], memory[i+2]);

    instr = memory[i];

    instructionsExecuted++;
    if (instructionsExecuted > 10) {
      return(0);
    }

    pc++;
  }

  free(memory);
  free(prgRom);

  return(0);
}




