
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "cartridge.h"

/**
 *
 * Returns error code:
 *  1: Error opening game file.
 *  2: Could not allocate memory for prgRom.
 *  3: Could not allocate memory for chrRom.
 *
 */
int loadCartridge(struct Cartridge **cartridge, char *filename) {
  uint8_t header[16];
  FILE *file;

  int gameFileOpenError = fopen_s(&file, filename, "rb");
  if (gameFileOpenError) {
    print("Error opening game file %s\n", filename);
    return(1);
  }

  fread(header, sizeof(header), 1, file);

  uint8_t numPrgRomUnits = header[4];
  int sizeOfPrgRomInBytes = 16 * 1024 * numPrgRomUnits;
  print("Size of PRG ROM in 16kb units: %d (and in bytes: %d)\n", numPrgRomUnits, sizeOfPrgRomInBytes);

  int sizeOfChrRomInBytes = 8 * 1024 * header[5];
  print("Size of CHR ROM in 8 KB units (Value 0 means the board uses CHR RAM): %d (and in bytes: %d)\n", header[5], sizeOfChrRomInBytes);

  if ((header[7] >> 2) & 3) {
    print("NES 2.0 format\n");
  } else {
    print("Not NES 2.0 format\n");
  }

  if (header[6] & 1) {
    print("Vertical mirroring\n");
  } else {
    print("Horizontal mirroring\n");
  }

  if ((header[6] >> 1) & 1) {
    print("Cartridge contains battery-backed PRG RAM ($6000-7FFF) or other persistent memory\n");
  } else {
    print("Cartridge does not contain battery-packed PRG RAM\n");
  }

  if ((header[6] >> 2) & 1) {
    print("512-byte trainer at $7000-$71FF (stored before PRG data)\n");
  } else {
    print("No 512-byte trainer at $7000-$71FF (stored before PRG data)\n");
  }

  if ((header[6] >> 3) & 1) {
    print("Ignore mirroring control or above mirroring bit; instead provide four-screen VRAM\n");
  } else {
    print("Do not ignore mirroring control or above mirroring bit\n");
  }

  // header[6] bits 4-7 are lower nybble
  // header[7] bits 4-7 are upper nybble
  int mapperNumber = (header[7] & 0xF0) | (header[6] >> 4);
  print("mapper number: %d\n", mapperNumber);

  uint8_t *prgRom = (uint8_t *) malloc(sizeOfPrgRomInBytes);
  if (prgRom == 0) {
    print("Could not allocate memory for prgRom.");
    fclose(file);
    return(2);
  }

  uint8_t *chrRom = (uint8_t *) malloc(sizeOfChrRomInBytes);
  if (chrRom == 0) {
    print("Could not allocate memory for chrRom.");
    free(prgRom);
    fclose(file);
    return(3);
  }

  print("about to read into prgRom, num of bytes: %d\n", sizeOfPrgRomInBytes);
  fread(prgRom, sizeOfPrgRomInBytes, 1, file);
  fread(chrRom, sizeOfChrRomInBytes, 1, file);
  fclose(file);

  // *cartridge is the pointer to the cartridge (cartridge is pointer to the pointer)
  *cartridge = (struct Cartridge *) malloc(sizeof(struct Cartridge));
  memcpy((**cartridge).rawHeader, header, 16 * sizeof(uint8_t));
  (**cartridge).mapperNumber = mapperNumber;
  (**cartridge).prgRom = prgRom;
  (**cartridge).chrRom = chrRom;
  (**cartridge).sizeOfPrgRomInBytes = sizeOfPrgRomInBytes;
  (**cartridge).sizeOfChrRomInBytes = sizeOfChrRomInBytes;
  (**cartridge).numPrgRomUnits = numPrgRomUnits;

  return 0;
}


