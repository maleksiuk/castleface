#include <stdio.h>
#include <stdlib.h>

// the 6502 has 256 byte pages


int main(int argc, char **argv) 
{
  printf("hi there\n");

  // TODO: read header into its own space, then prg rom into another space
  unsigned char buffer[262160];
  FILE *file;

  unsigned char test;
  printf("sizeof unsigned char: %lu\n", sizeof(test));

  file = fopen("donkey_kong.nes", "rb");

  fread(buffer, sizeof(buffer), 1, file);

  printf("Size of PRG ROM in 16kb units: %d\n", buffer[4]);

  int sizeOfPrgRomInBytes = 16 * 1024 * buffer[4];

  printf("Size of CHR ROM in 8 KB units (Value 0 means the board uses CHR RAM): %d\n", buffer[5]);

  if ((buffer[7]&0x0C)==0x08) {
    printf("NES 2.0 format\n");
  } else {
    printf("Not NES 2.0 format\n");
  }

  if ((buffer[6] & 0x01) == 0x01) {
    printf("Vertical mirroring\n");
  } else {
    printf("Horizontal mirroring\n");
  }

  if ((buffer[6] & 2) == 2) {
    printf("Cartridge contains battery-backed PRG RAM ($6000-7FFF) or other persistent memory\n");
  } else {
    printf("Cartridge does not contain battery-packed PRG RAM\n");
  }

  if ((buffer[6] & 4) == 4) {
    printf("512-byte trainer at $7000-$71FF (stored before PRG data)\n");
  } else {
    printf("No 512-byte trainer at $7000-$71FF (stored before PRG data)\n");
  }

  if ((buffer[6] & 8) == 8) {
    printf("Ignore mirroring control or above mirroring bit; instead provide four-screen VRAM\n");
  } else {
    printf("Do not ignore mirroring control or above mirroring bit\n");
  }

  int mapperNumber = buffer[7] << 8 | buffer[6];
  printf("mapper number: %d\n", mapperNumber);

  unsigned char *memory;
  memory = (unsigned char *) malloc(8192);

  unsigned char instr = 0;

  unsigned char acc = 0;
  unsigned char z = 0;   // zero flag
  unsigned char n = 0;   // negative flag

  unsigned int *stack;
  stack = (unsigned int *) malloc(100);
  int stackLastIndex = 0;

  int instructionsExecuted = 0;

  // 65532 and 65533 have the 16 bit reset address apparently

  printf("memory address to start at might be: %02x%02x\n", buffer[65533 + 16], buffer[65532 + 16]);
  int memoryAddressToStartAt = (buffer[65533 + 16] << 8) | buffer[65532 + 16];

  printf("\n\nexecution:\n");
  /*sizeOfPrgRomInBytes = 50000; // TODO: tmp*/
  for (int pc = memoryAddressToStartAt; pc < sizeOfPrgRomInBytes;)
  {
    int i = pc + 16;

    // printf("byte %i: %x\n", i, buffer[i]);
    printf("bytes %d to %d: %02x %02x %02x\n", pc, pc+2, buffer[i], buffer[i+1], buffer[i+2]);

    instr = buffer[i];

    // LDA; Absolute; Len 3; Time 4
    if (instr == 0xAD)
    {
      unsigned int memoryAddress = (buffer[i+2] << 8) | buffer[i+1];
      acc = memory[memoryAddress];
      if (acc == 0) {
        z = 1;
      }
      // TODO: set n flag
      printf("-> LDA %02x%02x -> acc = %d\n", buffer[i+2], buffer[i+1], acc);
      pc += 3;
    } 
    // LDA; Immediate; Len 2; Time 2
    else if (instr == 0xA9)
    {
      acc = buffer[i+1];
      if (acc == 0) {
        z = 1;
      }
      // TODO: set n flag
      printf("-> LDA #%02x -> acc = %d\n", acc, acc);
      pc += 2;
    }
    // BNE; Branch on not equal; Len 2
    else if (instr == 0xD0)
    {
      signed char relativeDisplacement = buffer[i+1] + 2;  // adjust by 2 to make up for the two bytes this instruction takes
      if (z == 0) {
        printf("-> BNE %02x -> jump forward by %d\n", relativeDisplacement, relativeDisplacement);
        pc += relativeDisplacement;
      } else {
        printf("-> BNE %02x -> did not jump forward by %d\n", relativeDisplacement, relativeDisplacement);
        pc += 2;
      }
    }
    // JSR; Jump to subroutine; Len 3; Time 6
    else if (instr == 0x20)
    {
      stackLastIndex += 1;
      stack[stackLastIndex] = pc + 2;  // save next operation pc minus one

      unsigned int memoryAddress = (buffer[i+2] << 8) | buffer[i+1];

      printf("-> JSR to memory address: %x\n", memoryAddress);
      
      pc = memoryAddress; 

    } else {
      pc++;
    }

    instructionsExecuted++;

    if (instructionsExecuted > 10) {
      return(0);
    }
  }

  // file size: 262160
  // minus 16 byte header: 262144
  //
  // 131072 in PRG ROM
  // 131072 in CHR ROM
  // adds up to 262144!
  // so the file is made up of these two chunks

  free(memory);
  free(stack);

  fclose(file);

  return(0);
}




