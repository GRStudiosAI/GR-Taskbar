#pragma once

typedef struct EPAppearanceTheme
{
    DWORD enabled;
    DWORD topColor;
    DWORD bottomColor;
} EPAppearanceTheme;

static EPAppearanceTheme ReadAppearanceTheme(void)
{
    EPAppearanceTheme theme = { 0, RGB(49, 112, 229), RGB(28, 64, 176) };
    DWORD bytes = sizeof(DWORD);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\GR-Taskbar", L"TaskbarCustomTheme",
        RRF_RT_REG_DWORD, NULL, &theme.enabled, &bytes);
    bytes = sizeof(DWORD);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\GR-Taskbar", L"TaskbarThemeTop",
        RRF_RT_REG_DWORD, NULL, &theme.topColor, &bytes);
    bytes = sizeof(DWORD);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\GR-Taskbar", L"TaskbarThemeBottom",
        RRF_RT_REG_DWORD, NULL, &theme.bottomColor, &bytes);
    return theme;
}

static BOOL AppearanceThemeHighContrast(void)
{
    HIGHCONTRASTW contrast = { sizeof(contrast) };
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
        (contrast.dwFlags & HCF_HIGHCONTRASTON);
}

static BOOL DrawAppearanceTaskbar(HDC dc, LPCRECT rect, LPCRECT clip)
{
    EPAppearanceTheme theme = ReadAppearanceTheme();
    if (!theme.enabled || AppearanceThemeHighContrast() || !rect) return FALSE;
    int saved = SaveDC(dc);
    if (!saved) return FALSE;
    if (clip) IntersectClipRect(dc, clip->left, clip->top, clip->right, clip->bottom);
    TRIVERTEX vertices[2] = {
        { rect->left, rect->top, (COLOR16)(GetRValue(theme.topColor) << 8), (COLOR16)(GetGValue(theme.topColor) << 8), (COLOR16)(GetBValue(theme.topColor) << 8), 0xffff },
        { rect->right, rect->bottom, (COLOR16)(GetRValue(theme.bottomColor) << 8), (COLOR16)(GetGValue(theme.bottomColor) << 8), (COLOR16)(GetBValue(theme.bottomColor) << 8), 0xffff }
    };
    GRADIENT_RECT gradient = { 0, 1 };
    BOOL result = GdiGradientFill(dc, vertices, 2, &gradient, 1,
        rect->right - rect->left >= rect->bottom - rect->top ? GRADIENT_FILL_RECT_V : GRADIENT_FILL_RECT_H);
    RestoreDC(dc, saved);
    return result;
}
