
#include "emu.h"
#include "cpu.h"
#include <stdint.h>
#include "controller.h"

void setPPUData(unsigned char value, struct PPU *ppu, int inc, struct Computer *state) 
{
  if (ppu->vRegister > 0x3FFF) {
    print("trying to write out of ppu bounds!\n");
  } else {
    if (ppu->debuggingOn && ppu->vRegister == 0x3F00) {
      print("writing %02x to $3F00; cpu pc: %d\n", value, state->pc);
    }
    ppu->memory[ppu->vRegister] = value;

    // TODO: consider implementing these mirrors as a read, not a write
    // Addresses $3F10/$3F14/$3F18/$3F1C are mirrors of $3F00/$3F04/$3F08/$3F0C
    if (ppu->vRegister >= 0x3F10 && ppu->vRegister <= 0x3F1C && ppu->vRegister % 4 == 0) {
      ppu->memory[ppu->vRegister - 0x0010] = value;
    }
  }

  ppu->vRegister = ppu->vRegister + inc;
}

void setButton(struct Computer *state, bool isButtonPressed, uint8_t position) {
  if (isButtonPressed) {
    state->buttons = state->buttons | (1 << position);
  }
}

// TODO: I'm not so sure I want this to be the final mechanism to handle PPU/CPU communication.
bool onCPUMemoryWrite(unsigned int memoryAddress, unsigned char value, struct Computer *state) 
{
  bool shouldWriteMemory = true;
  struct PPU *ppu = state->ppuClosure->ppu;

  // TODO: consider using a table of function pointers
  // TODO: make constants for these memory addresses
  if (memoryAddress == 0x2006) {  // PPUADDR
    char val[9] = "";
    sprintBitsUint8(val, value);

    if (!ppu->wRegister) { // first write
      /*
       t: .FEDCBA ........ = d: ..FEDCBA
       t: X...... ........ = 0
      */
      char was[17] = "";
      char is[17] = "";
      sprintBitsUint16(was, ppu->tRegister);

      uint8_t firstSixBits = value & 0x3F;
      ppu->tRegister = ppu->tRegister & ~0xFF00; // clear the last 8 bits
      ppu->tRegister = ppu->tRegister | (firstSixBits << 8);

      sprintBitsUint16(is, ppu->tRegister);
      /*print("[0x2006 1st write] [val = %s] t from %s to %s\n", val, was, is);*/
    } else {  // second write
      /*
       t: ....... HGFEDCBA = d: HGFEDCBA
       v                   = t
      */
      char was[17] = "";
      char is[17] = "";
      sprintBitsUint16(was, ppu->tRegister);

      ppu->tRegister = ppu->tRegister & ~0x00FF; // clear the first 8 bits
      ppu->tRegister = ppu->tRegister | value; 
      sprintBitsUint16(is, ppu->tRegister);
      /*print("[0x2006 2nd write] [val = %s] t from %s to %s\n", val, was, is);*/
      ppu->vRegister = ppu->tRegister; 
    }

    ppu->wRegister = !ppu->wRegister;
    shouldWriteMemory = false;
  } else if (memoryAddress == 0x2007) {
    setPPUData(value, ppu, vramIncrement(ppu), state);
    shouldWriteMemory = false;
  } else if (memoryAddress == 0x2001) {
    uint8_t before = ppu->mask;
    uint8_t after = value;
    uint8_t visibilityBefore = (before & 0x18) >> 3;
    uint8_t visibilityAfter = (after & 0x18) >> 3;

    /*
    if (visibilityBefore != visibilityAfter) {
      print("MASK! %02x => %02x\n", visibilityBefore, visibilityAfter);
    }
    */
    ppu->mask = value;
    shouldWriteMemory = false;
  } else if (memoryAddress == 0x2000) {  // PPUCTRL
    /*print("*************** PPUCTRL 0x2000 write: %02x\n", value);*/
    ppu->control = value;

    // set ppu->tRegister 11th and 10th bits to the 1st and 0th bits of value (nametable choice)
    uint8_t nametable = value & 0x03;
    ppu->tRegister = ppu->tRegister & ~(0x03 << 10);  // clear 11th and 10th bits
    ppu->tRegister = ppu->tRegister | (nametable << 10);

    shouldWriteMemory = false;
  } else if (memoryAddress == 0x2005) {  // PPUSCROLL
    /*print("*************** SCROLL 0x2005 write: %02x\n", value);*/

    if (!ppu->wRegister) { // first write
     /*
       t: ....... ...HGFED = d: HGFED...
       x:              CBA = d: .....CBA
     */
      ppu->tRegister = ppu->tRegister & ~0x1F;  // clear first five bits
      ppu->tRegister = ppu->tRegister | (value >> 3);

      // set x to the first three bits of value
      ppu->xRegister = value & 0x07; 
    } else { // second write
      // t: CBA..HG FED..... = d: HGFEDCBA
      uint8_t abc = value & 0x07;
      uint8_t defgh = value & ~0x07;

      ppu->tRegister = ppu->tRegister & ~0x73E0; // clear the relevant bits
      ppu->tRegister = ppu->tRegister | (abc << 12);
      ppu->tRegister = ppu->tRegister | (defgh << 5);
    }
    ppu->wRegister = !ppu->wRegister;
    shouldWriteMemory = false;
  } else if (memoryAddress == 0x2003) {
    ppu->oamAddr = value;
    shouldWriteMemory = false;
  } else if (memoryAddress == 0x2004) {
    // oamdata write
    shouldWriteMemory = false;
  } else if (memoryAddress == 0x4014) {
    int cpuAddr = value << 8;
    int numBytes = 256 - ppu->oamAddr;
    /*print("[OAM] OAMDMA write. Will get data from CPU memory page %02x (addr: %04x). Oam addr is %02x. Num bytes: %d\n", value, cpuAddr, ppu->oamAddr, numBytes);*/
    // TODO: I should probably write a getMemoryAddress method that translates the cpuAddr to the mapped address
    memcpy(&ppu->oam[ppu->oamAddr], &state->memory[cpuAddr], numBytes);
    /*dumpOam(1, ppu->oam);*/
    shouldWriteMemory = false;
  } else if (memoryAddress == 0x4016) {
    /*print("************ write to 0x4016: %02x\n", value);*/
    state->pollController = (value == 1);
    if (state->pollController) {
      // copy keyboard input into buttons
      state->buttons = 0; 
      setButton(state, state->keyboardInput->up, 4); 
      setButton(state, state->keyboardInput->down, 5); 
      setButton(state, state->keyboardInput->left, 6); 
      setButton(state, state->keyboardInput->right, 7); 
      setButton(state, state->keyboardInput->select, 2); 
      setButton(state, state->keyboardInput->start, 3); 
      setButton(state, state->keyboardInput->a, 0); 
      setButton(state, state->keyboardInput->b, 1);
    }
    state->currentButtonBit = 0;  // is this right?
    shouldWriteMemory = false;
  } else if (memoryAddress >= 0x8000 && memoryAddress <= 0xFFFF) {
    shouldWriteMemory = false;
    if (ppu->mapperNumber == 1) {
      if (value >= 0x80) {
        print("clear the shift register\n");
        // TODO: I think this might need to reset the mapping so that $C000-FFFF is fixed to the last prg bank
        state->mmc1ShiftRegister = 0x0;
        state->mmc1ShiftCounter = 0;
      } else {
        /*print("do not clear the shift register\n");*/
        // put bit 0 of value in
        uint8_t valueOfBit0 = value & 0x01;
        if (valueOfBit0 == 0) {
          state->mmc1ShiftRegister = state->mmc1ShiftRegister >> 1;
        } else {
          state->mmc1ShiftRegister = state->mmc1ShiftRegister >> 1;
          state->mmc1ShiftRegister += 16;
        }
        /*printBitsUint8(state->mmc1ShiftRegister);*/
        state->mmc1ShiftCounter++;

        if (state->mmc1ShiftCounter == 5) {
          state->mmc1ShiftCounter = 0;

          if (memoryAddress >= 0xE000 && memoryAddress <= 0xFFFF) {
            /*print("%04x, storing %02x into the prg bank thing\n", memoryAddress, state->mmc1ShiftRegister);*/
            state->mmc1PrgRomBank = state->mmc1ShiftRegister;

            // four lowest bits determine prg bank
            uint8_t prgBank = state->mmc1PrgRomBank & 0x0F;
            /*print("choosing prg bank %d\n", prgBank);*/

            int addressOfSelectedBank = 0x8000 + (prgBank * 0x4000); 
            state->prgRomBlock1 = &state->memory[addressOfSelectedBank];
            state->prgRomBlock2 = &state->memory[addressOfSelectedBank + 0x2000];
          } else {
            print(">>>>> trying to change a different thing %04x\n", memoryAddress);
          }
        }
      }
    } else {
      print("uih oh\n");
    }
  }

  return shouldWriteMemory;
}

unsigned char onCPUMemoryRead(unsigned int memoryAddress, struct Computer *state, bool *shouldOverride) {
  if (memoryAddress == 0x2002) { // PPUSTATUS
    struct PPU *ppu = state->ppuClosure->ppu;
    uint8_t status = ppu->status;  // copy the status out so we can return it as it was before clearing flags

    ppu->status = ppu->status & ~0x80;  // clear vblank flag

    ppu->wRegister = false;

    *shouldOverride = true;

    return status; 
  } else if (memoryAddress == 0x2007) {
    struct PPU *ppu = state->ppuClosure->ppu;
    /*print("READING 0x2007 *************************\n\n");*/
    ppu->vRegister = ppu->vRegister + vramIncrement(ppu);
  } else if (memoryAddress == 0x4016) {
    /*print("*********** read from 0x4016 (val is %02x)\n", state->memory[0x4016]);*/
    *shouldOverride = true;
    if (state->pollController) {
      /*print("**** polling the controller, so reading will just return state of first button (A)\n");*/
      // return state of first button (A)
      return state->buttons & 0x01;
    } else {
      unsigned char buttonValue = (state->buttons >> state->currentButtonBit) & 1;
      /*print("**** (buttons: %02x) reading from button bit %d (value is %02x)\n", state->buttons, state->currentButtonBit, buttonValue);*/
      state->currentButtonBit++;
      return buttonValue;
    }
  } else if (memoryAddress >= 0x8000 && memoryAddress <= 0xFFFF) {
    *shouldOverride = true;
    // I want the 32 kB of PRG ROM to be splittable into 8 kB chunks. 
    // 0x8000 to 0x9FFF
    // 0xA000 to 0xBFFF
    // 0xC000 to 0xDFFF
    // 0xE000 to 0xFFFF

    // I want to lay the cartridge memory out in a big chunk, then have
    // four pointers into it. Maybe.
    //
    // When a bank switch happens, we just change the prgRomBlock pointers

    if (memoryAddress >= 0x8000 && memoryAddress <= 0x9FFF) {
      return *(state->prgRomBlock1 + (memoryAddress - 0x8000));
    } else if (memoryAddress >= 0xA000 && memoryAddress <= 0xBFFF) {
      return *(state->prgRomBlock2 + (memoryAddress - 0xA000));
    } else if (memoryAddress >= 0xC000 && memoryAddress <= 0xDFFF) {
      return *(state->prgRomBlock3 + (memoryAddress - 0xC000));
    } else if (memoryAddress >= 0xE000 && memoryAddress <= 0xFFFF) {
      return *(state->prgRomBlock4 + (memoryAddress - 0xE000));
    }

    // for mapper 1, we need to grab from the right spot
    /*print("reading %04x\n", memoryAddress);*/
  }

  *shouldOverride = false;
  return 0;
}

int vramIncrement(struct PPU *ppu) 
{
  if (ppu->control >> 2 & 0x01 == 1) {
    return 32;
  } else {
    return 1;
  }
}

// runs on scanlines 0 to 239; I believe a game programmer has to set their y to 0 w/ an understanding it'll render at y = 1
void spriteEvaluation(struct PPU *ppu) 
{
  int y = ppu->scanline;

  memset(ppu->followingSprites, 0xFF, 8 * sizeof(ppu->followingSprites[0]));

  int curSpriteNum = 0;

  // search through the 64 sprites to see if any affect us
  for (int i = 0; i < 256; i+=4) {
    uint8_t ypos = ppu->oam[i];
    uint8_t yStart = ypos;
    uint8_t yEnd = ypos + 8;

    if (yStart <= y && y < yEnd) {
      if (curSpriteNum < 8) { 
        ppu->followingSprites[curSpriteNum] = (struct Sprite) { .yPosition = ypos, .xPosition = ppu->oam[i+3], 
          .tileIndex = ppu->oam[i+1], .attributes = ppu->oam[i+2], .spriteIndex = i / 4 };
      } else {
        // TODO: set sprite overflow
      }

      curSpriteNum++;
    }
  }
}

void renderSpritePixel(struct PPU *ppu, struct Computer *state, struct Color *palette, uint8_t backgroundVal, void *videoBuffer) 
{
  unsigned char control = ppu->control;

  int spritePatternTableAddress = 0x0000;
  if (control >> 3 == 1) {
    spritePatternTableAddress = 0x1000;
  }

  // find which pixel we're working on, see if that matches a sprite, and then figure out what to render there if it does.

  int pixelX = ppu->scanlineClockCycle - STARTING_PIXEL;
  int pixelY = ppu->scanline;

  for (int i = 0; i < 8; i++) {
    struct Sprite sprite = ppu->sprites[i];
    if (sprite.yPosition == 0xFF) {
      return;
    }

    uint8_t xpos = sprite.xPosition;
    uint8_t xStart = xpos;
    uint8_t xEnd = xpos + 8;

    if (xStart <= pixelX && pixelX < xEnd) {
      // render pixel at pixelX, pixelY
      uint8_t tileIndex = sprite.tileIndex;
      uint8_t attributes = sprite.attributes;
      bool isSpriteZero = sprite.spriteIndex == 0;

      bool flipHorizontally = attributes & 0x40;

      // 16 because the pattern table comes in 16 byte chunks 
      int addressOfSprite = spritePatternTableAddress + (tileIndex * 16);
      uint8_t *spriteData = &ppu->memory[addressOfSprite];

      uint32_t *videoBufferRow = (uint32_t *)videoBuffer;
      videoBufferRow += (pixelY * VIDEO_BUFFER_WIDTH) + pixelX;

      {
        // find out which row of the 8x8 sprite is relevant for us
        int rowOfSprite = pixelY - 1 - sprite.yPosition;

        uint8_t lowByte = spriteData[rowOfSprite];
        uint8_t highByte = spriteData[rowOfSprite+8];

        uint32_t *pixel = videoBufferRow;

        int colOfSprite = pixelX - xpos;
        if (flipHorizontally) {
          colOfSprite = 7 - colOfSprite;
        }

        int bitNumber = 7 - colOfSprite;
        uint8_t bit1 = (highByte >> bitNumber) & 0x01;
        uint8_t bit0 = (lowByte >> bitNumber) & 0x01;
        int val = bit1 << 1 | bit0;

        if (val > 0) {
          // TODO: there are other conditions to handle http://wiki.nesdev.com/w/index.php/PPU_OAM#Sprite_zero_hits
          if (isSpriteZero && backgroundVal > 0) {
            ppu->status = ppu->status | 0x40;  // sprite zero hit!
          }

          int paletteNumber = (attributes & 0x03) + 4;
          uint8_t colorIndex = ppu->memory[0x3F00 + 4*paletteNumber + val];
          struct Color color = palette[colorIndex];
          *pixel = ((color.red << 16) | (color.green << 8) | color.blue);
        }
      }
      
      // TODO: I think we may want to return out of this function here due to sprite priority (only draw first one)
    }
  }
}

bool isRenderingEnabled(struct PPU *ppu) {
  return (ppu->mask & 0x18);
}

uint8_t renderBackgroundPixel2(struct PPU *ppu, struct Computer *state, struct Color *palette, void *videoBuffer) {
  if (isRenderingEnabled(ppu)) {
    /*print("rendering. line %d, cycle %d\n", ppu->scanline, ppu->scanlineClockCycle);*/
  } else {
    return 0;
    /*print("RENDERING BUT IT IS NOT ENABLED line %d, cycle %d\n", ppu->scanline, ppu->scanlineClockCycle);*/
  }
  const int y = ppu->scanline;
  const int x = ppu->scanlineClockCycle;
  uint8_t universalBackgroundColor = ppu->memory[0x3F00];

  uint8_t fineX = ppu->xRegister;

  uint8_t bit1 = ((ppu->patternTableShiftRegisterHigh << fineX) & 0x8000) != 0;
  uint8_t bit0 = ((ppu->patternTableShiftRegisterLow << fineX) & 0x8000) != 0;
  /*print("regs are %04x %04x; bit1: %d, bit0: %d\n", ppu->patternTableShiftRegisterHigh, ppu->patternTableShiftRegisterLow, bit1, bit0);*/

  /*
  ppu->patternTableShiftRegisterLow = ppu->patternTableShiftRegisterLow << 1;
  ppu->patternTableShiftRegisterHigh = ppu->patternTableShiftRegisterHigh << 1;
  */

  uint8_t *videoBufferRow = (uint8_t *)videoBuffer;
  videoBufferRow = videoBufferRow + (y * VIDEO_BUFFER_WIDTH * 4);

  uint32_t *pixel = (uint32_t *)(videoBufferRow);
  pixel += (x - STARTING_PIXEL);

  // TODO: rename val
  int val = bit1 << 1 | bit0;


  /*$3F00 	Universal background color*/
  /*$3F01-$3F03 	Background palette 0*/
  /*$3F05-$3F07 	Background palette 1*/
  /*$3F09-$3F0B 	Background palette 2*/
  /*$3F0D-$3F0F 	Background palette 3 */
  uint8_t paletteNumber = ppu->paletteNumberFirst;
  uint8_t colorIndex = universalBackgroundColor;
  if (val > 0) {
    colorIndex = ppu->memory[0x3F00 + 4*paletteNumber + val];
  }
  struct Color color = palette[colorIndex];

  uint16_t tmpPalAddr = 0x3F00 + 4*paletteNumber + val;

  // tile 0 is 0 to 7, tile 1 is 8 to 15, tile 2 is 16 to 23, etc. So tile 9 starts at 9*8, tile 16 starts at 16*8
  int debugTileX = 17;
  int debugTileY = 13;
  int debugFineY = 2;
  if (ppu->debuggingOn && ppu->scanline == debugTileY*8 + debugFineY && ppu->scanlineClockCycle >= debugTileX*8+STARTING_PIXEL && ppu->scanlineClockCycle <= debugTileX*8+8+STARTING_PIXEL) {
    print("[cycle %d / %d] [fineX: %d] [pal addr %04x, color index %02x, palnum %d] print pixel %02x\n", 
        ppu->scanline, ppu->scanlineClockCycle, fineX, tmpPalAddr, colorIndex, paletteNumber, val);
    char high[17] = "";
    char low[17] = "";
    sprintBitsUint16(high, ppu->patternTableShiftRegisterHigh);
    sprintBitsUint16(low, ppu->patternTableShiftRegisterLow);
    print("  --> using shift regs high: %s, low: %s\n", high, low);
    print("  --> ppu mem $3F00 onwards: %02x %02x %02x %02x %02x %02x\n", 
        ppu->memory[0x3F00], ppu->memory[0x3F01], ppu->memory[0x3F02], ppu->memory[0x3F03], ppu->memory[0x3F04], ppu->memory[0x3F05]);
  }

  *pixel = ((color.red << 16) | (color.green << 8) | color.blue);
  return val;
}

void fetchyFetchy(struct PPU *ppu) {
  /*print("fetchyFetchy: line %d, cycle %d\n", ppu->scanline, ppu->scanlineClockCycle);*/

  // TODO: make sure this one from the wiki matches the one we use down there to get the attributeByte
  /*uint16_t attributeAddress = 0x23C0 | (ppu->vRegister & 0x0C00) | ((ppu->vRegister >> 4) & 0x38) | ((ppu->vRegister >> 2) & 0x07);*/

  uint8_t baseNametableAddressCode = (ppu->vRegister >> 10) & 0x03; 
  int baseNametableAddress = 0x2000 + 0x0400 * baseNametableAddressCode;

  uint16_t coarseY = (ppu->vRegister >> 5) & 0x001F;
  uint16_t coarseX = ppu->vRegister & 0x001F;

  uint8_t fineY = (ppu->vRegister >> 12) & 0x07;

  // attribute table is 64 bytes of goodness (8 x 8; each of these blocks is made of up 4 x 4 tiles)
  int attributeTableAddress = baseNametableAddress + 960;

  // want to find byte in attributeTable for coarseY, coarseX
  int attributeBlockRow = coarseY / 4;  // 0 to 7
  int attributeBlockCol = coarseX / 4;  // 0 to 7
  const uint16_t attributeAddress = attributeTableAddress + (8 * attributeBlockRow + attributeBlockCol);
  uint8_t attributeByte = ppu->memory[attributeAddress];

  // 0: $0000; 1: $1000
  uint8_t backgroundPatternTableAddressCode = (ppu->control & 0x10) >> 4;
  int backgroundPatternTableAddress = 0x1000 * backgroundPatternTableAddressCode;

  int address = baseNametableAddress + coarseY*32 + coarseX;
  uint8_t indexIntoBackgroundPatternTable = ppu->memory[address];

  // 16 because the pattern table comes in 16 byte chunks 
  int addressOfBackgroundTile = backgroundPatternTableAddress + (indexIntoBackgroundPatternTable * 16);
  /*print("fetchyFetchy -- addressOfBackgroundTile: %04x\n", addressOfBackgroundTile);*/

  // Mesen calls 'address' the PPU Addr and the addressOfBackgroundTile the 'Tile Address'
  // mine matches theirs (ppu addr $2520, tile addr $1050)

  uint8_t byte1 = ppu->memory[addressOfBackgroundTile+8+fineY];
  uint8_t byte0 = ppu->memory[addressOfBackgroundTile+fineY];

  if (ppu->debuggingOn && (coarseX == 17 && coarseY == 13 && fineY == 2)) {
    print("[cycle %d / %d] [ppu addr: %04x] [tile addr: %04x] [attr addr: %04x] [attr byte: %02x] fetchy for tile Y: %d, X: %d, row %d (should render on line %d, pxs %d - %d)\n", 
        ppu->scanline, ppu->scanlineClockCycle, address, addressOfBackgroundTile, attributeAddress, attributeByte, coarseY, coarseX, fineY,
        coarseY * 8 + fineY, coarseX * 8, coarseX * 8 + 8);

    int vals[8];
    for (int i = 0; i < 8; i++) {
      uint8_t bit1 = (byte1 >> i) & 0x01;
      uint8_t bit0 = (byte0 >> i) & 0x01;
      vals[i] = bit1 << 1 | bit0;
    }
    print(" --> (%02x, %02x) %02x %02x %02x %02x %02x %02x %02x %02x\n", byte1, byte0, vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7]);
  }

  ppu->nt = address;
  ppu->at = attributeByte;
  ppu->ptTileHigh = byte1;
  ppu->ptTileLow = byte0;
}

void shifterReload(struct PPU *ppu) {
  uint16_t lowWas = ppu->patternTableShiftRegisterLow;
  uint16_t highWas = ppu->patternTableShiftRegisterHigh;

  ppu->patternTableShiftRegisterLow = ppu->patternTableShiftRegisterLow & ~0x00FF;  // clear the bottom 8 bits
  ppu->patternTableShiftRegisterLow = ppu->patternTableShiftRegisterLow | (ppu->ptTileLow);

  ppu->patternTableShiftRegisterHigh = ppu->patternTableShiftRegisterHigh & ~0x00FF;  // clear the bottom 8 bits
  ppu->patternTableShiftRegisterHigh = ppu->patternTableShiftRegisterHigh | (ppu->ptTileHigh);

  uint8_t attributeByte = ppu->at;

  uint8_t quadrantX = (ppu->nt & 0x02) == 0x02;
  uint8_t quadrantY = (ppu->nt & 0x40) == 0x40;

  // each attribute block is split into four quadrants; each quadrant is 2x2 tiles
  uint8_t quadrantNumber = quadrantY << 1 | quadrantX;

  ppu->paletteNumberFirst = ppu->paletteNumberSecond;

  // attribute byte might be something like 00 10 00 10; each pair of bits representing quads 3, 2, 1, 0 respectively
  ppu->paletteNumberSecond = (attributeByte >> quadrantNumber*2) & 0x03;
}

// This x-increment code taken from http://wiki.nesdev.com/w/index.php/PPU_scrolling#Wrapping_around
void coarseXIncrement(struct PPU *ppu) {
  if (!isRenderingEnabled(ppu)) {
    return;
  }
  if ((ppu->vRegister & 0x001F) == 31) { // if coarse X == 31
    ppu->vRegister &= ~0x001F;          // coarse X = 0
    /*print("switch horizontal nametable\n");*/
    ppu->vRegister ^= 0x0400;           // switch horizontal nametable
  } else {
    ppu->vRegister += 1;                // increment coarse X
  }
}

void copyHorizontalPosition(struct PPU *ppu) {
  if (!isRenderingEnabled(ppu)) {
    return;
  }
  // Copy bits related to horizontal position from t to v
  // v: ....F.. ...EDCBA = t: ....F.. ...EDCBA
  uint16_t justTheRelevantBitsOfT = ppu->tRegister & 0x041F; 
  ppu->vRegister = ppu->vRegister & ~0x041F;  // clear relevant bits
  ppu->vRegister = ppu->vRegister | justTheRelevantBitsOfT;
}

void copyVerticalPosition(struct PPU *ppu) {
  if (!isRenderingEnabled(ppu)) {
    return;
  }
  // v: IHGF.ED CBA..... = t: IHGF.ED CBA.....
  uint16_t justTheRelevantBitsOfT = ppu->tRegister & 0x7BE0; 
  ppu->vRegister = ppu->vRegister & ~0x7BE0;  // clear relevant bits
  ppu->vRegister = ppu->vRegister | justTheRelevantBitsOfT;
}

// This y-increment code taken from http://wiki.nesdev.com/w/index.php/PPU_scrolling#Wrapping_around
void incrementVerticalPosition(struct PPU *ppu) {
  if (!isRenderingEnabled(ppu)) {
    return;
  }

  if ((ppu->vRegister & 0x7000) != 0x7000) {        // if fine Y < 7
    ppu->vRegister += 0x1000;                      // increment fine Y
  } else {
    ppu->vRegister &= ~0x7000;                     // fine Y = 0
    int y = (ppu->vRegister & 0x03E0) >> 5;        // let y = coarse Y
    if (y == 29) {
      y = 0;                         // coarse Y = 0
      ppu->vRegister ^= 0x0800;                    // switch vertical nametable
    } else if (y == 31) {
      y = 0;                          // coarse Y = 0, nametable not switched
    } else {
      y += 1;                         // increment coarse Y
    }
    ppu->vRegister = (ppu->vRegister & ~0x03E0) | (y << 5);     // put coarse Y back into v
  }
}

void performBackgroundRegisterShifts(struct PPU *ppu) {
  // the PPU timing image on nesdev says this is when we do the register shifts
  if ((ppu->scanlineClockCycle >= 2 && ppu->scanlineClockCycle <= 257) || 
      (ppu->scanlineClockCycle >= 322 && ppu->scanlineClockCycle <= 337)) {
    ppu->patternTableShiftRegisterLow = ppu->patternTableShiftRegisterLow << 1;
    ppu->patternTableShiftRegisterHigh = ppu->patternTableShiftRegisterHigh << 1;
  }
}

void ppuTick(struct PPU *ppu, struct Computer *state, struct Color *palette, void *videoBuffer)
{
  if (ppu->scanline >= 241) {  // vblank
    if (ppu->scanline == 241 && ppu->scanlineClockCycle == 1) {
      /*print("VBLANK START\n");*/
      ppu->status = ppu->status | 0x80; // set vblank flag
      if (ppu->control >> 7 == 1) {
        triggerNmiInterrupt(state);
      }
    }

    performBackgroundRegisterShifts(ppu);
  } else if (ppu->scanline == -1) {
    // pre-render line

    if (ppu->scanlineClockCycle == 1) {
      /*print("VBLANK END\n");*/
      ppu->status = ppu->status & ~0x80; // clear vblank flag
      ppu->status = ppu->status & ~0x40; // clear sprite 0 hit flag

      // clear the sprites; these will be set starting on scanline 0 (for rendering beginning on scanline 1)
      memset(ppu->sprites0, 0xFF, sizeof ppu->sprites0);
      memset(ppu->sprites1, 0xFF, sizeof ppu->sprites1);

    } if (ppu->scanlineClockCycle == 256) {
      incrementVerticalPosition(ppu);
    } else if (ppu->scanlineClockCycle == 257) {
      copyHorizontalPosition(ppu);
    } else if (ppu->scanlineClockCycle >= 280 && ppu->scanlineClockCycle <= 304) {
      copyVerticalPosition(ppu);
    }

    if (ppu->scanlineClockCycle > 0 && ppu->scanlineClockCycle % 8 == 0 && (ppu->scanlineClockCycle <= 256 || ppu->scanlineClockCycle >= 328)) {
      // before we increment X, let's fetch some data for rendering 
      fetchyFetchy(ppu);

      coarseXIncrement(ppu);
    }

    performBackgroundRegisterShifts(ppu);

    if ((ppu->scanlineClockCycle - 1) > 0 && (ppu->scanlineClockCycle - 1) % 8 == 0 && (ppu->scanlineClockCycle <= 257 || ppu->scanlineClockCycle >= 329)) {
      shifterReload(ppu);
    }
  } else if (ppu->scanline >= 0 && ppu->scanline <= 239) {
    // visible scanlines

    if (ppu->scanlineClockCycle == 0) {
      struct Sprite *tmp = ppu->sprites;
      ppu->sprites = ppu->followingSprites;
      ppu->followingSprites = tmp;
    }

    if (ppu->scanlineClockCycle >= STARTING_PIXEL && ppu->scanlineClockCycle < VIDEO_BUFFER_WIDTH + STARTING_PIXEL) {
      uint8_t backgroundVal = renderBackgroundPixel2(ppu, state, palette, videoBuffer);
      if (ppu->scanline > 0) {
        renderSpritePixel(ppu, state, palette, backgroundVal, videoBuffer);
      }
    } // end of conditional for visible clock cycle

    if (ppu->scanlineClockCycle == 256) {
      incrementVerticalPosition(ppu);
    }

    if (ppu->scanlineClockCycle == 257) {
      copyHorizontalPosition(ppu);
    }

    if (ppu->scanlineClockCycle > 0 && ppu->scanlineClockCycle % 8 == 0 && (ppu->scanlineClockCycle <= 256 || ppu->scanlineClockCycle >= 328)) {
      fetchyFetchy(ppu);

      coarseXIncrement(ppu);
    }

    performBackgroundRegisterShifts(ppu);

    if ((ppu->scanlineClockCycle - 1) > 0 && (ppu->scanlineClockCycle - 1) % 8 == 0 && (ppu->scanlineClockCycle <= 257 || ppu->scanlineClockCycle >= 329)) {
      shifterReload(ppu);
    }

    if (ppu->scanlineClockCycle == 65) {
      spriteEvaluation(ppu);
    }
  }

  ppu->scanlineClockCycle++;
  if (ppu->scanlineClockCycle == 341) {
    ppu->scanline++;
    ppu->scanlineClockCycle = 0;
    if (ppu->scanline == 261) {
      ppu->scanline = -1;
    }
  }
}

void buildPPUClosure(struct PPUClosure *ppuClosure, struct PPU *ppu)
{
  *ppuClosure = (struct PPUClosure) { .ppu = ppu, .onMemoryWrite = &onCPUMemoryWrite, .onMemoryRead = &onCPUMemoryRead };
}

bool executeEmulatorCycle(struct Computer *state, struct PPU *ppu, void *videoBuffer, struct Color *palette) 
{
  uint8_t ppuStatusBefore = ppu->status;

  unsigned char instr = readMemory(state->pc, state);
  int cycles = executeInstruction(instr, state);

  for (int i = 0; i < cycles*3; i++) {
    ppuTick(ppu, state, palette, videoBuffer);
  }

  uint8_t ppuStatusAfter = ppu->status;

  return (ppuStatusBefore & 0x80) == 0 && (ppuStatusAfter & 0x80) == 0x80;
}
