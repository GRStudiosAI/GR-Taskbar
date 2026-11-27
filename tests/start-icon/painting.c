#define COBJMACROS
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <oleacc.h>
#include <stdio.h>
#include "../../GR-Taskbar/CustomOrb.h"
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

static BOOL bOldTaskbar = TRUE;
static DWORD dwOrbStyle = ORB_STYLE_CUSTOM;
#define ORB_STYLE_WINDOWS10 0
#define ORB_STYLE_WINDOWS11 1
#define ORB_STYLE_TRANSPARENT 2
static CustomOrbImage customOrb;
static SRWLOCK customOrbLock = SRWLOCK_INIT;
static GRStartIconRenderCache cache;
typedef struct OrbInfo { HTHEME hTheme; HWND hwnd; BOOL taskbarBackground; UINT dpi; } OrbInfo;
static LONG accessibleState;
static ULONG STDMETHODCALLTYPE ReleaseAccessible(IAccessible* self) { (void)self; return 1; }
static HRESULT STDMETHODCALLTYPE ReadAccessible(IAccessible* self, VARIANT child, VARIANT* result)
{ (void)self; (void)child; result->vt = VT_I4; result->lVal = accessibleState; return S_OK; }
static IAccessibleVtbl accessibleMethods;
static IAccessible accessible = { &accessibleMethods };
static HRESULT TestAccessible(HWND hwnd, DWORD object, REFIID iid, void** result)
{ (void)hwnd; (void)object; (void)iid; *result = &accessible; return S_OK; }
#define AccessibleObjectFromWindow TestAccessible
#define GRStartIconTrace(...) ((void)0)
static HWND GRResolveStartButton(HWND hwnd, HDC dc) { (void)dc; return hwnd; }
static BOOL DrawCustomOrb(HDC dc, LPCRECT rect, LPCRECT clip, int part, int state, const OrbInfo* orb)
{
    (void)clip; (void)part; (void)orb;
    GRStartIconSizing sizing = { 2, 24 };
    RECT target = GRStartIconLayout(*rect, customOrb.contentBounds, 96, sizing);
    return GRDrawOrbFrame(dc, &customOrb, &cache, target, GRStartIconFrame(state));
}
#include "../../GR-Taskbar/StartIconButton.h"

static int nativePaints, nativeClicks, failures;
#define CHECK(condition) do { if (!(condition)) { printf("FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)
static LRESULT CALLBACK NativeButton(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_PRINTCLIENT) {
        RECT bounds = { 0, 0, 40, 40 }; HBRUSH blue = CreateSolidBrush(RGB(0, 0, 255));
        FillRect((HDC)wParam, &bounds, blue); DeleteObject(blue); ++nativePaints; return 0;
    }
    if (message == WM_LBUTTONUP) { ++nativeClicks; return 123; }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int main(void)
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    accessibleMethods.Release = ReleaseAccessible; accessibleMethods.get_accState = ReadAccessible;
    WNDCLASSW wc = { 0 }; wc.lpfnWndProc = NativeButton; wc.lpszClassName = L"GRArtworkPaintTest";
    wc.hInstance = GetModuleHandleW(NULL); CHECK(RegisterClassW(&wc));
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Test", WS_POPUP, 0, 0, 40, 40, NULL, NULL, wc.hInstance, NULL);
    CHECK(hwnd != NULL); GRInstallStartArtworkPainter(hwnd);
    BITMAPINFO info = { 0 }; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 8; info.bmiHeader.biHeight = -24;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    DWORD* pixels;
    customOrb.bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);
    CHECK(customOrb.bitmap != NULL); customOrb.width = customOrb.frameHeight = 8;
    customOrb.contentBounds = (RECT){ 0, 0, 8, 8 };
    DWORD colors[] = { 0xFFFF0000, 0xFF00FF00, 0xFFFF00FF };
    ZeroMemory(pixels, 8 * 24 * 4);
    for (int frame = 0; frame < 3; ++frame)
        for (int y = 2; y < 6; ++y) for (int x = 2; x < 6; ++x) pixels[(frame * 8 + y) * 8 + x] = colors[frame];
    HDC dc = CreateCompatibleDC(NULL);
    info.bmiHeader.biWidth = 40; info.bmiHeader.biHeight = -40;
    DWORD* destination;
    HBITMAP bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void**)&destination, NULL, 0);
    HGDIOBJ old = SelectObject(dc, bitmap);
    LONG states[] = { 0, STATE_SYSTEM_HOTTRACKED, STATE_SYSTEM_PRESSED, STATE_SYSTEM_CHECKED };
    for (int i = 0; i < 4; ++i) {
        accessibleState = states[i];
        SendMessageW(hwnd, WM_PRINTCLIENT, (WPARAM)dc, PRF_CLIENT);
        GdiFlush();
        CHECK((destination[20 * 40 + 20] & 0xFFFFFF) == (colors[min(i, 2)] & 0xFFFFFF));
        CHECK((destination[0] & 0xFFFFFF) != 0x0000FF);
        CHECK(nativePaints == 0);
    }
    CHECK(SendMessageW(hwnd, WM_LBUTTONUP, 0, 0) == 123); CHECK(nativeClicks == 1);
    DWORD gdiBefore = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    ULONGLONG started = GetTickCount64();
    for (int repeat = 0; repeat < 1000; ++repeat) {
        accessibleState = states[repeat % 4];
        SendMessageW(hwnd, WM_PRINTCLIENT, (WPARAM)dc, PRF_CLIENT);
    }
    GdiFlush();
    CHECK(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) == gdiBefore);
    printf("1000 state paints: %llu ms, no GDI resource growth\n", GetTickCount64() - started);
    dwOrbStyle = ORB_STYLE_TRANSPARENT;
    SendMessageW(hwnd, WM_PRINTCLIENT, (WPARAM)dc, PRF_CLIENT); CHECK(nativePaints == 0);
    dwOrbStyle = 99;
    SendMessageW(hwnd, WM_PRINTCLIENT, (WPARAM)dc, PRF_CLIENT); CHECK(nativePaints == 1);
    for (UINT dpi = 96; dpi <= 192; dpi += 24) {
        RECT button = { 0, 0, MulDiv(40, dpi, 96), MulDiv(40, dpi, 96) };
        GRStartIconSizing sizing = { 0, 24 };
        RECT target = GRStartIconLayout(button, customOrb.contentBounds, dpi, sizing);
        CHECK(target.right - target.left == MulDiv(24, dpi, 96));
        CHECK(abs((target.left + target.right) - button.right) <= 1);
    }
    DestroyWindow(hwnd); SelectObject(dc, old); DeleteObject(bitmap); DeleteDC(dc);
    GRClearStartIconCache(&cache); DeleteObject(customOrb.bitmap); CoUninitialize();
    printf("Start icon painting: %d failures\n", failures); return failures ? 1 : 0;
}
