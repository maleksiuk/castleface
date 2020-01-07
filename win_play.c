#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// helpful: https://docs.microsoft.com/en-us/windows/win32/learnwin32/your-first-windows-program

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int running = 1;

#define VIDEO_BUFFER_WIDTH 256
#define VIDEO_BUFFER_HEIGHT 240

void *videoBuffer;
BITMAPINFO bitmapInfo = { 0 };

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    // TODO: read header into its own space, then prg rom into another space
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

  int backgroundImageStart = 0x1000;

  while(running)
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


    // https://wiki.nesdev.com/w/index.php/PPU_pattern_tables
    for (int row = 0; row < 8; row++)
    {
      for (int col = 0; col < 8; col++)
      {
        int bit = 7 - col;

        unsigned char bit1 = (chrRom[backgroundImageStart+8+row] >> bit & 0x01) != 0;
        unsigned char bit0 = (chrRom[backgroundImageStart+row] >> bit & 0x01) != 0;

        uint8_t *videoBufferRow = (uint8_t *)videoBuffer;
        videoBufferRow = videoBufferRow + (row * VIDEO_BUFFER_WIDTH * 4);

        uint32_t *pixel = (uint32_t *)videoBufferRow;
        pixel += col;

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
    uint8_t *row = (uint8_t *)videoBuffer;    
    for (int y = 0; y < VIDEO_BUFFER_HEIGHT; y++)
    {
        uint32_t *pixel = (uint32_t *)row;
        for(int x = 0; x < VIDEO_BUFFER_WIDTH; x++)
        {
            uint8_t blue = (x + xOffset);
            uint8_t green = (y + yOffset);

            *pixel++ = ((green << 8) | blue);
        }
        
        row += (VIDEO_BUFFER_WIDTH * 4);
    }
    */

    if (loopCount % 600 == 0)
    {
      backgroundImageStart += 16;
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

