#pragma once

// Original sizing/layout modifications: GR Studios AI LLC, 2026-11-25.
// All dimensions saved in settings are logical pixels (96 DPI).
#define GR_START_ICON_SIZE_MODE L"StartIconSizeMode"
#define GR_START_ICON_CUSTOM_SIZE L"StartIconCustomSize"
#define GR_START_ICON_DEFAULT_SIZE 24
#define GR_START_ICON_MIN_SIZE 16
#define GR_START_ICON_MAX_SIZE 48

typedef struct GRStartIconSizing
{
    DWORD mode;       // 0 Automatic, 1 Small, 2 Medium, 3 Large, 4 Custom
    DWORD customSize;
} GRStartIconSizing;

static GRStartIconSizing GRReadStartIconSizing(void)
{
    GRStartIconSizing settings = { 0, GR_START_ICON_DEFAULT_SIZE };
    DWORD bytes = sizeof(DWORD);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\GR-Taskbar", GR_START_ICON_SIZE_MODE,
        RRF_RT_REG_DWORD, NULL, &settings.mode, &bytes);
    bytes = sizeof(DWORD);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\GR-Taskbar", GR_START_ICON_CUSTOM_SIZE,
        RRF_RT_REG_DWORD, NULL, &settings.customSize, &bytes);
    if (settings.mode > 4) settings.mode = 0;
    settings.customSize = max(GR_START_ICON_MIN_SIZE, min(GR_START_ICON_MAX_SIZE, settings.customSize));
    return settings;
}

static RECT GRStartIconLayout(RECT button, RECT content, UINT dpi, GRStartIconSizing settings)
{
    RECT destination = { button.left, button.top, button.left, button.top };
    int buttonWidth = button.right - button.left, buttonHeight = button.bottom - button.top;
    int contentWidth = content.right - content.left, contentHeight = content.bottom - content.top;
    if (buttonWidth <= 0 || buttonHeight <= 0 || contentWidth <= 0 || contentHeight <= 0) return destination;
    if (!dpi || dpi > 1536) dpi = 96;
    int padding = max(1, MulDiv(2, (int)dpi, 96));
    int available = min(buttonWidth, buttonHeight) - 2 * padding;
    if (available <= 0) available = min(buttonWidth, buttonHeight);
    int logical;
    switch (settings.mode)
    {
    case 1: logical = 16; break;
    case 2: logical = 24; break;
    case 3: logical = 32; break;
    case 4: logical = (int)max(16, min(48, settings.customSize)); break;
    default:
        // Standard 40-logical-pixel taskbars use a 32-logical-pixel icon.
        // Smaller/larger buttons adapt without depending on orientation.
        logical = max(16, min(48, MulDiv(min(buttonWidth, buttonHeight), 96 * 4, (int)dpi * 5)));
        break;
    }
    int side = max(1, min(available, MulDiv(logical, (int)dpi, 96)));
    int longest = max(contentWidth, contentHeight);
    int width = max(1, MulDiv(side, contentWidth, longest));
    int height = max(1, MulDiv(side, contentHeight, longest));
    destination.left = button.left + (buttonWidth - width) / 2;
    destination.top = button.top + (buttonHeight - height) / 2;
    destination.right = destination.left + width;
    destination.bottom = destination.top + height;
    return destination;
}

static int GRStartIconFrame(int themeState)
{
    // Pressed and checked/active buttons retain their pressed artwork.
    return themeState == 3 || themeState == 5 || themeState == 6 ? 2 : themeState == 2 ? 1 : 0;
}

static RECT GRStartIconButtonBounds(HDC dc, HWND themeWindow, RECT glyph, UINT* dpi)
{
    RECT button = glyph;
    HWND paintWindow = WindowFromDC(dc);
    HWND hwnd = IsWindow(themeWindow) ? themeWindow : paintWindow;
    if (hwnd)
    {
        UINT currentDpi = GetDpiForWindow(hwnd);
        if (currentDpi) *dpi = currentDpi;
        RECT client;
        if (GetClientRect(hwnd, &client) && client.right > 0 && client.bottom > 0 &&
            client.right <= MulDiv(200, (int)*dpi, 96) && client.bottom <= MulDiv(200, (int)*dpi, 96))
        {
            if (paintWindow)
            {
                MapWindowPoints(hwnd, paintWindow, (POINT*)&client, 2);
                DPtoLP(dc, (POINT*)&client, 2);
                button = client;
            }
            else
            {
                int centerX = glyph.left + (glyph.right - glyph.left) / 2;
                int centerY = glyph.top + (glyph.bottom - glyph.top) / 2;
                button.left = centerX - client.right / 2;
                button.top = centerY - client.bottom / 2;
                button.right = button.left + client.right;
                button.bottom = button.top + client.bottom;
            }
            return button;
        }
    }
    // A buffered paint DC can expose a button-sized backing DIB even when the
    // theme was opened without an HWND. Never mistake a whole-taskbar buffer
    // for a button or draw outside the actual backing bitmap.
    if (!paintWindow)
    {
        BITMAP bitmap = { 0 };
        HGDIOBJ selected = GetCurrentObject(dc, OBJ_BITMAP);
        if (selected && GetObjectW(selected, (int)sizeof(bitmap), &bitmap) == (int)sizeof(bitmap) &&
            bitmap.bmWidth >= glyph.right - glyph.left && bitmap.bmHeight >= glyph.bottom - glyph.top &&
            bitmap.bmWidth <= MulDiv(200, (int)*dpi, 96) && bitmap.bmHeight <= MulDiv(200, (int)*dpi, 96))
        {
            button = (RECT){ 0, 0, bitmap.bmWidth, bitmap.bmHeight };
            DPtoLP(dc, (POINT*)&button, 2);
        }
    }
    return button;
}
