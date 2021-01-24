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

// upper byte gets written first, then lower byte
void setPPUAddr(unsigned char value, struct PPU *ppu) 
{
  if (ppu->ppuAddrSetLow) {
    // write lower byte
    ppu->ppuAddrGateLow = value;
    ppu->ppuAddrSetLow = false;  // so that next time we set the upper byte

    ppu->ppuAddr = (ppu->ppuAddrGateHigh << 8) | ppu->ppuAddrGateLow;
  } else {
    // write upper byte
    ppu->ppuAddrGateHigh = value;
    ppu->ppuAddrSetLow = true;  // so that next time we set the lower byte
  }
}

void setPPUData(unsigned char value, struct PPU *ppu, int inc) 
{
  /*print("setPPUData. Set addr %04x to %02x (then increment by %d)\n", ppu->ppuAddr, value, inc);*/

  if (ppu->ppuAddr > 0x3FFF) {
    print("trying to write out of ppu bounds!\n");
  } else {
    ppu->memory[ppu->ppuAddr] = value;
  }

  ppu->ppuAddr = ppu->ppuAddr + inc;
}

void setButton(struct Computer *state, bool isButtonPressed, uint8_t position) {
  if (isButtonPressed) {
    state->buttons = state->buttons | position;
  }
}

// TODO: I'm not so sure I want this to be the final mechanism to handle PPU/CPU communication.
void onCPUMemoryWrite(unsigned int memoryAddress, unsigned char value, struct Computer *state) 
{
  struct PPU *ppu = state->ppuClosure->ppu;

  // TODO: consider using a table of function pointers
  if (memoryAddress == 0x2006) {
    setPPUAddr(value, ppu);
  } else if (memoryAddress == 0x2007) {
    setPPUData(value, ppu, vramIncrement(ppu));
  } else if (memoryAddress == 0x2001) {
    ppu->mask = value;
  } else if (memoryAddress == 0x2000) {
    ppu->control = value;
  } else if (memoryAddress == 0x2005) {
    // scroll
    /*print("*************** SCROLL 0x2005 write: %02x\n", value);*/
  } else if (memoryAddress == 0x2003) {
    ppu->oamAddr = value;
  } else if (memoryAddress == 0x2004) {
    // oamdata write
  } else if (memoryAddress == 0x4014) {
    int cpuAddr = value << 8;
    int numBytes = 256 - ppu->oamAddr;
    /*print("[OAM] OAMDMA write. Will get data from CPU memory page %02x (addr: %04x). Oam addr is %02x. Num bytes: %d\n", value, cpuAddr, ppu->oamAddr, numBytes);*/
    memcpy(&ppu->oam[ppu->oamAddr], &state->memory[cpuAddr], numBytes);
    /*dumpOam(1, ppu->oam);*/
  } else if (memoryAddress == 0x4016) {
    /*print("************ write to 0x4016: %02x\n", value);*/
    state->pollController = (value == 1);
    if (state->pollController) {
      // copy keyboard input into buttons
      state->buttons = 0; 
      setButton(state, state->keyboardInput->up, 0x10); 
      setButton(state, state->keyboardInput->down, 0x20); 
      setButton(state, state->keyboardInput->left, 0x40); 
      setButton(state, state->keyboardInput->right, 0x80); 
      setButton(state, state->keyboardInput->select, 0x04); 
      setButton(state, state->keyboardInput->start, 0x08); 
      setButton(state, state->keyboardInput->a, 0x00); 
      setButton(state, state->keyboardInput->b, 0x01);
    }
    state->currentButtonBit = 0;  // is this right?
  }
}

unsigned char onCPUMemoryRead(unsigned int memoryAddress, struct Computer *state, bool *shouldOverride) {
  if (memoryAddress == 0x2002) {
    struct PPU *ppu = state->ppuClosure->ppu;
    uint8_t status = ppu->status;  // copy the status out so we can return it as it was before clearing flags

    ppu->status = ppu->status & ~0x80;  // clear vblank flag
    ppu->ppuAddrGateLow = 0;
    ppu->ppuAddrGateHigh = 0;
    *shouldOverride = true;

    return status; 
  } else if (memoryAddress == 0x2007) {
    struct PPU *ppu = state->ppuClosure->ppu;
    /*print("READING 0x2007 *************************\n\n");*/
    ppu->ppuAddr = ppu->ppuAddr + vramIncrement(ppu);
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



uint8_t renderBackgroundPixel(struct PPU *ppu, struct Computer *state, struct Color *palette) {
  unsigned char control = ppu->control;
  uint8_t universalBackgroundColor = ppu->memory[0x3F00];

  // 0 = $2000; 1 = $2400; 2 = $2800; 3 = $2C00
  unsigned char baseNametableAddressCode = (control & 0x02) | (control & 0x01);
  int baseNametableAddress = 0x2000 + 0x0400 * baseNametableAddressCode;

  // attribute table is 64 bytes of goodness (8 x 8; each of these blocks is made of up 4 x 4 tiles)
  int attributeTableAddress = baseNametableAddress + 960;

  // 0: $0000; 1: $1000
  unsigned char backgroundPatternTableAddressCode = (control & 0x10) >> 4;
  int backgroundPatternTableAddress = 0x1000 * backgroundPatternTableAddressCode;

  // each nametable has 30 rows of 32 tiles each, for 960 ($3C0) bytes. Each tile is 8x8 pixels.
  int tileRow = ppu->scanline / 8;
  int tileCol = ppu->scanlineClockCycle / 8;

  /*print("(%d, %d) working on tile at row, col: %d, %d\n", ppu->scanline, ppu->scanlineClockCycle, tileRow, tileCol);*/

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

  // TODO: right now these are the start of the background tile.
  int tileRowPixel = tileRow * 8;
  int tileColPixel = tileCol * 8;

  int row = ppu->scanline - tileRowPixel;
  int col = ppu->scanlineClockCycle - tileColPixel;

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
  } else if (ppu->scanline == -1) {
    // pre-render line

    if (ppu->scanlineClockCycle == 1) {
      /*print("VBLANK END\n");*/
      ppu->status = ppu->status & ~0x80; // clear vblank flag
      ppu->status = ppu->status & ~0x40; // clear sprite 0 hit flag
    }
  } else if (ppu->scanline >= 0 && ppu->scanline <= 239) {
    // visible scanlines

    if (ppu->scanlineClockCycle >= 0 && ppu->scanlineClockCycle <= 255) {
      uint8_t backgroundVal = renderBackgroundPixel(ppu, state, palette);
      renderSpritePixel(ppu, state, palette, backgroundVal);
    } // end of conditional for visible clock cycle
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
  file = fopen("Excitebike.nes", "rb");

  fread(header, sizeof(header), 1, file);

  int sizeOfPrgRomInBytes = 16 * 1024 * header[4];
  print("Size of PRG ROM in 16kb units: %d (and in bytes: %d)\n", header[4], sizeOfPrgRomInBytes);

  int sizeOfChrRomInBytes = 8 * 1024 * header[5];
  print("Size of CHR ROM in 8 KB units (Value 0 means the board uses CHR RAM): %d (and in bytes: %d)\n", header[5], sizeOfChrRomInBytes);

  if ((header[7]&0x0C)==0x08) {
    print("NES 2.0 format\n");
  } else {
    print("Not NES 2.0 format\n");
  }

  if ((header[6] & 0x01) == 0x01) {
    print("Vertical mirroring\n");
  } else {
    print("Horizontal mirroring\n");
  }

  if ((header[6] & 2) == 2) {
    print("Cartridge contains battery-backed PRG RAM ($6000-7FFF) or other persistent memory\n");
  } else {
    print("Cartridge does not contain battery-packed PRG RAM\n");
  }

  if ((header[6] & 4) == 4) {
    print("512-byte trainer at $7000-$71FF (stored before PRG data)\n");
  } else {
    print("No 512-byte trainer at $7000-$71FF (stored before PRG data)\n");
  }

  if ((header[6] & 8) == 8) {
    print("Ignore mirroring control or above mirroring bit; instead provide four-screen VRAM\n");
  } else {
    print("Do not ignore mirroring control or above mirroring bit\n");
  }

  int mapperNumber = header[7] << 8 | header[6];
  print("mapper number: %d\n", mapperNumber);

  // TODO: check return value of mallocs and callocs
  unsigned char *memory;
  memory = (unsigned char *) malloc(0xFFFF);

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

  // put prgRom into memory at $8000-$BFFF and $C000-$FFFF   https://wiki.nesdev.com/w/index.php/NROM
  // TODO: this shouldn't be a copy, it should be a mirror. It's ROM so it shouldn't matter though.
  memcpy(&memory[0x8000], prgRom, 0x4000);
  memcpy(&memory[0xC000], prgRom, 0x4000);

  // TODO: might as well just read chrRom right into ppuMemory, right?
  memcpy(ppuMemory, chrRom, sizeOfChrRomInBytes);
  struct PPU ppu = { .memory = ppuMemory, .oam = oam, .scanline = -1 };   // TODO: make a struct initializer

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

  print("memory address to start is: %02x%02x\n", memory[0xFFFD], memory[0xFFFC]);
  int memoryAddressToStartAt = (memory[0xFFFD] << 8) | memory[0xFFFC];

  struct KeyboardInput keyboardInput = { .up = false  };
  struct PPUClosure ppuClosure = { .ppu = &ppu, .onMemoryWrite = &onCPUMemoryWrite, .onMemoryRead = &onCPUMemoryRead };

  struct Computer state = { .memory = memory, .keyboardInput = &keyboardInput, .ppuClosure = &ppuClosure };
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
            break;
          case 0x35: // 5
            state.debuggingOn = false;
            break;
        }
      }

      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }

    // TODO: we could move the instruction execution and ppuTick into a game layer. It could tell us
    // when vblank has started so this platform layer can display the frame and sleep.

    uint8_t ppuStatusBefore = ppu.status;

    instr = state.memory[state.pc];
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
      print("# of milliseconds for frame: %f\nframes per second: %f\n", milliseconds, framesPerSecond);

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

