#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "ppu.h"
#include "emu.h"
#include "controller.h"

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

void *videoBuffer;
BITMAPINFO bitmapInfo = { 0 };

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

  // TODO: might as well just read chrRom right into ppuMemory, right?
  memcpy(ppuMemory, chrRom, sizeOfChrRomInBytes);
  struct PPU ppu = { .memory = ppuMemory, .oam = oam, .scanline = -1, .mapperNumber = mapperNumber };   // TODO: make a struct initializer
  ppu.sprites = ppu.sprites0;
  ppu.followingSprites = ppu.sprites1;
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
  struct PPUClosure ppuClosure;
  buildPPUClosure(&ppuClosure, &ppu);

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

    bool vblankStarted = executeEmulatorCycle(&state, &ppu, videoBuffer, palette);

    instructionsExecuted++;
    loopCount++;

    if (vblankStarted) {
      LARGE_INTEGER midPerfCount;
      QueryPerformanceCounter(&midPerfCount);
      int64_t midPerfDiff = midPerfCount.QuadPart - lastPerfCount.QuadPart;
      double millisecondsElapsed = (1000.0f*(double)midPerfDiff) / (double)perfFrequency;

      if (millisecondsElapsed < 16) {
        DWORD sleepTime = (16 - (DWORD)millisecondsElapsed);
        if (state.debuggingOn) {
          print("sleep for %d milliseconds\n", sleepTime);
        }
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

