#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "ppu.h"
#include "emu.h"
#include "debug.h"
#include "controller.h"
#include "cartridge.h"
#include <dsound.h>

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

typedef HRESULT directSoundCreate(LPCGUID lpcGuidDevice, LPDIRECTSOUND8 *ppDS8, LPUNKNOWN pUnkOuter);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static int running = 1;

static void *videoBuffer;
static BITMAPINFO bitmapInfo = { 0 };

void dumpNametable(int num, const struct PPU *ppu)
{
  print("dumping!\n\n");
  unsigned char control = ppu->control;
  int baseNametableAddress = 0x2000 + 0x0400 * num;
  
  int tileRow = 0;
  int tileCol = 0;

  FILE *file;
  char filename[30];
  sprintf_s(filename, 30, "nametable-%d.dump", num);
  int fileOpenError = fopen_s(&file, filename, "w+");
  if (fileOpenError) {
    print("Error opening file %s. Error code: %d\n", filename, fileOpenError);
    exit(fileOpenError);
  }

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

static void displayFrame(void *videoBuffer, HWND windowHandle, BITMAPINFO *bitmapInfo) {
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

static void setKeyboardInput(bool *buttonValue, bool wasDown, bool isDown) 
{
  if (wasDown && !isDown) {
    *buttonValue = false;
  }
  if (!wasDown && isDown) {
    *buttonValue = true;
  }
}

void initDirectSound(HWND windowHandle)
{
  HMODULE directSoundLibrary = LoadLibraryA("dsound.dll");

  if (directSoundLibrary) {
    directSoundCreate* create = (directSoundCreate*) GetProcAddress(directSoundLibrary, "DirectSoundCreate8");
    LPDIRECTSOUND8 directSound;

    if (create && create(0, &directSound, 0) == DS_OK) {
      if (IDirectSound_SetCooperativeLevel(directSound, windowHandle, DSSCL_NORMAL) == DS_OK) {
        print("success!\n");

        int nChannels = 1;
        int nSamplesPerSec = 44100;
        int wBitsPerSample = 8;
        int nBlockAlign = (nChannels * wBitsPerSample) / 8;

        WAVEFORMATEX waveFormat = {
          .wFormatTag = WAVE_FORMAT_PCM,
          .nChannels = nChannels,
          .nSamplesPerSec = nSamplesPerSec,
          .nAvgBytesPerSec = nSamplesPerSec * nBlockAlign,
          .nBlockAlign = nBlockAlign,
          .wBitsPerSample = wBitsPerSample
        };

        DSBUFFERDESC bufferDescription = {
          .dwSize = sizeof(bufferDescription),
          .dwFlags = DSBCAPS_GETCURRENTPOSITION2,
          .dwBufferBytes = 3 * nSamplesPerSec * (wBitsPerSample / 8),
          .lpwfxFormat = &waveFormat,
          .guid3DAlgorithm = DS3DALG_DEFAULT
        };

        LPDIRECTSOUNDBUFFER soundBuffer;
        
        if (IDirectSound_CreateSoundBuffer(directSound, &bufferDescription, &soundBuffer, 0) == DS_OK) {
          print("successfully created a sound buffer\n");

          DWORD audioOffset = 0;
          DWORD bytesToWrite = 44100 * 2;  // 2 seconds worth of samples (each sample is 1 byte)
          LPVOID audioPointer1;
          DWORD numAudioBytes1;
          LPVOID audioPointer2;
          DWORD numAudioBytes2;

          if (IDirectSoundBuffer_Lock(soundBuffer, 
              audioOffset,
              bytesToWrite,
              &audioPointer1,
              &numAudioBytes1,
              &audioPointer2,
              &numAudioBytes2,
              0) == DS_OK) {
            print("locked. %d %d\n", numAudioBytes1, numAudioBytes2);

            bool highWave = false;

            // middle C frequency is about 262 Hz
            // so we want 262 cycles every second
            // for us, we have 44100 samples in a second
            // so that's 168.32 samples per cycle
            // which would be 84.16 for a high square, 84.16 for a low square

            int8_t *ptr = (int8_t *) audioPointer1;
            for (DWORD i = 0; i < numAudioBytes1; i++) {
              if (highWave) {
                *ptr = 105;
              } else {
                *ptr = -105;
              }
              if (i % 84 == 0) {
                highWave = !highWave;
              }
              ptr++;
            }

            if (IDirectSoundBuffer_Unlock(soundBuffer,
                audioPointer1,
                numAudioBytes1,
                audioPointer2,
                0) == DS_OK) {
              print("unlocked\n");
            } else {
              print("could not unlock\n");
            }
          } else {
            print("could not lock\n");
          }

          if (IDirectSoundBuffer_Play(soundBuffer, 0, 0, DSBPLAY_LOOPING) != DS_OK) {
            print("Could not play sound.\n");
          }

        } else {
          print("Could not create a sound buffer\n");
        }
      } else {
        print("Could not set DirectSound cooperative level.");
      }
    } else {
      print("Could not initialize DirectSound.\n");
    }
  } else {
    print("Could not find DirectSound DLL.\n");
  }
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
  LARGE_INTEGER perfFrequencyResult;
  QueryPerformanceFrequency(&perfFrequencyResult);
  int64_t perfFrequency = perfFrequencyResult.QuadPart;

  // TODO: check return value to see if it worked
  timeBeginPeriod(1);

  char gameFile[] = "MegaMan2.nes";
  // char gameFile[] = "donkey_kong.nes";
  // char gameFile[] = "Excitebike.nes";

  // char gameFile[] = "01-basics.nes";
  // char gameFile[] = "06-right_edge.nes";

  struct Cartridge *cartridge;
  int loadCartridgeError = loadCartridge(&cartridge, gameFile);
  if (loadCartridgeError) {
    print("Error loading cartridge: %d\n", loadCartridgeError);
    exit(loadCartridgeError);
  }

  struct PPU *ppu;
  int ppuCreationError = createPPU(&ppu, cartridge);
  if (ppuCreationError) {
    print("Error creating the PPU");
    exit(ppuCreationError);
  }

  struct Color palette[64];
  loadPalette(palette);

  videoBuffer = calloc(VIDEO_BUFFER_WIDTH * VIDEO_BUFFER_HEIGHT, 4);
  if (!videoBuffer) {
    print("Error creating the video buffer");
    exit(1);
  }

  // Set up bitmap info
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

  initDirectSound(windowHandle);

  ShowWindow(windowHandle, nShowCmd);

  uint32_t loopCount = 0;

  struct KeyboardInput keyboardInput = { .up = false  };
  struct PPUClosure ppuClosure;
  buildPPUClosure(&ppuClosure, ppu);

  uint8_t *memory = (uint8_t *) malloc(0x8000 + cartridge->sizeOfPrgRomInBytes);
  if (!memory) {
    print("Could not initialize main memory block.");
    return 1;
  }

  // copy prgRom starting at 0x8000, even if it's bigger than 32 kB. We will intercept reads
  // from this memory and do the right thing based on MMC.
  memcpy(&memory[0x8000], cartridge->prgRom, cartridge->sizeOfPrgRomInBytes);

  struct Computer state = { .memory = memory, .keyboardInput = &keyboardInput, .ppuClosure = &ppuClosure };

  if (cartridge->mapperNumber == 0) {
    state.prgRomBlock1 = &memory[0x8000];
    state.prgRomBlock2 = &memory[0xA000];
    if (cartridge->sizeOfPrgRomInBytes == 0x8000) {
      // NROM-256
      state.prgRomBlock3 = &memory[0xC000];
      state.prgRomBlock4 = &memory[0xE000];
    } else {
      // for NROM-128 the second 16 kB is a mirror of the first 16 kB
      state.prgRomBlock3 = &memory[0x8000];
      state.prgRomBlock4 = &memory[0xA000];
    }
  } else if (cartridge->mapperNumber == 1) {
    // For mapper 1 it seems to be important to start off with the last 16 kB bank in 0xC000 - 0xFFFF
    state.prgRomBlock1 = &memory[0x8000];
    state.prgRomBlock2 = &memory[0xA000];
    int addressOfLastBank = 0x8000 + ((cartridge->numPrgRomUnits - 1) * 0x4000); 
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
        print("going to quit");
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
            dumpOam(1, ppu->oam);
            break;
          case 0x34: // 4
            state.debuggingOn = true;
            ppu->debuggingOn = true;
            break;
          case 0x35: // 5
            state.debuggingOn = false;
            ppu->debuggingOn = false;
            break;
        }
      }

      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }

    bool vblankStarted = executeEmulatorCycle(&state, ppu, videoBuffer, palette);

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
  free(ppu->memory);
  free(ppu->oam);
  free(ppu);
  free(cartridge->prgRom);
  free(cartridge->chrRom);
  free(cartridge);
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

