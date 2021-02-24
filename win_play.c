#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "ppu.h"
#include "win_play.h"

// helpful: https://docs.microsoft.com/en-us/windows/win32/learnwin32/your-first-windows-program

// the 6502 has 256 byte pages

// might be helpful for PPU implementation:
// - http://nesdev.com/NESDoc.pdf
// - https://forums.nesdev.com/viewtopic.php?p=157086&sid=67b5e4517ef101b69e0c9d1286eeda16#p157086 
// - https://forums.nesdev.com/viewtopic.php?p=157167#p157167
// - http://nesdev.com/NES%20emulator%20development%20guide.txt
// - https://www.reddit.com/r/EmuDev/comments/7k08b9/not_sure_where_to_start_with_the_nes_ppu/drapgie/
// - https://www.dustmop.io/blog/2015/12/18/nes-graphics-part-3/
// - http://www.michaelburge.us/2019/03/18/nes-design.html#ppu
//
     /*
     *
     * https://wiki.nesdev.com/w/index.php/PPU_memory_map
     *
     * This seems helpful: http://forums.nesdev.com/viewtopic.php?f=3&t=18656
     *
     */

// super helpful: https://austinmorlan.com/posts/nes_rendering_overview/#nametable

// - got the kingswood assembler from http://web.archive.org/web/20190301123637/http://www.kingswood-consulting.co.uk/assemblers/
//   (but then learned that it also comes with the Klaus functional tests)

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int running = 1;

// TODO: get rid of global var
int mapperNumber = 0;

#define VIDEO_BUFFER_WIDTH 256
#define VIDEO_BUFFER_HEIGHT 240

void *videoBuffer;
BITMAPINFO bitmapInfo = { 0 };

struct Color 
{
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

void print(const char *format, ...) {
  va_list arguments;
  va_start(arguments, format);

  char str[200];
  vsprintf(str, format, arguments);
  va_end(arguments);

  printf(str);
  OutputDebugString(str);
}

void sprintBitsUint8(char *str, uint8_t val) {
  for (int i = 7; 0 <= i; i--) {
    sprintf(str + strlen(str), "%c", (val & (1 << i)) ? '1' : '0');
  }
}

void sprintBitsUint16(char *str, uint16_t val) {
  for (int i = 15; 0 <= i; i--) {
    sprintf(str + strlen(str), "%c", (val & (1 << i)) ? '1' : '0');
  }
}

void printBitsUint8(uint8_t val) {
  for (int i = 7; 0 <= i; i--) {
    print("%c", (val & (1 << i)) ? '1' : '0');
  }
  print("\n");
}

void dumpOam(int num, uint8_t *oam)
{
  FILE *file;
  char filename[30];
  sprintf(filename, "oam-%d.dump", num);
  file = fopen(filename, "w+");

  for (int i = 0; i < 256; i+=4) {
    fprintf(file, "ypos: %02x, tileindex: %02x, attr: %02x, xpos: %02x\n", oam[i], oam[i+1], oam[i+2], oam[i+3]);
  }
  print("done dumping oam to %s\n", filename);

  fclose(file);
}

void dumpNametable(int num, const struct PPU *ppu)
{
  print("dumping!\n\n");
  unsigned char control = ppu->control;
  int baseNametableAddress = 0x2000 + 0x0400 * num;
  
  int tileRow = 0;
  int tileCol = 0;

  FILE *file;
  char filename[30];
  sprintf(filename, "nametable-%d.dump", num);
  file = fopen(filename, "w+");

  // each nametable has 30 rows of 32 tiles each, for 960 ($3C0) bytes
  int nametableByteIndex = 0;
  for (nametableByteIndex = 0; nametableByteIndex < 0x3C0; nametableByteIndex += 1) {
    tileRow = nametableByteIndex / 32;
    tileCol = nametableByteIndex % 32;

    int address = baseNametableAddress + tileRow*32 + tileCol;
    unsigned char val = ppu->memory[address];

    fprintf(file, "%02x", val);
    if (tileCol == 31) {
      fprintf(file, "\n");
    } else {
      fprintf(file, " ");
    }
  }

  fclose(file);
}

void displayFrame(void *videoBuffer, HWND windowHandle, BITMAPINFO *bitmapInfo) {
  HDC deviceContext = GetDC(windowHandle);

  RECT clientRect;
  GetClientRect(windowHandle, &clientRect);
  int windowWidth = clientRect.right - clientRect.left;
  int windowHeight = clientRect.bottom - clientRect.top;

  StretchDIBits(deviceContext,
      0, 0, windowWidth, windowHeight,
      0, 0, VIDEO_BUFFER_WIDTH, VIDEO_BUFFER_HEIGHT,
      videoBuffer,
      bitmapInfo,
      DIB_RGB_COLORS, SRCCOPY);

  ReleaseDC(windowHandle, deviceContext);
}

void setPPUData(unsigned char value, struct PPU *ppu, int inc) 
{
  /*print("setPPUData. Set addr %04x to %02x (then increment by %d)\n", ppu->ppuAddr, value, inc);*/

  if (ppu->vRegister > 0x3FFF) {
    print("trying to write out of ppu bounds!\n");
  } else {
    ppu->memory[ppu->vRegister] = value;
  }

  uint16_t iWas = ppu->vRegister;
  char was[17] = "";
  char is[17] = "";
  sprintBitsUint16(was, ppu->vRegister);

  ppu->vRegister = ppu->vRegister + inc;
  sprintBitsUint16(is, ppu->vRegister);
  uint16_t iIs = ppu->vRegister;
  /*print("(setPPUData) v was %s (%04x), is %s (%04x); value set to %02x\n", was, iWas, is, iIs, value);*/
}

void setButton(struct Computer *state, bool isButtonPressed, uint8_t position) {
  if (isButtonPressed) {
    state->buttons = state->buttons | (1 << position);
  }
}

/*
  
PPUCTRL: 0001 0000  (background pattern table address: 1 == $1000)
SCROLL:  1001 1011  (x offset = 155)
SCROLL:  0000 0000  (y offset = 0)
PPUCTRL: 1001 0000  (turn NMI back on)
 
*/

/*
 megaman 2, metal man
# of milliseconds for frame: 21.440800  (frames per second: 46.640051)
*************** PPUCTRL 0x2000 write: 10
*************** SCROLL 0x2005 write: 9b
*************** SCROLL 0x2005 write: 00
*************** PPUCTRL 0x2000 write: 90
# of milliseconds for frame: 23.077800  (frames per second: 43.331687)
*************** PPUCTRL 0x2000 write: 10
*************** SCROLL 0x2005 write: 9d
*************** SCROLL 0x2005 write: 00
*************** PPUCTRL 0x2000 write: 90
# of milliseconds for frame: 21.525800  (frames per second: 46.455881)
*************** PPUCTRL 0x2000 write: 10
*************** SCROLL 0x2005 write: a0
*************** SCROLL 0x2005 write: 00
*************** PPUCTRL 0x2000 write: 90
# of milliseconds for frame: 21.908100  (frames per second: 45.645218)
*************** PPUCTRL 0x2000 write: 10
*************** SCROLL 0x2005 write: a2
*************** SCROLL 0x2005 write: 00
*************** PPUCTRL 0x2000 write: 90
*/

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
    setPPUData(value, ppu, vramIncrement(ppu));
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
    if (mapperNumber == 1) {
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
    print("(0x2007 read) incremented vRegister to %04x\n", ppu->vRegister);
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

int vramIncrement(struct PPU *ppu) {
  if (ppu->control >> 2 & 0x01 == 1) {
    return 32;
  } else {
    return 1;
  }
}

void renderSpritePixel(struct PPU *ppu, struct Computer *state, struct Color *palette, uint8_t backgroundVal) {
  unsigned char control = ppu->control;

  int spritePatternTableAddress = 0x0000;
  if (control >> 3 == 1) {
    spritePatternTableAddress = 0x1000;
  }

  // find which pixel we're working on, see if that matches a sprite, and then figure out what to render there if it does.

  int pixelX = ppu->scanlineClockCycle;
  int pixelY = ppu->scanline;

  // search through the 64 sprites to see if any affect us
  for (int i = 0; i < 256; i+=4) {
    bool isSpriteZero = (i == 0);
    uint8_t ypos = ppu->oam[i];
    uint8_t xpos = ppu->oam[i+3];

    // TODO: find docs on this. I'm seeing tons of ypos FF and xpos 0 sprites which I guess are 'empty'?
    // I think the docs are here: 'hide a sprite...' https://wiki.nesdev.com/w/index.php/PPU_OAM
    if (ypos > 240-8) {
      continue;
    }

    uint8_t xStart = xpos;
    uint8_t xEnd = xpos + 8;
    uint8_t yStart = ypos;
    uint8_t yEnd = ypos + 8;

    if (xStart <= pixelX && pixelX < xEnd && yStart <= pixelY && pixelY < yEnd) {
      // render pixel at pixelX, pixelY
      uint8_t tileIndex = ppu->oam[i+1];
      uint8_t attributes = ppu->oam[i+2];

      bool flipHorizontally = attributes & 0x40;


      // 16 because the pattern table comes in 16 byte chunks 
      int addressOfSprite = spritePatternTableAddress + (tileIndex * 16);
      uint8_t *sprite = &ppu->memory[addressOfSprite];

      uint32_t *videoBufferRow = (uint32_t *)videoBuffer;
      videoBufferRow += (pixelY * VIDEO_BUFFER_WIDTH) + pixelX;

      {
        // find out which row of the 8x8 sprite is relevant for us
        int rowOfSprite = pixelY - ypos;

        uint8_t lowByte = sprite[rowOfSprite];
        uint8_t highByte = sprite[rowOfSprite+8];

        uint32_t *pixel = videoBufferRow;

        int pixelIncrement = 1;

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
          uint8_t colorIndex = ppu->memory[0x3F01 + 4*paletteNumber + val - 1];
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

uint8_t renderBackgroundPixel2(struct PPU *ppu, struct Computer *state, struct Color *palette) {
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
  pixel += x;


  // TODO: rename val
  int val = bit1 << 1 | bit0;

  // tile 0 is 0 to 7, tile 1 is 8 to 15, tile 2 is 16 to 23, etc. So tile 9 starts at 9*8
  if (ppu->debuggingOn && ppu->scanline == 9*8 + 2 && ppu->scanlineClockCycle <= 19) {
    print("[cycle %d / %d] [fineX: %d], print pixel %02x\n", ppu->scanline, ppu->scanlineClockCycle, fineX, val);
    char high[17] = "";
    char low[17] = "";
    sprintBitsUint16(high, ppu->patternTableShiftRegisterHigh);
    sprintBitsUint16(low, ppu->patternTableShiftRegisterLow);
    print("  --> using shift regs high: %s, low: %s\n", high, low);
  }

  /*$3F00 	Universal background color*/
  /*$3F01-$3F03 	Background palette 0*/
  /*$3F05-$3F07 	Background palette 1*/
  /*$3F09-$3F0B 	Background palette 2*/
  /*$3F0D-$3F0F 	Background palette 3 */
  uint8_t paletteNumber = ppu->paletteNumber;
  uint8_t colorIndex = universalBackgroundColor;
  if (val > 0) {
    colorIndex = ppu->memory[0x3F01 + 4*paletteNumber + val - 1];
  }
  struct Color color = palette[colorIndex];

  /*
  struct Color color = { .red = 0, .green = 0, .blue = 0 };
  if (val == 0) {
  } else if (val == 1) {
    color.red = 0xFF;
  } else if (val == 2) {
    color.green = 0xFF;
  } else {
    color.blue = 0xFF;
  }
  */

  *pixel = ((color.red << 16) | (color.green << 8) | color.blue);
  return val;
}

uint8_t renderBackgroundPixel(struct PPU *ppu, struct Computer *state, struct Color *palette) {
  const int y = ppu->scanline;
  const int x = ppu->scanlineClockCycle;

  unsigned char control = ppu->control;
  uint8_t universalBackgroundColor = ppu->memory[0x3F00];

  // 0 = $2000; 1 = $2400; 2 = $2800; 3 = $2C00
  uint8_t origBaseNametableAddressCode = (control & 0x02) | (control & 0x01);
  uint8_t baseNametableAddressCode = (ppu->vRegister >> 10) & 0x03; 
  int baseNametableAddress = 0x2000 + 0x0400 * baseNametableAddressCode;

  // 0: $0000; 1: $1000
  unsigned char backgroundPatternTableAddressCode = (control & 0x10) >> 4;
  int backgroundPatternTableAddress = 0x1000 * backgroundPatternTableAddressCode;

  // each nametable has 30 rows of 32 tiles each, for 960 ($3C0) bytes. Each tile is 8x8 pixels.
  int tileRow = y / 8;
  int tileCol = x / 8;

  // attribute table is 64 bytes of goodness (8 x 8; each of these blocks is made of up 4 x 4 tiles)
  int attributeTableAddress = baseNametableAddress + 960;

  // want to find byte in attributeTable for tileRow, tileCol
  int attributeBlockRow = tileRow / 4;  // 0 to 7
  int attributeBlockCol = tileCol / 4;  // 0 to 7
  uint8_t attributeByte = ppu->memory[attributeTableAddress + (8 * attributeBlockRow + attributeBlockCol)];

  // each attribute block is split into four quadrants; each quadrant is 2x2 tiles
  int quadrantX = (tileCol - attributeBlockCol*4) / 2;
  int quadrantY = (tileRow - attributeBlockRow*4) / 2;
  int quadrantNumber = quadrantY << 1 | quadrantX;

  // attribute byte might be something like 00 10 00 10; each pair of bits representing quads 3, 2, 1, 0 respectively
  int paletteNumber = (attributeByte >> quadrantNumber*2) & 0x03;

  int address = baseNametableAddress + tileRow*32 + tileCol;
  uint8_t indexIntoBackgroundPatternTable = ppu->memory[address];

  // 16 because the pattern table comes in 16 byte chunks 
  int addressOfBackgroundTile = backgroundPatternTableAddress + (indexIntoBackgroundPatternTable * 16);
  /*print("orig -- addressOfBackgroundTile: %04x\n", addressOfBackgroundTile);*/

  int tileRowPixel = tileRow * 8;
  int tileColPixel = tileCol * 8;

  int row = y - tileRowPixel;
  int col = x - tileColPixel;

  {
    int bit = 7 - col;

    unsigned char bit1 = (ppu->memory[addressOfBackgroundTile+8+row] >> bit & 0x01) != 0;
    unsigned char bit0 = (ppu->memory[addressOfBackgroundTile+row] >> bit & 0x01) != 0;

    uint8_t *videoBufferRow = (uint8_t *)videoBuffer;
    videoBufferRow = videoBufferRow + ((tileRowPixel + row) * VIDEO_BUFFER_WIDTH * 4);

    uint32_t *pixel = (uint32_t *)(videoBufferRow);
    pixel += (tileColPixel + col);

    // TODO: rename val
    int val = bit1 << 1 | bit0;

    if (ppu->debuggingOn && ppu->scanline == 9*8 + 2 && ppu->scanlineClockCycle <= (0+1)*8) {
      print("[cycle %d / %d] print pixel %02x\n", ppu->scanline, ppu->scanlineClockCycle, val);
    }

    /*
    struct Color color = { .red = 0, .green = 0, .blue = 0 };
    if (val == 0) {
    } else if (val == 1) {
      color.red = 0xFF;
    } else if (val == 2) {
      color.green = 0xFF;
    } else {
      color.blue = 0xFF;
    }
    */

    /*$3F00 	Universal background color*/
    /*$3F01-$3F03 	Background palette 0*/
    /*$3F05-$3F07 	Background palette 1*/
    /*$3F09-$3F0B 	Background palette 2*/
    /*$3F0D-$3F0F 	Background palette 3 */
    uint8_t colorIndex = universalBackgroundColor;
    if (val > 0) {
      colorIndex = ppu->memory[0x3F01 + 4*paletteNumber + val - 1];
    }
    struct Color color = palette[colorIndex];

    *pixel = ((color.red << 16) | (color.green << 8) | color.blue);
    return val;
  }
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
  uint8_t attributeByte = ppu->memory[attributeTableAddress + (8 * attributeBlockRow + attributeBlockCol)];

  /*
  // each attribute block is split into four quadrants; each quadrant is 2x2 tiles
  uint8_t quadrantX = (coarseX - attributeBlockCol*4) / 2;
  uint8_t quadrantY = (coarseY - attributeBlockRow*4) / 2;
  uint8_t quadrantNumber = quadrantY << 1 | quadrantX;

  // attribute byte might be something like 00 10 00 10; each pair of bits representing quads 3, 2, 1, 0 respectively
  uint8_t paletteNumber = (attributeByte >> quadrantNumber*2) & 0x03;
  */

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

  if (ppu->debuggingOn && ((coarseX == 0 && coarseY == 9 && fineY == 2) || (coarseX == 1 && coarseY == 9 && fineY == 2))) {
    print("[cycle %d / %d] [ppu addr: %04x] [tile addr: %04x] fetchy for tile Y: %d, X: %d, row %d (should render on line %d, pxs %d - %d)\n", 
        ppu->scanline, ppu->scanlineClockCycle, address, addressOfBackgroundTile, coarseY, coarseX, fineY,
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
  // what do we store here? there are two attribute shift registers to fill. https://forums.nesdev.com/viewtopic.php?t=10348
  // last comment here seems important: https://forums.nesdev.com/viewtopic.php?t=17568
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

  if (ppu->debuggingOn) {
    print("shifter reload. high/low: %04x, %04x -> %04x, %04x\n", highWas, lowWas, ppu->patternTableShiftRegisterHigh, ppu->patternTableShiftRegisterLow);
  }

  uint8_t attributeByte = ppu->at;

  // TODO: make sure I understand this
  uint8_t quadrantX = ppu->nt & 0x02 == 0x02;
  uint8_t quadrantY = ppu->nt & 0x40 == 0x40;
  /*print("quadX: %d, quadY: %d\n", quadrantX, quadrantY);*/

  // each attribute block is split into four quadrants; each quadrant is 2x2 tiles
  /*uint8_t quadrantX = (coarseX - attributeBlockCol*4) / 2;*/
  /*uint8_t quadrantY = (coarseY - attributeBlockRow*4) / 2;*/
  uint8_t quadrantNumber = quadrantY << 1 | quadrantX;

  // attribute byte might be something like 00 10 00 10; each pair of bits representing quads 3, 2, 1, 0 respectively
  ppu->paletteNumber = (attributeByte >> quadrantNumber*2) & 0x03;

  /*ppu->attributeTableShiftRegisterHigh*/
  /*ppu->attributeTableShiftRegisterLow*/
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

void ppuTick(struct PPU *ppu, struct Computer *state, struct Color *palette)
{
  /*print("ppuTick. scanline: %d  cycle: %d\n", ppu->scanline, ppu->scanlineClockCycle);*/
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

    if (ppu->scanlineClockCycle >= 2 && ppu->scanlineClockCycle <= 257) {
      uint8_t backgroundVal = renderBackgroundPixel2(ppu, state, palette);
      renderSpritePixel(ppu, state, palette, backgroundVal);
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





void setKeyboardInput(bool *buttonValue, bool wasDown, bool isDown) {
  if (wasDown && !isDown) {
    *buttonValue = false;
  }
  if (!wasDown && isDown) {
    *buttonValue = true;
  }
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
  LARGE_INTEGER perfFrequencyResult;
  QueryPerformanceFrequency(&perfFrequencyResult);
  int64_t perfFrequency = perfFrequencyResult.QuadPart;

  // TODO: check return value to see if it worked
  timeBeginPeriod(1);

  unsigned char header[16];
  FILE *file;

  /*file = fopen("donkey_kong.nes", "rb");*/
  /*file = fopen("Excitebike.nes", "rb");*/
  file = fopen("MegaMan2.nes", "rb");

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
  mapperNumber = (header[7] & 0xF0) | (header[6] >> 4);
  print("mapper number: %d\n", mapperNumber);

  // TODO: check return value of mallocs and callocs
  unsigned char *memory;
  memory = (unsigned char *) malloc(0x8000 + sizeOfPrgRomInBytes);

  unsigned char *ppuMemory;
  ppuMemory = (unsigned char *) calloc(1, 0x3FFF);

  uint8_t *oam;
  oam = (uint8_t *) calloc(256, sizeof(uint8_t));

  unsigned char *prgRom;
  prgRom = (unsigned char *) malloc(sizeOfPrgRomInBytes);
  if (prgRom == 0) 
  {
    print("Could not allocate memory for prgRom.");
    // TODO: free the other memory I've allocated?
    return(0);
  }

  unsigned char *chrRom;
  chrRom = (unsigned char *) malloc(sizeOfChrRomInBytes);
  if (chrRom == 0)
  {
    print("Could not allocate memory for chrRom.");
    return(0);
  }

  print("about to read into prgRom, num of bytes: %d\n", sizeOfPrgRomInBytes);
  fread(prgRom, sizeOfPrgRomInBytes, 1, file);
  fread(chrRom, sizeOfChrRomInBytes, 1, file);
  fclose(file);

  // copy prgRom starting at 0x8000, even if it's bigger than 32 kB. We will intercept reads
  // from this memory and do the right thing based on MMC.
  memcpy(&memory[0x8000], prgRom, sizeOfPrgRomInBytes);

  /*
  // NROM: put prgRom into memory at $8000-$BFFF and $C000-$FFFF   https://wiki.nesdev.com/w/index.php/NROM
  memcpy(&memory[0x8000], prgRom, 0x4000);
  if (mapperNumber == 0) {
    // "mirror" (copy in my case) of what's at 0x8000
    memcpy(&memory[0xC000], prgRom, 0x4000);
  } else if (mapperNumber == 1) {
    // copy last bank of prgRom here (http://wiki.nesdev.com/w/index.php/MMC1)
    memcpy(&memory[0xC000], prgRom + ((numPrgRomUnits - 1) * 0x4000), 0x4000);
  } else {
    exit(EXIT_FAILURE);
  }
  */

  // TODO: might as well just read chrRom right into ppuMemory, right?
  memcpy(ppuMemory, chrRom, sizeOfChrRomInBytes);
  struct PPU ppu = { .memory = ppuMemory, .oam = oam, .scanline = -1 };   // TODO: make a struct initializer
  ppu.wRegister = false;

  // colours from http://www.thealmightyguru.com/Games/Hacking/Wiki/index.php/NES_Palette
  // TODO: write them into a .pal file and read them out
  struct Color palette[64] = {
    {124,124,124},{0,0,252},{0,0,188},{68,40,188},{148,0,132},{168,0,32},{168,16,0},{136,20,0},
    {80,48,0},{0,120,0},{0,104,0},{0,88,0},{0,64,88},{0,0,0},{0,0,0},{0,0,0},
    {188,188,188},{0,120,248},{0,88,248},{104,68,252},{216,0,204},{228,0,88},{248,56,0},{228,92,16},
    {172,124,0},{0,184,0},{0,168,0},{0,168,68},{0,136,136},{0,0,0},{0,0,0},{0,0,0},
    {248,248,248},{60,188,252},{104,136,252},{152,120,248},{248,120,248},{248,88,152},{248,120,88},{252,160,68},
    {248,184,0},{184,248,24},{88,216,84},{88,248,152},{0,232,216},{120,120,120},{0,0,0},{0,0,0},
    {252,252,252},{164,228,252},{184,184,248},{216,184,248},{248,184,248},{248,164,192},{240,208,176},{252,224,168},
    {248,216,120},{216,248,120},{184,248,184},{184,248,216},{0,252,252},{248,216,248},{0,0,0},{0,0,0}
  };

  // Prep video buffer and bitmap info
  videoBuffer = malloc(VIDEO_BUFFER_WIDTH * VIDEO_BUFFER_HEIGHT * 4);

  bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
  bitmapInfo.bmiHeader.biWidth = VIDEO_BUFFER_WIDTH;
  bitmapInfo.bmiHeader.biHeight = -VIDEO_BUFFER_HEIGHT;
  bitmapInfo.bmiHeader.biPlanes = 1;
  bitmapInfo.bmiHeader.biBitCount = 32;
  bitmapInfo.bmiHeader.biCompression = BI_RGB;

  // Register the window class.
  LPCSTR CLASS_NAME = "CastlefaceWindowClass";

  WNDCLASSA wc = { 0 };

  wc.lpfnWndProc   = WindowProc;
  wc.hInstance     = hInstance;
  wc.lpszClassName = CLASS_NAME;
  wc.style = CS_HREDRAW|CS_VREDRAW;

  RegisterClassA(&wc);

  // Create the window.
  // TODO: I might have to account for the title bar when setting the window height.
  HWND windowHandle = CreateWindowExA(
      0,                              // Optional window styles.
      CLASS_NAME,                     // Window class
      "Castleface",     // Window text
      WS_OVERLAPPEDWINDOW,            // Window style

      // Size and position
      CW_USEDEFAULT, CW_USEDEFAULT, VIDEO_BUFFER_WIDTH*4, VIDEO_BUFFER_HEIGHT*4,

      NULL,       // Parent window    
      NULL,       // Menu
      hInstance,  // Instance handle
      NULL        // Additional application data
      );

  if (windowHandle == NULL) {
    return 0;
  }

  ShowWindow(windowHandle, nShowCmd);

  uint32_t loopCount = 0;



  struct KeyboardInput keyboardInput = { .up = false  };
  struct PPUClosure ppuClosure = { .ppu = &ppu, .onMemoryWrite = &onCPUMemoryWrite, .onMemoryRead = &onCPUMemoryRead };

  struct Computer state = { .memory = memory, .keyboardInput = &keyboardInput, .ppuClosure = &ppuClosure };

  if (mapperNumber == 0) {
    state.prgRomBlock1 = &memory[0x8000];
    state.prgRomBlock2 = &memory[0xA000];
    // for NROM the second 16 kB is a mirror of the first 16 kB
    state.prgRomBlock3 = &memory[0x8000];
    state.prgRomBlock4 = &memory[0xA000];
  } else if (mapperNumber == 1) {
    // For mapper 1 it seems to be important to start off with the last 16 kB bank in 0xC000 - 0xFFFF
    state.prgRomBlock1 = &memory[0x8000];
    state.prgRomBlock2 = &memory[0xA000];
    int addressOfLastBank = 0x8000 + ((numPrgRomUnits - 1) * 0x4000); 
    state.prgRomBlock3 = &memory[addressOfLastBank];
    state.prgRomBlock4 = &memory[addressOfLastBank + 0x2000];
  } else {
    exit(EXIT_FAILURE);
  }

  int memoryAddressToStartAt = (readMemory(0xFFFD, &state) << 8) | readMemory(0xFFFC, &state);
  print("memory address to start is: %04x\n", memoryAddressToStartAt);
  state.pc = memoryAddressToStartAt;

  int instructionsExecuted = 0;
  unsigned char instr = 0;

  LARGE_INTEGER lastPerfCount;
  QueryPerformanceCounter(&lastPerfCount);

  while(running && state.pc < 0xFFFF)
  {
    MSG msg = { 0 };
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_QUIT) {
        OutputDebugString("going to quit");
        running = 0;
      } else if (msg.message == WM_KEYDOWN || msg.message == WM_KEYUP) {
        uint8_t wasDown = ((msg.lParam & (1 << 30)) != 0);
        uint8_t isDown = ((msg.lParam & (1 << 31)) == 0);

        switch(msg.wParam) {
          case 0x57: // w
            setKeyboardInput(&keyboardInput.up, wasDown, isDown);
            break;
          case 0x53: // s
            setKeyboardInput(&keyboardInput.down, wasDown, isDown);
            break;
          case 0x44: // d
            setKeyboardInput(&keyboardInput.right, wasDown, isDown);
            break;
          case 0x41: // a
            setKeyboardInput(&keyboardInput.left, wasDown, isDown);
            break;
          case 0x31: // 1
            setKeyboardInput(&keyboardInput.select, wasDown, isDown);
            break;
          case 0x32: // 2
            setKeyboardInput(&keyboardInput.start, wasDown, isDown);
            break;
          case 0x4B: // k 
            setKeyboardInput(&keyboardInput.b, wasDown, isDown);
            break;
          case 0x4C: // l 
            setKeyboardInput(&keyboardInput.a, wasDown, isDown);
            break;
          case 0x33: // 3
            dumpOam(1, ppu.oam);
            break;
          case 0x34: // 4
            state.debuggingOn = true;
            ppu.debuggingOn = true;
            break;
          case 0x35: // 5
            state.debuggingOn = false;
            ppu.debuggingOn = false;
            break;
        }
      }

      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }

    // TODO: we could move the instruction execution and ppuTick into a game layer. It could tell us
    // when vblank has started so this platform layer can display the frame and sleep.

    uint8_t ppuStatusBefore = ppu.status;

    instr = readMemory(state.pc, &state);
    int cycles = executeInstruction(instr, &state);

    for (int i = 0; i < cycles*3; i++) {
      ppuTick(&ppu, &state, palette);
    }

    uint8_t ppuStatusAfter = ppu.status;

    instructionsExecuted++;
    loopCount++;

    // if vblank has started
    if ((ppuStatusBefore & 0x80) == 0 && (ppuStatusAfter & 0x80) == 0x80) {
      LARGE_INTEGER midPerfCount;
      QueryPerformanceCounter(&midPerfCount);
      int64_t midPerfDiff = midPerfCount.QuadPart - lastPerfCount.QuadPart;
      double millisecondsElapsed = (1000.0f*(double)midPerfDiff) / (double)perfFrequency;

      if (millisecondsElapsed < 16) {
        DWORD sleepTime = (16 - (DWORD)millisecondsElapsed);
        print("sleep for %d milliseconds\n", sleepTime);
        Sleep(sleepTime);

        // TODO: Consider adding what Casey does in Handmade Hero: a loop to kill time if the sleep isn't
        // granular enough. I think he also runs that loop if the sleep didn't sleep long enough, although
        // that seems like an unlikely scenario to me.
      }

      displayFrame(videoBuffer, windowHandle, &bitmapInfo);

      LARGE_INTEGER endPerfCount;
      QueryPerformanceCounter(&endPerfCount);
      int64_t perfCount = endPerfCount.QuadPart - lastPerfCount.QuadPart;
      double milliseconds = (1000.0f*(double)perfCount) / (double)perfFrequency;
      double framesPerSecond = (double)perfFrequency / (double)perfCount;
      if (state.debuggingOn) {
        print("# of milliseconds for frame: %f  (frames per second: %f)\n", milliseconds, framesPerSecond);
      }

      lastPerfCount = endPerfCount;
    }

  }

  free(videoBuffer);
  free(memory);
  free(ppuMemory);
  free(oam);
  free(prgRom);
  free(chrRom);
  timeEndPeriod(1);
  return 0;
}

LRESULT CALLBACK WindowProc(HWND windowHandle, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  LRESULT result = 0;

  switch (uMsg)
  {
    case WM_CLOSE:
      running = 0;
      break;

    case WM_DESTROY:
      PostQuitMessage(0);
      break;

    case WM_PAINT:
      {
        PAINTSTRUCT ps;
        HDC deviceContext = BeginPaint(windowHandle, &ps);

        // FillRect(deviceContext, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW+1));

        RECT clientRect;
        GetClientRect(windowHandle, &clientRect);
        int windowWidth = clientRect.right - clientRect.left;
        int windowHeight = clientRect.bottom - clientRect.top;

        uint8_t *row = (uint8_t *)videoBuffer;    
        for (int y = 0; y < VIDEO_BUFFER_HEIGHT; y++)
        {
          uint32_t *pixel = (uint32_t *)row;
          for(int x = 0; x < VIDEO_BUFFER_WIDTH; x++)
          {
            uint8_t blue = (x + 1);
            uint8_t green = (y + 1);

            *pixel++ = ((green << 8) | blue);
          }

          row += (VIDEO_BUFFER_WIDTH * 4);
        }

        StretchDIBits(deviceContext,
          0, 0, windowWidth, windowHeight,
          0, 0, VIDEO_BUFFER_WIDTH, VIDEO_BUFFER_HEIGHT,
          videoBuffer,
          &bitmapInfo,
          DIB_RGB_COLORS, SRCCOPY);

        EndPaint(windowHandle, &ps);
      }

      break;
    default:
      result = DefWindowProc(windowHandle, uMsg, wParam, lParam);
      break;
  }

  return result;
}

