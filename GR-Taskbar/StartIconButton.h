#pragma once
#include <oleacc.h>
#pragma comment(lib, "Oleacc.lib")

// The native pearl painter caches and recolors the normal glyph, and paints its
// highlight separately. Replace only its paint messages, leaving activation,
// accessibility, focus, keyboard handling and context menus with Windows.
static BOOL GRHasCustomStartArtwork(void)
{
    AcquireSRWLockShared(&customOrbLock);
    BOOL available = customOrb.bitmap != NULL;
    ReleaseSRWLockShared(&customOrbLock);
    return bOldTaskbar && available && (dwOrbStyle == ORB_STYLE_CUSTOM ||
        dwOrbStyle == ORB_STYLE_WINDOWS10 || dwOrbStyle == ORB_STYLE_WINDOWS11 ||
        dwOrbStyle == ORB_STYLE_WINDOWS7 || dwOrbStyle == ORB_STYLE_WINDOWSXP);
}

static int GRNativeStartVisualState(HWND hwnd)
{
    LONG flags = 0;
    IAccessible* accessible = NULL;
    if (SUCCEEDED(AccessibleObjectFromWindow(hwnd, OBJID_CLIENT, &IID_IAccessible, (void**)&accessible)) && accessible)
    {
        VARIANT child, state;
        VariantInit(&child); VariantInit(&state);
        child.vt = VT_I4; child.lVal = CHILDID_SELF;
        if (SUCCEEDED(accessible->lpVtbl->get_accState(accessible, child, &state)) && state.vt == VT_I4)
            flags = state.lVal;
        VariantClear(&state);
        accessible->lpVtbl->Release(accessible);
    }
    if (flags & (STATE_SYSTEM_PRESSED | STATE_SYSTEM_CHECKED)) return 3;
    // Some native shell builds omit HOTTRACKED in MSAA. Consult the pointer
    // only during a requested paint; no timer, polling thread or input hooks.
    POINT cursor; RECT bounds;
    BOOL over = GetCursorPos(&cursor) && GetWindowRect(hwnd, &bounds) &&
        PtInRect(&bounds, cursor) && WindowFromPoint(cursor) == hwnd;
    if (over && (GetKeyState(VK_LBUTTON) & 0x8000)) return 3;
    return (flags & STATE_SYSTEM_HOTTRACKED) || over ? 2 : 1;
}

static BOOL GRPaintStartArtwork(HWND hwnd, HDC dc)
{
    RECT client;
    if (!dc || !GetClientRect(hwnd, &client)) return FALSE;
    int saved = SaveDC(dc);
    if (!saved) return FALSE;
    IntersectClipRect(dc, client.left, client.top, client.right, client.bottom);
    // Paint the taskbar surface, never the native Start button highlight.
    HRESULT background = DrawThemeParentBackground(hwnd, dc, &client);
    if (FAILED(background)) FillRect(dc, &client, GetSysColorBrush(COLOR_3DFACE));
    OrbInfo orb = { NULL, hwnd, FALSE, GetDpiForWindow(hwnd) };
    int state = GRNativeStartVisualState(hwnd);
    BOOL drawn = DrawCustomOrb(dc, &client, &client, 0, state, &orb);
    GRStartIconTrace(L"button-paint hwnd=%p state=%d frame=%d background=0x%08lX drawn=%d",
        hwnd, state, GRStartIconFrame(state), (ULONG)background, drawn);
    RestoreDC(dc, saved);
    return drawn;
}

static LRESULT CALLBACK GRStartArtworkSubclass(HWND hwnd, UINT message, WPARAM wParam,
    LPARAM lParam, UINT_PTR id, DWORD_PTR reference)
{
    (void)reference;
    if (message == WM_NCDESTROY)
    {
        RemoveWindowSubclass(hwnd, GRStartArtworkSubclass, id);
        return DefSubclassProc(hwnd, message, wParam, lParam);
    }
    if (GRHasCustomStartArtwork())
    {
        if (message == WM_ERASEBKGND) return 1;
        if (message == WM_PAINT)
        {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(hwnd, &paint);
            BOOL drawn = GRPaintStartArtwork(hwnd, dc);
            EndPaint(hwnd, &paint);
            if (drawn) return 0;
            // Allocation/render failures restore native painting, never leave
            // an invalid region permanently blank.
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (message == WM_PRINTCLIENT)
        {
            if (GRPaintStartArtwork(hwnd, (HDC)wParam)) return 0;
        }
    }
    LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
    if (message == WM_MOUSEMOVE || message == WM_MOUSELEAVE || message == WM_LBUTTONDOWN ||
        message == WM_LBUTTONUP || message == WM_CAPTURECHANGED || message == WM_ENABLE ||
        message == WM_SETFOCUS || message == WM_KILLFOCUS || message == BM_SETSTATE ||
        message == WM_KEYDOWN || message == WM_KEYUP || message == WM_UPDATEUISTATE)
        if (GRHasCustomStartArtwork()) InvalidateRect(hwnd, NULL, FALSE);
    return result;
}

static void GRInstallStartArtworkPainter(HWND themeWindow)
{
    HWND start = GRResolveStartButton(themeWindow, NULL);
    if (start && GetWindowThreadProcessId(start, NULL) == GetCurrentThreadId())
        SetWindowSubclass(start, GRStartArtworkSubclass, 0x47525349, 0);
}
