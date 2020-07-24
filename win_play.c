#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"

// helpful: https://docs.microsoft.com/en-us/windows/win32/learnwin32/your-first-windows-program

/*
 * Possible next steps:
 *  - make interrupts work. Look at the Klaus interrupt test.
 *  - break out a PPU struct and put the chrRom in its memory.
 *  - make gates 2006/2007 on the PPU actually fill PPU memory.
 */

/*
 *
 
 What if I start by rendering 3x pixels where x is the number of cycles an instruction
 took for the CPU to do? That might be ok for Donkey Kong and will at least
 get me started.

 So my plan could be:
  - CPU to return number of cycles.
  - Mapping PPU registers so the CPU's writes to them actually do stuff.
  - Interrupt stuff (probably).
  - Write a PPU function to render the next N pixels. 

 */

// the 6502 has 256 byte pages

// might be helpful for PPU implementation:
// - http://nesdev.com/NESDoc.pdf
// - https://forums.nesdev.com/viewtopic.php?p=157086&sid=67b5e4517ef101b69e0c9d1286eeda16#p157086 
// - https://forums.nesdev.com/viewtopic.php?p=157167#p157167
// - http://nesdev.com/NES%20emulator%20development%20guide.txt
// - https://www.reddit.com/r/EmuDev/comments/7k08b9/not_sure_where_to_start_with_the_nes_ppu/drapgie/
// - https://www.dustmop.io/blog/2015/12/18/nes-graphics-part-3/
// - http://www.michaelburge.us/2019/03/18/nes-design.html#ppu

// - got the kingswood assembler from http://web.archive.org/web/20190301123637/http://www.kingswood-consulting.co.uk/assemblers/
//   (but then learned that it also comes with the Klaus functional tests)

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int running = 1;

#define VIDEO_BUFFER_WIDTH 256
#define VIDEO_BUFFER_HEIGHT 240

void *videoBuffer;
BITMAPINFO bitmapInfo = { 0 };


void renderToVideoBuffer(char *chrRom) 
{
  int baseNametableAddress = 0x2000; // because of value read from $2000 (bits 0 and 1); but this will change over time I bet
  // int spriteImageStart = 0x0000;  // because of value read from $2000 (bit 3)
  int backgroundImageStart = 0x1000;  // because of value read from $2000 (bit 4)

  int tileRow = 0;
  int tileCol = 0;
  int tileColPixel = 0;

  int nametableByteIndex = 0;
  for (nametableByteIndex = 0; nametableByteIndex < 0x3C0; nametableByteIndex += 1)
  {
    // if nametableByteIndex = 5, it should be tileRow 0 and tileCol 4
    // if nametableByteIndex = 35, it should be tileRow 1 and tileCol 3
    // if nametableByteIndex = 70, it should be tileRow 2 and tileCol 5
    tileRow = nametableByteIndex / 32;
    tileCol = nametableByteIndex % 32;

    /*
     *
     * Future MIKE: I think we're making a mistake here... we're getting val from the chrRom.
     * But really we need to load chrRom into ppumemory $0000-$1FFF, and then $2000
     * will be the base nametable address for Donkey Kong, but will be empty I guess
     * until the program fills it.
     *
     * https://wiki.nesdev.com/w/index.php/PPU_memory_map
     *
     * This seems helpful: http://forums.nesdev.com/viewtopic.php?f=3&t=18656
     *
     */

    // int address = baseNametableAddress + nametableByteIndex;
    int address = baseNametableAddress + tileRow*32 + tileCol;

    // if base address is $2000, tileRow = 3, and tileCol = 2
    // we want address $2000 + 3*32 + tileCol, maybe?

    /*char str2[500];*/
    /*sprintf(str2, "index: %d; tileRow %d, tileCol %d, - address in nametable is %x\n", nametableByteIndex, tileRow, tileCol, address);*/
    /*OutputDebugString(str2);*/

    unsigned char val = chrRom[address];

    // super helpful: https://austinmorlan.com/posts/nes_rendering_overview/#nametable

    // I think val is an índex into the background table.

    // 16 because the pattern table comes in 16 byte chunks 
    int addressOfBackgroundTile = backgroundImageStart + (val * 16);

    // temporarily lock it down to one background image. Try to repeat that on every tile.
    // int addressOfBackgroundTile = backgroundImageStart + 16;

    // let's say we're on tileRow 3, tileCol 18 
    // what pixel does it start on?
    // int tilePixel = tileRow * VIDEO_BUFFER_WIDTH + tileCol * 8;

    int tileRowPixel = tileRow * 8;
    tileColPixel = tileCol * 8;

    // chrRom[addressOfBackgroundTile]

    /*char str[500];*/
    /*sprintf(str, "index: %d - about to render background tile at address %x\n", nametableByteIndex, addressOfBackgroundTile);*/
    /*OutputDebugString(str);*/

    // these row/col are within the 8x8 tile
    for (int row = 0; row < 8; row++)
    {
      for (int col = 0; col < 8; col++)
      {
        int bit = 7 - col;

        unsigned char bit1 = (chrRom[addressOfBackgroundTile+8+row] >> bit & 0x01) != 0;
        unsigned char bit0 = (chrRom[addressOfBackgroundTile+row] >> bit & 0x01) != 0;

        uint8_t *videoBufferRow = (uint8_t *)videoBuffer;
        videoBufferRow = videoBufferRow + ((tileRowPixel + row) * VIDEO_BUFFER_WIDTH * 4);

        // videoBuffer is 256 x 240 

        uint32_t *pixel = (uint32_t *)(videoBufferRow);
        pixel += (tileColPixel + col);
        /*char str[500];*/
        /*sprintf(str, "pixel - %d %d\n", (tileRowPixel + row), tileColPixel + col);*/
        /*OutputDebugString(str);*/

        int val = bit1 << 1 | bit0;
        uint8_t red = 0;
        uint8_t green = 0;
        uint8_t blue = 0;
        if (val == 0)
        {
        }
        else if (val == 1)
        {
          blue = 0xFF;
        }
        else if (val == 2)
        {
          green = 0xFF;
        }
        else if (val == 3)
        {
          red = 0xFF;
        }

        *pixel = ((red << 16) | (green << 8) | blue);
      }
    }

    /*
       char str[500];
       sprintf(str, "nametableByteIndex: %d; tileRow %d, tileCol %d -- %x: %02x; address of background tile: %x\n", nametableByteIndex, tileRow, tileCol, address, val, addressOfBackgroundTile);
       OutputDebugString(str);
       */
  }
}


int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
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

  // put prgRom into memory at $8000-$BFFF and $C000-$FFFF   https://wiki.nesdev.com/w/index.php/NROM
  // TODO: this shouldn't be a copy, it should be a mirror. It's ROM so it shouldn't matter though.
  memcpy(&memory[0x8000], prgRom, 0x4000);
  memcpy(&memory[0xC000], prgRom, 0x4000);

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
  HWND windowHandle = CreateWindowExA(
      0,                              // Optional window styles.
      CLASS_NAME,                     // Window class
      "Castleface",     // Window text
      WS_OVERLAPPEDWINDOW,            // Window style

      // Size and position
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

      NULL,       // Parent window    
      NULL,       // Menu
      hInstance,  // Instance handle
      NULL        // Additional application data
      );

  if (windowHandle == NULL)
  {
    return 0;
  }

  ShowWindow(windowHandle, nShowCmd);

  uint32_t loopCount = 0;

  printf("memory address to start is: %02x%02x\n", memory[0xFFFD], memory[0xFFFC]);
  int memoryAddressToStartAt = (memory[0xFFFD] << 8) | memory[0xFFFC];

  struct Computer state = { memory, 0, 0, 0, 0, 0, 0, 0, 0 };
  state.pc = memoryAddressToStartAt;

  int instructionsExecuted = 0;
  unsigned char instr = 0;

  int instructionLimit = 18000;

  // what I'm seeing Donkey Kong do:
  //
  // instr 6169: finishes setting a ton of memory to 0
  // instr 6179: sets 2001 to 06
  // instr 8037: sets 2006 to 20
  // instr 8039: sets 2006 to 00 (so sets ppu address to $2000)
  // instr 8044: starts setting 2007 to 24 repeatedly to fill the nametable
  // instr 11125: sets 2006 to 23
  // instr 11127: sets 2006 to c0
  // instr 11130: sets 2007 back to 00 (finishes setting it to 24 around here)
  // instr 11334: sets 2000 to 90 (10010000)

  while(running && state.pc < 0xFFFF)
  {
    MSG msg = { 0 };
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_LBUTTONDOWN)
      {
        OutputDebugString("left button down");
      }

      if (msg.message == WM_QUIT)
      {
        OutputDebugString("going to quit");
        running = 0;
      }

      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }

    // just temporarily slow its roll
    if (loopCount % 50 == 0) {
      renderToVideoBuffer(chrRom);
    }

  // Donkey Kong is setting 2000 (first ppu reg) to 00010000, indicating that the background table address is $1000 (in the CHR ROM I believe)
  //    https://wiki.nesdev.com/w/index.php/PPU_pattern_tables
  //    https://wiki.nesdev.com/w/index.php/PPU_registers#Controller_.28.242000.29_.3E_write

  // Donkey Kong is waiting for 2002 to have the 7th bit set (i.e. "vertical blank has started")

    instr = state.memory[state.pc];

    if (instructionsExecuted < instructionLimit) 
    {
      executeInstruction(instr, &state);

      char str[500];
      sprintf(str, "(instr %d): PPU registers: %02x %02x %02x %02x %02x %02x %02x %02x\n", instructionsExecuted, state.memory[0x2000], state.memory[0x2001], state.memory[0x2002], state.memory[0x2003], state.memory[0x2004], state.memory[0x2005], state.memory[0x2006], state.memory[0x2007]);
      OutputDebugString(str);
    }

    if (instructionsExecuted == instructionLimit)
    {
      OutputDebugString("no further instructions executed because limit hit\n");
    }

    instructionsExecuted++;

    // pretend vertical blank has started just to see what happens next
    if (instructionsExecuted == 5)
    {
      OutputDebugString("setting memory address 2002 to 0x80");
      state.memory[0x2002] = 0x80; 
    }

    HDC deviceContext = GetDC(windowHandle);

    RECT clientRect;
    GetClientRect(windowHandle, &clientRect);
    int windowWidth = clientRect.right - clientRect.left;
    int windowHeight = clientRect.bottom - clientRect.top;

    StretchDIBits(deviceContext,
      0, 0, windowWidth, windowHeight,
      0, 0, VIDEO_BUFFER_WIDTH, VIDEO_BUFFER_HEIGHT,
      videoBuffer,
      &bitmapInfo,
      DIB_RGB_COLORS, SRCCOPY);

    ReleaseDC(windowHandle, deviceContext);

    loopCount++;
  }

  free(videoBuffer);
  free(memory);
  free(prgRom);
  free(chrRom);
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

