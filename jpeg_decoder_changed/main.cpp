
#include <Windows.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "MiuJpegDecoder.h"

#pragma comment(linker,"/subsystem:\"console\" /entry:\"WinMainCRTStartup\"")

#define                 WINDOW_CLASS                    L"JpegDecoderWindow"
#define                 WINDOW_TITLE                    L"JpegDecoderWindow"
#define                 WINDOW_WIDTH                    1000
#define                 WINDOW_HEIGHT                   700
#define                 WINDOW_X                        300
#define                 WINDOW_Y                        200

// 隐藏光标
void hideCursor() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hOut, &cursorInfo);
    cursorInfo.bVisible = FALSE;  // 设置光标为不可见
    SetConsoleCursorInfo(hOut, &cursorInfo);
}
// 显示光标
void showCursor() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hOut, &cursorInfo);
    cursorInfo.bVisible = TRUE;  // 设置光标为可见
    SetConsoleCursorInfo(hOut, &cursorInfo);
}

HWND g_hWnd = nullptr;
std::mutex mtx;
std::condition_variable cv;
extern const char* file;
VOID Draw(HWND hWnd, int width, int height, COLORREF* colRGB)
{
    HDC m_hdc = GetDC(hWnd);
    HDC hCompatibleDC = CreateCompatibleDC(m_hdc);


    HBITMAP hCustomBmp = NULL;
    hCustomBmp = CreateCompatibleBitmap(m_hdc, width, height);
    SelectObject(hCompatibleDC, hCustomBmp);

    BITMAPINFO bmpInfo = { 0 };
    bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmpInfo.bmiHeader.biWidth = width;
    bmpInfo.bmiHeader.biHeight = -(int)height;
    bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biCompression = BI_RGB;
    bmpInfo.bmiHeader.biBitCount = 32;

    SetDIBits(NULL, hCustomBmp, 0, height, colRGB, &bmpInfo, DIB_RGB_COLORS);
    SelectObject(hCompatibleDC, hCustomBmp);
    BitBlt(m_hdc, 0, 0, width, height, hCompatibleDC, 0, 0, SRCCOPY);
    ReleaseDC(hWnd, m_hdc);
}
unsigned long offset = 127;
bool running = true;
std::thread global_thread;

LRESULT CALLBACK JpegDecoderWindowProc(
    HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        LPCREATESTRUCT pc = (LPCREATESTRUCT)lParam;
        global_thread = std::thread([&]() {

            HWND m_hWnd = hWnd;

            char str[128] = "";
            sprintf_s(str, "offset:%lu You can change it by left key or right key", offset);
            SetWindowTextA(hWnd, str);

            try
            {
                Miu::JpegDecoder jpeg;
                jpeg.debug_offset = offset;
                jpeg.load(file);
                Draw(m_hWnd, jpeg.width(), jpeg.height(), (COLORREF*)jpeg.getRGB());
                jpeg.clear();

                while (running)
                {
                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        cv.wait(lock);
                        if (!running)
                            break;
                    }
                    jpeg.debug_offset = offset;
                    jpeg.load(file);
                    Draw(m_hWnd, jpeg.width(), jpeg.height(), (COLORREF*)jpeg.getRGB());
                    jpeg.clear();
                }
            }
            catch (const std::exception& info)
            {
                MessageBoxA(0, info.what(), "Error", MB_ICONWARNING);
            }
        });
    }
        break;
    case WM_KEYDOWN:
    {
        if (wParam == VK_LEFT)
        {
            if(offset > 0) offset -= 1;
            cv.notify_all();
        }
        else if (wParam == VK_RIGHT)
        {
            if(offset < 255) offset += 1;
            cv.notify_all();
        }

        char str[128] = "";
        sprintf_s(str, "offset:%lu", offset);
        SetWindowTextA(hWnd, str);
    }
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        cv.notify_all();
        
        EndPaint(hWnd, &ps);
    }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd
)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    WNDCLASSEX wnd = { 0 };
    wnd.cbSize = sizeof(wnd);
    wnd.hInstance = GetModuleHandleA(0);
    wnd.lpszClassName = L"JpegDecoderWindow";
    wnd.cbClsExtra = 0;
    wnd.cbWndExtra = 0;
    wnd.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
    wnd.hIcon = LoadIcon(NULL, IDC_ICON);
    wnd.lpfnWndProc = JpegDecoderWindowProc;
    wnd.lpszMenuName = 0;
    wnd.style = CS_HREDRAW;

    RegisterClassEx(&wnd);

    HWND hWindow = CreateWindow(
        WINDOW_CLASS,
        WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        WINDOW_X,
        WINDOW_Y,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        NULL,
        NULL,
        wnd.hInstance,
        0
    );

    g_hWnd = hWindow;

    ShowWindow(hWindow, SW_SHOW);
    UpdateWindow(hWindow);

    MSG msg;
    while (GetMessage(&msg, 0, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    running = false;
    cv.notify_all();
    global_thread.join();

    return 0;
}
