#include <windows.h>
#include <stdint.h>

// helpful: https://docs.microsoft.com/en-us/windows/win32/learnwin32/your-first-windows-program

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int running = 1;

#define VIDEO_BUFFER_WIDTH 256
#define VIDEO_BUFFER_HEIGHT 240

void *videoBuffer;
BITMAPINFO bitmapInfo = { 0 };

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
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

  int xOffset = 0;
  int yOffset = 0;
  uint32_t loopCount = 0;

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

    if (loopCount % 30 == 0)
    {
      xOffset++;
      yOffset++;
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

