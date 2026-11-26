#pragma once
#include <errno.h>

// Theme files contain data only. Only the appearance settings listed here can
// be imported; paths, registry commands and executable actions are not accepted.
typedef struct EPThemeField
{
    PCWSTR group;
    PCWSTR key;
    PCWSTR path;
    PCWSTR value;
    DWORD fallback;
    DWORD maximum;
} EPThemeField;

#define EP_THEME_KEY L"Software\\GR-Taskbar"
#define EP_ADVANCED_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced"
#define EP_FIELD(g, n, d, max) { L##g, L##n, EP_THEME_KEY, L##n, d, max }
#define EP_ADV_FIELD(g, n, d, max) { L##g, L##n, EP_ADVANCED_KEY, L##n, d, max }
static const EPThemeField epThemeFields[] = {
    EP_FIELD("Taskbar", "OldTaskbar", 2, 2),
    EP_FIELD("Taskbar", "TaskbarCustomTheme", 0, 1),
    EP_FIELD("Taskbar", "TaskbarThemeTop", 0x00e57031, 0xffffff),
    EP_FIELD("Taskbar", "TaskbarThemeBottom", 0x00b0401c, 0xffffff),
    EP_FIELD("Taskbar", "OldTaskbarAl", 0, 7),
    EP_FIELD("Taskbar", "MMOldTaskbarAl", 0, 7),
    EP_FIELD("Taskbar", "ToolbarSeparators", 0, 1),
    EP_ADV_FIELD("Taskbar", "TaskbarSmallIcons", 0, 1),
    EP_ADV_FIELD("Taskbar", "TaskbarGlomLevel", 0, 2),
    EP_ADV_FIELD("Taskbar", "MMTaskbarGlomLevel", 0, 2),
    EP_ADV_FIELD("Taskbar", "ShowTaskViewButton", 1, 1),
    EP_FIELD("StartIcon", "OrbStyle", 0, 5),
    EP_FIELD("StartIcon", "StartIconSizeMode", 0, 4),
    EP_FIELD("StartIcon", "StartIconCustomSize", 24, 48),
    EP_FIELD("Explorer", "DisableWindows11ContextMenu", 0, 1),
    EP_FIELD("Explorer", "LegacyFileTransferDialog", 0, 1),
    EP_FIELD("Explorer", "UseClassicDriveGrouping", 0, 1),
    EP_FIELD("Explorer", "FileExplorerCommandUI", 0, 4),
    EP_FIELD("Explorer", "ShrinkExplorerAddressBar", 0, 1),
    EP_FIELD("Explorer", "HideExplorerSearchBar", 0, 1),
    EP_FIELD("Explorer", "HideIconAndTitleInExplorer", 0, 3),
    EP_FIELD("Explorer", "MicaEffectOnTitlebar", 0, 1),
    EP_ADV_FIELD("StartMenu", "Start_ShowClassicMode", 0, 1),
    EP_FIELD("StartMenu", "StartUI_ShowMoreTiles", 0, 1),
    { L"StartMenu", L"ForceStartSize", L"Software\\Policies\\Microsoft\\Windows\\Explorer", L"ForceStartSize", 0, 3 },
    { L"StartMenu", L"NoStartMenuMorePrograms", L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", L"NoStartMenuMorePrograms", 0, 3 },
    EP_FIELD("Tray", "SkinMenus", 1, 1),
    EP_FIELD("Tray", "FlyoutMenus", 1, 1),
    EP_FIELD("Tray", "CenterMenus", 0, 1),
    EP_FIELD("Tray", "SkinIcons", 1, 1),
    EP_FIELD("Tray", "ReplaceNetwork", 0, 6),
    EP_FIELD("Tray", "UseWin32TrayClockExperience", 0, 2),
    EP_FIELD("Tray", "UseWin32BatteryFlyout", 0, 1),
    { L"Switcher", L"AltTabSettings", L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer", L"AltTabSettings", 0, 3 },
    { L"Switcher", L"Theme", L"Software\\GR-Taskbar\\sws", L"Theme", 0, 3 },
    EP_FIELD("Other", "DisableAeroSnapQuadrants", 0, 1),
    EP_FIELD("Other", "DisableAeroShake", 0, 1)
};
#undef EP_FIELD
#undef EP_ADV_FIELD

static BOOL ThemeFolder(WCHAR* directory)
{
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, directory)) ||
        !PathAppendW(directory, L"GR-Taskbar\\Themes")) return FALSE;
    int result = SHCreateDirectoryExW(NULL, directory, NULL);
    return result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS || result == ERROR_FILE_EXISTS;
}

static BOOL ThemeWriteNumber(PCWSTR file, PCWSTR group, PCWSTR name, DWORD value)
{
    WCHAR number[32];
    swprintf_s(number, ARRAYSIZE(number), L"0x%08lx", value);
    return WritePrivateProfileStringW(group, name, number, file);
}

static BOOL ThemeReadNumber(PCWSTR file, PCWSTR group, PCWSTR name, DWORD* value)
{
    WCHAR number[64];
    if (!GetPrivateProfileStringW(group, name, L"", number, ARRAYSIZE(number), file)) return FALSE;
    if (number[0] == L'-' || number[0] == L'+') return FALSE;
    WCHAR* end;
    errno = 0;
    unsigned long parsed = wcstoul(number, &end, 0);
    if (errno || end == number || *end) return FALSE;
    *value = parsed;
    return TRUE;
}

static BOOL ThemeGroupEnabled(PCWSTR group)
{
    WCHAR name[64];
    swprintf_s(name, ARRAYSIZE(name), L"ThemeApply%s", group);
    DWORD enabled = wcscmp(group, L"StartIcon") != 0, bytes = sizeof(enabled);
    RegGetValueW(HKEY_CURRENT_USER, EP_THEME_KEY, name, RRF_RT_REG_DWORD, NULL, &enabled, &bytes);
    return enabled != 0;
}

static BOOL ThemeSaveSnapshot(PCWSTR file)
{
    HANDLE output = CreateFileW(file, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) return FALSE;
    const WORD marker = 0xfeff;
    DWORD written = 0;
    BOOL initialized = WriteFile(output, &marker, sizeof(marker), &written, NULL) && written == sizeof(marker);
    CloseHandle(output);
    if (!initialized) return FALSE;
    if (!WritePrivateProfileStringW(L"Theme", L"Format", L"1", file)) return FALSE;
    for (UINT i = 0; i < ARRAYSIZE(epThemeFields); ++i)
    {
        const EPThemeField* field = &epThemeFields[i];
        DWORD value = field->fallback, bytes = sizeof(value);
        LSTATUS read = RegGetValueW(HKEY_CURRENT_USER, field->path, field->value, RRF_RT_REG_DWORD, NULL, &value, &bytes);
        WCHAR present[MAX_PATH];
        swprintf_s(present, ARRAYSIZE(present), L"%s.Present", field->key);
        if (!ThemeWriteNumber(file, field->group, present, read == ERROR_SUCCESS) ||
            !ThemeWriteNumber(file, field->group, field->key, value)) return FALSE;
    }
    WCHAR orb[MAX_PATH] = L"";
    DWORD bytes = sizeof(orb);
    RegGetValueW(HKEY_CURRENT_USER, EP_THEME_KEY, CUSTOM_ORB_IMAGE_VALUE, RRF_RT_REG_SZ, NULL, orb, &bytes);
    return WritePrivateProfileStringW(L"StartIcon", L"Image", orb, file);
}

static void ThemeRefresh(void)
{
    SendNotifyMessageW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"TraySettings");
    HWND tray = FindWindowW(L"Shell_TrayWnd", NULL);
    if (tray) RedrawWindow(tray, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
}

static BOOL ThemeApplyFile(HWND owner, PCWSTR file, BOOL undo)
{
    DWORD format;
    if (!ThemeReadNumber(file, L"Theme", L"Format", &format) || format != 1)
    {
        MessageBoxW(owner, L"Choose an GR Taskbar theme file with format version 1.", L"Themes", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    DWORD values[ARRAYSIZE(epThemeFields)] = { 0 }, present[ARRAYSIZE(epThemeFields)] = { 0 };
    BOOL included[ARRAYSIZE(epThemeFields)] = { 0 };
    // Validate the complete selected portion before changing any setting.
    for (UINT i = 0; i < ARRAYSIZE(epThemeFields); ++i)
    {
        const EPThemeField* field = &epThemeFields[i];
        if (!undo && !ThemeGroupEnabled(field->group)) continue;
        WCHAR raw[64];
        if (!GetPrivateProfileStringW(field->group, field->key, L"", raw, ARRAYSIZE(raw), file)) continue;
        if (!ThemeReadNumber(file, field->group, field->key, &values[i]) || values[i] > field->maximum) goto invalid;
        if (!wcscmp(field->key, GR_START_ICON_CUSTOM_SIZE) && values[i] < GR_START_ICON_MIN_SIZE) goto invalid;
        included[i] = TRUE;
        present[i] = 1;
        WCHAR name[MAX_PATH];
        swprintf_s(name, ARRAYSIZE(name), L"%s.Present", field->key);
        ThemeReadNumber(file, field->group, name, &present[i]);
        if (present[i] > 1) goto invalid;
    }
    WCHAR orb[MAX_PATH] = L"";
    GetPrivateProfileStringW(L"StartIcon", L"Image", L"", orb, ARRAYSIZE(orb), file);
    BOOL customIcon = FALSE;
    for (UINT i = 0; i < ARRAYSIZE(epThemeFields); ++i)
        if (included[i] && !wcscmp(epThemeFields[i].key, L"OrbStyle") && values[i] == ORB_STYLE_CUSTOM) customIcon = TRUE;
    if (customIcon)
    {
        if (!orb[0]) goto invalid;
        OrbGallery library = { 0 };
        if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, library.directory)) ||
            !PathAppendW(library.directory, L"GR-Taskbar\\Orbs")) goto invalid;
        SHCreateDirectoryExW(NULL, library.directory, NULL);
        if (!OrbGalleryImportFile(&library, orb)) goto invalid;
        wcscpy_s(orb, ARRAYSIZE(orb), library.selectedPath);
    }
    WCHAR previous[MAX_PATH];
    if (!ThemeFolder(previous) || !PathAppendW(previous, L"previous.eptheme")) goto invalid;
    if (!undo && !ThemeSaveSnapshot(previous))
    {
        MessageBoxW(owner, L"Could not back up your current appearance settings. No settings were applied.", L"Themes", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    BOOL failed = FALSE;
    for (UINT i = 0; i < ARRAYSIZE(epThemeFields); ++i)
    {
        if (!included[i]) continue;
        const EPThemeField* field = &epThemeFields[i];
        LSTATUS result;
        if (!wcscmp(field->value, L"FileExplorerCommandUI"))
        {
            DWORD commandUI = present[i] ? values[i] : field->fallback;
            result = GUI_RegSetValueExW(NULL, L"Virtualized_" _T(EP_CLSID) L"_FileExplorerCommandUI", 0, REG_DWORD, (const BYTE*)&commandUI, sizeof(DWORD));
            if (!present[i] && result == ERROR_SUCCESS)
            {
                result = RegDeleteKeyValueW(HKEY_CURRENT_USER, field->path, field->value);
                if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
            }
        }
        else if (!present[i])
        {
            result = RegDeleteKeyValueW(HKEY_CURRENT_USER, field->path, field->value);
            if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
        }
        else
            result = RegSetKeyValueW(HKEY_CURRENT_USER, field->path, field->value, REG_DWORD, &values[i], sizeof(DWORD));
        if (result != ERROR_SUCCESS) failed = TRUE;
    }
    if ((undo || ThemeGroupEnabled(L"StartIcon")) && orb[0] &&
        RegSetKeyValueW(HKEY_CURRENT_USER, EP_THEME_KEY, CUSTOM_ORB_IMAGE_VALUE, REG_SZ,
            orb, (DWORD)((wcslen(orb) + 1) * sizeof(WCHAR))) != ERROR_SUCCESS) failed = TRUE;
    ThemeRefresh();
    MessageBoxW(owner, failed ? L"Some appearance settings could not be applied. Use Undo last theme to restore the previous settings." :
        L"Appearance settings applied. Restart File Explorer to load all changes. The Start icon changes only when its theme checkbox is enabled.",
        L"Themes", MB_OK | (failed ? MB_ICONWARNING : MB_ICONINFORMATION));
    InvalidateRect(owner, NULL, FALSE);
    return !failed;
invalid:
    MessageBoxW(owner, L"This theme has invalid or unsupported appearance values, or its Start icon image is missing. No appearance settings were applied.", L"Themes", MB_OK | MB_ICONERROR);
    return FALSE;
}

static void ThemePreset(HWND owner, int preset)
{
    if (preset < 0 || preset > 5) return;
    WCHAR file[MAX_PATH];
    if (!ThemeFolder(file) || !PathAppendW(file, L"builtin.eptheme")) return;
    // Clear only this internally generated preset's INI sections.
    const PCWSTR groups[] = { L"Theme", L"Taskbar", L"Explorer", L"StartMenu", L"Tray", L"Switcher", L"StartIcon", L"Other" };
    for (UINT i = 0; i < ARRAYSIZE(groups); ++i) WritePrivateProfileStringW(groups[i], NULL, NULL, file);
    WritePrivateProfileStringW(L"Theme", L"Format", L"1", file);
    const COLORREF tops[] = { RGB(77,142,245), RGB(54,91,151), RGB(224,223,227), RGB(69,113,147), RGB(32,32,32), RGB(242,242,242) };
    const COLORREF bottoms[] = { RGB(27,70,183), RGB(22,48,96), RGB(176,175,179), RGB(21,47,69), RGB(24,24,24), RGB(230,230,230) };
    ThemeWriteNumber(file, L"Taskbar", L"OldTaskbar", preset == 5 ? 0 : 2);
    ThemeWriteNumber(file, L"Taskbar", L"TaskbarCustomTheme", preset < 4);
    ThemeWriteNumber(file, L"Taskbar", L"TaskbarThemeTop", tops[preset]);
    ThemeWriteNumber(file, L"Taskbar", L"TaskbarThemeBottom", bottoms[preset]);
    ThemeWriteNumber(file, L"Taskbar", L"TaskbarSmallIcons", preset < 3);
    ThemeWriteNumber(file, L"Taskbar", L"TaskbarGlomLevel", 0);
    ThemeWriteNumber(file, L"Taskbar", L"MMTaskbarGlomLevel", 0);
    ThemeWriteNumber(file, L"Taskbar", L"OldTaskbarAl", 0);
    ThemeWriteNumber(file, L"Taskbar", L"MMOldTaskbarAl", 0);
    ThemeWriteNumber(file, L"Explorer", L"DisableWindows11ContextMenu", preset != 5);
    ThemeWriteNumber(file, L"Explorer", L"LegacyFileTransferDialog", preset < 4);
    ThemeWriteNumber(file, L"Explorer", L"UseClassicDriveGrouping", preset < 4);
    ThemeWriteNumber(file, L"Explorer", L"FileExplorerCommandUI", preset < 4 ? 2 : preset == 4 ? 1 : 0);
    ThemeWriteNumber(file, L"Explorer", L"MicaEffectOnTitlebar", preset == 5);
    ThemeWriteNumber(file, L"StartMenu", L"Start_ShowClassicMode", preset != 5);
    ThemeWriteNumber(file, L"Tray", L"SkinMenus", preset != 2);
    ThemeWriteNumber(file, L"Tray", L"CenterMenus", 0);
    ThemeWriteNumber(file, L"Switcher", L"AltTabSettings", preset < 4 ? 1 : 0);
    ThemeWriteNumber(file, L"StartIcon", L"OrbStyle", preset < 3 ? ORB_STYLE_WINDOWSXP : preset == 3 ? ORB_STYLE_WINDOWS7 : preset == 4 ? 0 : 1);
    ThemeApplyFile(owner, file, FALSE);
}

static void ThemeApplySelectedPreset(HWND owner)
{
    DWORD preset = 0, bytes = sizeof(preset);
    RegGetValueW(HKEY_CURRENT_USER, EP_THEME_KEY, L"ThemePresetChoice", RRF_RT_REG_DWORD, NULL, &preset, &bytes);
    ThemePreset(owner, (int)preset);
}

static void ThemeChooseFile(HWND owner, BOOL save)
{
    WCHAR file[MAX_PATH] = L"", directory[MAX_PATH];
    if (!ThemeFolder(directory)) return;
    OPENFILENAMEW dialog = { sizeof(dialog) };
    dialog.hwndOwner = owner;
    dialog.lpstrTitle = save ? L"Save all appearance settings as a custom theme" : L"Load a custom appearance theme";
    dialog.lpstrFilter = L"GR Taskbar themes (*.eptheme)\0*.eptheme\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = ARRAYSIZE(file);
    dialog.lpstrInitialDir = directory;
    dialog.lpstrDefExt = L"eptheme";
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
        (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (save ? !GetSaveFileNameW(&dialog) : !GetOpenFileNameW(&dialog)) return;
    if (!save) ThemeApplyFile(owner, file, FALSE);
    else MessageBoxW(owner, ThemeSaveSnapshot(file) ? L"Custom theme saved, including all supported appearance groups and the selected Start icon path." :
        L"Could not save this custom theme.", L"Themes", MB_OK);
}

static void ThemeUndo(HWND owner)
{
    WCHAR file[MAX_PATH];
    if (!ThemeFolder(file) || !PathAppendW(file, L"previous.eptheme") || !PathFileExistsW(file))
    {
        MessageBoxW(owner, L"No previous theme snapshot is available yet.", L"Themes", MB_OK | MB_ICONINFORMATION);
        return;
    }
    ThemeApplyFile(owner, file, TRUE);
}

static void ThemeChooseColor(HWND owner, BOOL bottom)
{
    static COLORREF customColors[16];
    DWORD color = bottom ? RGB(27, 70, 183) : RGB(77, 142, 245), bytes = sizeof(color);
    PCWSTR name = bottom ? L"TaskbarThemeBottom" : L"TaskbarThemeTop";
    RegGetValueW(HKEY_CURRENT_USER, EP_THEME_KEY, name, RRF_RT_REG_DWORD, NULL, &color, &bytes);
    CHOOSECOLORW dialog = { sizeof(dialog) };
    dialog.hwndOwner = owner;
    dialog.rgbResult = color;
    dialog.lpCustColors = customColors;
    dialog.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColorW(&dialog))
    {
        color = dialog.rgbResult;
        if (RegSetKeyValueW(HKEY_CURRENT_USER, EP_THEME_KEY, name, REG_DWORD, &color, sizeof(color)) != ERROR_SUCCESS)
            MessageBoxW(owner, L"Could not save the taskbar color.", L"Themes", MB_OK | MB_ICONERROR);
        ThemeRefresh();
    }
}
