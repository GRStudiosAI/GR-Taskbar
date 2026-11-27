#pragma once
#include <commctrl.h>
#include <knownfolders.h>
#pragma comment(lib, "Comctl32.lib")

#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "Comdlg32.lib")

typedef struct OrbGalleryItem
{
    DWORD style;
    WCHAR path[MAX_PATH];
    HBITMAP preview;
    BOOL previewAttempted;
} OrbGalleryItem;

typedef struct OrbGallery
{
    HWND list;
    HWND status;
    HWND useButton;
    HWND preview;
    UINT previewState;
    BOOL embedded;
    HFONT font;
    BOOL dark;
    HBRUSH background;
    BOOL sizingInitializing;
    HWND sizeMode;
    HWND customSize;
    HWND sizeSpin;
    WCHAR directory[MAX_PATH];
    WCHAR selectedPath[MAX_PATH];
    DWORD selectedStyle;
    BOOL done;
    CustomOrbImage activeImage;
    GRStartIconRenderCache activeCache;
    OrbGalleryItem* activeItem;
} OrbGallery;

static HBITMAP OrbGalleryPreview(CustomOrbImage* image, UINT dpi)
{
    // Gallery thumbnails use the same visible bounds and filtering as the shell.
    RECT button = { 0, 0, MulDiv(64, (int)dpi, 96), MulDiv(64, (int)dpi, 96) };
    GRStartIconSizing sizing = { 4, 48 };
    RECT target = GRStartIconLayout(button, image->contentBounds, dpi, sizing);
    return GRScaleOrbFrames(image, target.right - target.left, target.bottom - target.top);
}

static void OrbGalleryAdd(OrbGallery* gallery, DWORD style, PCWSTR path, PCWSTR name)
{

    OrbGalleryItem* item = calloc(1, sizeof(*item));
    if (item)
    {
        item->style = style;
        if (path) wcscpy_s(item->path, ARRAYSIZE(item->path), path);

        LRESULT index = SendMessageW(gallery->list, LB_ADDSTRING, 0, (LPARAM)name);
        if (index == LB_ERR || index == LB_ERRSPACE)
        {
            if (item->preview) DeleteObject(item->preview);
            free(item);
        }
        else
        {
            SendMessageW(gallery->list, LB_SETITEMDATA, index, (LPARAM)item);
            if (style == gallery->selectedStyle && (style != ORB_STYLE_CUSTOM ||
                !_wcsicmp(item->path, gallery->selectedPath)))
                SendMessageW(gallery->list, LB_SETCURSEL, index, 0);
        }
    }
}

static void OrbGalleryReadSelection(OrbGallery* gallery)
{
    gallery->selectedStyle = 0;
    DWORD bytes = sizeof(DWORD);
    RegGetValueW(HKEY_CURRENT_USER, _T(REGPATH), L"OrbStyle", RRF_RT_REG_DWORD,
        NULL, &gallery->selectedStyle, &bytes);
    bytes = sizeof(gallery->selectedPath);
    RegGetValueW(HKEY_CURRENT_USER, _T(REGPATH), CUSTOM_ORB_IMAGE_VALUE,
        RRF_RT_REG_SZ | RRF_ZEROONFAILURE, NULL, gallery->selectedPath, &bytes);
}

static void OrbGalleryScan(OrbGallery* gallery, PCWSTR directory, PCWSTR relative, BOOL* found, unsigned depth)
{
    if (depth > 32) return;
    WCHAR pattern[MAX_PATH];
    if (swprintf_s(pattern, MAX_PATH, L"%s\\*", directory) < 0) return;
    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileW(pattern, &data);
    if (find == INVALID_HANDLE_VALUE) return;
    do {
        if (!wcscmp(data.cFileName,L".") || !wcscmp(data.cFileName,L"..") ||
            (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        WCHAR path[MAX_PATH], label[MAX_PATH];
        if (swprintf_s(path, MAX_PATH, L"%s\\%s", directory, data.cFileName) < 0 ||
            swprintf_s(label, MAX_PATH, L"%s%s%s", relative, *relative ? L"\\" : L"", data.cFileName) < 0) continue;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (_wcsicmp(data.cFileName, L"Nudes")) OrbGalleryScan(gallery, path, label, found, depth + 1);
            continue;
        }
        PCWSTR ext = PathFindExtensionW(path);
        if (_wcsicmp(ext,L".png") && _wcsicmp(ext,L".bmp") && _wcsicmp(ext,L".ico") && _wcsicmp(ext,L".jpg") && _wcsicmp(ext,L".jpeg")) continue;
        OrbGalleryAdd(gallery, ORB_STYLE_CUSTOM, path, label);
        if (!_wcsicmp(path, gallery->selectedPath)) *found = TRUE;
    } while (FindNextFileW(find,&data));
    FindClose(find);
}

static BOOL OrbGalleryInstalledDirectory(WCHAR path[MAX_PATH])
{
    // Resolve beside the actual loaded settings DLL first, including nondefault installs.
    HMODULE settings = NULL;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (PCWSTR)(ULONG_PTR)&OrbGalleryInstalledDirectory, &settings) &&
        GetModuleFileNameW(settings, path, MAX_PATH) && PathRemoveFileSpecW(path) && PathAppendW(path, L"Orbs")) {
        DWORD attributes = GetFileAttributesW(path);
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) return TRUE;
    }
    PWSTR programs = NULL;
    HRESULT result = SHGetKnownFolderPath(&FOLDERID_ProgramFilesX64, KF_FLAG_DEFAULT, NULL, &programs);
    if (FAILED(result)) result = SHGetKnownFolderPath(&FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL, &programs);
    if (FAILED(result) || !programs) return FALSE;
    BOOL copied = wcslen(programs) < MAX_PATH && wcscpy_s(path, MAX_PATH, programs) == 0;
    CoTaskMemFree(programs);
    if (!copied || !PathAppendW(path, L"GR-Taskbar\\Orbs")) return FALSE;
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}
static void OrbGalleryClearActive(OrbGallery* gallery)
{
    GRClearStartIconCache(&gallery->activeCache);
    if (gallery->activeImage.bitmap) DeleteObject(gallery->activeImage.bitmap);
    ZeroMemory(&gallery->activeImage, sizeof(gallery->activeImage));
    gallery->activeItem = NULL;
}

static void OrbGalleryPopulate(OrbGallery* gallery)
{
    OrbGalleryClearActive(gallery);
    SendMessageW(gallery->list, WM_SETREDRAW, FALSE, 0);
    SendMessageW(gallery->list, LB_RESETCONTENT, 0, 0);
    OrbGalleryAdd(gallery, 0, NULL, L"Windows 10");
    OrbGalleryAdd(gallery, 1, NULL, L"Windows 11");
    OrbGalleryAdd(gallery, ORB_STYLE_WINDOWS7, NULL, L"Windows 7");
    OrbGalleryAdd(gallery, ORB_STYLE_WINDOWSXP, NULL, L"Windows XP");
    OrbGalleryAdd(gallery, ORB_STYLE_TRANSPARENT, NULL, L"Transparent (no icon)");
    BOOL found = FALSE;
    WCHAR installed[MAX_PATH];
    if (OrbGalleryInstalledDirectory(installed)) OrbGalleryScan(gallery, installed, L"", &found, 0);
    OrbGalleryScan(gallery, gallery->directory, L"Imported", &found, 0);
    if (gallery->selectedStyle == ORB_STYLE_CUSTOM && !found && gallery->selectedPath[0])
        OrbGalleryAdd(gallery, ORB_STYLE_CUSTOM, gallery->selectedPath, PathFindFileNameW(gallery->selectedPath));
    if (SendMessageW(gallery->list, LB_GETCURSEL, 0, 0) == LB_ERR) SendMessageW(gallery->list, LB_SETCURSEL, 0, 0);
    SendMessageW(gallery->list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(gallery->list, NULL, TRUE);
    WCHAR status[256];
    swprintf_s(status, ARRAYSIZE(status), L"%ld icons listed. Select an icon to preview it.",
        (long)SendMessageW(gallery->list, LB_GETCOUNT, 0, 0));
    SetWindowTextW(gallery->status, status);
}

static BOOL OrbGalleryImportFile(OrbGallery* gallery, PCWSTR source)
{
    if (wcslen(source) >= MAX_PATH) return FALSE;
    CustomOrbImage image = { 0 };
    if (!LoadCustomOrbImage(source, &image)) return FALSE;
    DeleteObject(image.bitmap);
    WCHAR dest[MAX_PATH], stem[MAX_PATH];
    wcscpy_s(stem, ARRAYSIZE(stem), PathFindFileNameW(source));
    PathRemoveExtensionW(stem);
    PCWSTR extension = PathFindExtensionW(source);
    for (int number = 0; number < 10000; ++number)
    {
        int written = number ? swprintf_s(dest, ARRAYSIZE(dest), L"%s\\%s (%d)%s", gallery->directory, stem, number, extension) :
            swprintf_s(dest, ARRAYSIZE(dest), L"%s\\%s%s", gallery->directory, stem, extension);
        if (written < 0) return FALSE;
        if (!_wcsicmp(source, dest) || CopyFileW(source, dest, TRUE))
        {
            wcscpy_s(gallery->selectedPath, ARRAYSIZE(gallery->selectedPath), dest);
            gallery->selectedStyle = ORB_STYLE_CUSTOM;
            return TRUE;
        }
        if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) return FALSE;
    }
    return FALSE;
}

static void OrbGalleryImport(HWND hwnd, OrbGallery* gallery)
{
    WCHAR files[32768] = L"";
    OPENFILENAMEW dialog = { sizeof(dialog) };
    dialog.hwndOwner = hwnd;
    dialog.lpstrTitle = L"Add Start icons to your library";
    dialog.lpstrFilter = L"Start icons (*.png;*.bmp;*.ico;*.jpg;*.jpeg)\0*.png;*.bmp;*.ico;*.jpg;*.jpeg\0";
    dialog.lpstrFile = files;
    dialog.nMaxFile = ARRAYSIZE(files);
    dialog.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT;
    if (!GetOpenFileNameW(&dialog)) return;
    int added = 0, failed = 0;
    WCHAR* next = files + wcslen(files) + 1;
    if (!*next)
    {
        if (OrbGalleryImportFile(gallery, files)) ++added; else ++failed;
    }
    else
    {
        while (*next)
        {
            WCHAR path[MAX_PATH];
            if (swprintf_s(path, ARRAYSIZE(path), L"%s\\%s", files, next) >= 0 && OrbGalleryImportFile(gallery, path)) ++added;
            else ++failed;
            next += wcslen(next) + 1;
        }
    }
    OrbGalleryPopulate(gallery);
    WCHAR status[256];
    swprintf_s(status, ARRAYSIZE(status), L"Added %d image(s). %d rejected. Select an icon, then choose Use selected.", added, failed);
    SetWindowTextW(gallery->status, status);
    if (failed) MessageBoxW(hwnd, L"Some images could not be imported. Use a square PNG or BMP, or a strip with three equal-height frames stacked vertically (up to 1024 pixels wide and 3072 pixels tall). Also check the file path and folder permissions.", L"Start icon import", MB_OK | MB_ICONWARNING);
}

static BOOL OrbGalleryApply(HWND hwnd, OrbGallery* gallery)
{
    LRESULT index = SendMessageW(gallery->list, LB_GETCURSEL, 0, 0);
    if (index == LB_ERR) return FALSE;
    OrbGalleryItem* item = (OrbGalleryItem*)SendMessageW(gallery->list, LB_GETITEMDATA, index, 0);
    if (!item || item == (void*)LB_ERR) return FALSE;
    if (item->style == ORB_STYLE_CUSTOM)
    {
        CustomOrbImage image = { 0 };
        if (!LoadCustomOrbImage(item->path, &image))
        {
            MessageBoxW(hwnd, L"This image is no longer available. Import it again.", L"Start icon", MB_OK | MB_ICONERROR);
            return FALSE;
        }
        DeleteObject(image.bitmap);
        if (RegSetKeyValueW(HKEY_CURRENT_USER, _T(REGPATH), CUSTOM_ORB_IMAGE_VALUE, REG_SZ,
            item->path, (DWORD)((wcslen(item->path) + 1) * sizeof(WCHAR))) != ERROR_SUCCESS) goto save_error;
    }
    if (RegSetKeyValueW(HKEY_CURRENT_USER, _T(REGPATH), L"OrbStyle", REG_DWORD,
        &item->style, sizeof(item->style)) != ERROR_SUCCESS) goto save_error;
    OrbGalleryReadSelection(gallery);
    SetWindowTextW(gallery->status, L"Selected icon saved. Your taskbar and Start menu styles have not changed.");
    return TRUE;
save_error:
    MessageBoxW(hwnd, L"Could not save the Start icon setting.", L"Start icon", MB_OK | MB_ICONERROR);
    return FALSE;
}

static LRESULT CALLBACK OrbPreviewSubclass(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR context)
{
    OrbGallery* gallery = (OrbGallery*)context;
    if (message == WM_MOUSEMOVE)
    {
        TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&track);
        UINT state = (wParam & MK_LBUTTON) ? 2u : 1u;
        if (gallery->previewState != state)
        { gallery->previewState = state; InvalidateRect(hwnd, NULL, FALSE); }
    }
    else if (message == WM_MOUSELEAVE || message == WM_CAPTURECHANGED)
    {
        gallery->previewState = 0;
        InvalidateRect(hwnd, NULL, FALSE);
    }
    else if (message == WM_LBUTTONDOWN)
    {
        SetCapture(hwnd);
        gallery->previewState = 2;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    else if (message == WM_LBUTTONUP)
    {
        ReleaseCapture();
        gallery->previewState = 1;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

static void GRMoveGalleryControl(HWND hwnd, int id, int x, int y, int width, int height, UINT dpi)
{
    HWND control = GetDlgItem(hwnd, id);
    if (control) MoveWindow(control, MulDiv(x, (int)dpi, 96), MulDiv(y, (int)dpi, 96),
        max(1, MulDiv(width, (int)dpi, 96)), max(1, MulDiv(height, (int)dpi, 96)), TRUE);
}

static void OrbGalleryReadSizing(OrbGallery* gallery)
{
    gallery->sizingInitializing = TRUE;
    GRStartIconSizing settings = GRReadStartIconSizing();
    SendMessageW(gallery->sizeMode, CB_SETCURSEL, settings.mode, 0);
    SetDlgItemInt(GetParent(gallery->customSize), 110, settings.customSize, FALSE);
    EnableWindow(gallery->customSize, settings.mode == 4);
    EnableWindow(gallery->sizeSpin, settings.mode == 4);
    gallery->sizingInitializing = FALSE;
}

static void OrbGallerySaveSizing(HWND hwnd, OrbGallery* gallery, BOOL clampInput)
{
    if (gallery->sizingInitializing) return;
    LRESULT selected = SendMessageW(gallery->sizeMode, CB_GETCURSEL, 0, 0);
    if (selected < 0 || selected > 4) return;
    BOOL valid = FALSE;
    UINT size = GetDlgItemInt(hwnd, 110, &valid, FALSE);
    if (!valid || size < 16 || size > 48)
    {
        if (!clampInput) return;
        size = valid ? max(16, min(48, size)) : 24;
        gallery->sizingInitializing = TRUE;
        SetDlgItemInt(hwnd, 110, size, FALSE);
        gallery->sizingInitializing = FALSE;
    }
    DWORD mode = (DWORD)selected, value = size;
    if (RegSetKeyValueW(HKEY_CURRENT_USER, _T(REGPATH), GR_START_ICON_CUSTOM_SIZE, REG_DWORD, &value, sizeof(value)) != ERROR_SUCCESS ||
        RegSetKeyValueW(HKEY_CURRENT_USER, _T(REGPATH), GR_START_ICON_SIZE_MODE, REG_DWORD, &mode, sizeof(mode)) != ERROR_SUCCESS)
    {
        SetWindowTextW(gallery->status, L"Could not save the icon size. Your previous setting remains available.");
        OrbGalleryReadSizing(gallery);
        return;
    }
    EnableWindow(gallery->customSize, mode == 4);
    EnableWindow(gallery->sizeSpin, mode == 4);
    InvalidateRect(gallery->preview, NULL, FALSE);
    SetWindowTextW(gallery->status, L"Icon size saved. The taskbar updates automatically; no Windows restart is needed.");
    SendNotifyMessageW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"TraySettings");
}

static void OrbGalleryLayout(HWND hwnd, OrbGallery* gallery)
{
    RECT rc; GetClientRect(hwnd, &rc);
    UINT dpi = GetDpiForWindow(hwnd); if (!dpi) dpi = 96;
    int width = MulDiv(rc.right, 96, (int)dpi), height = MulDiv(rc.bottom, 96, (int)dpi);
    int listWidth = max(200, width - 270);
    GRMoveGalleryControl(hwnd, 103, 16, 8, width - 32, 24, dpi);
    GRMoveGalleryControl(hwnd, 108, 16, 43, 38, 24, dpi);
    GRMoveGalleryControl(hwnd, 109, 58, 38, 180, 200, dpi);
    GRMoveGalleryControl(hwnd, 110, 248, 40, 56, 26, dpi);
    GRMoveGalleryControl(hwnd, 111, 290, 40, 14, 26, dpi);
    GRMoveGalleryControl(hwnd, 113, 311, 43, 28, 24, dpi);
    GRMoveGalleryControl(hwnd, 112, 346, 38, 132, 30, dpi);
    GRMoveGalleryControl(hwnd, 100, 16, 80, listWidth - 24, max(100, height - 188), dpi);
    GRMoveGalleryControl(hwnd, 102, listWidth + 8, 88, 230, max(100, height - 238), dpi);
    GRMoveGalleryControl(hwnd, 107, listWidth + 12, max(190, height - 140), 230, 36, dpi);
    GRMoveGalleryControl(hwnd, 115, 16, height - 100, width - 32, 44, dpi);
    GRMoveGalleryControl(hwnd, 101, 16, height - 44, 140, 32, dpi);
    GRMoveGalleryControl(hwnd, IDOK, width - 164, height - 44, 148, 32, dpi);
    if (!gallery->embedded) GRMoveGalleryControl(hwnd, IDCANCEL, 170, height - 44, 100, 32, dpi);
}

static LRESULT CALLBACK OrbGalleryWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    OrbGallery* gallery = (OrbGallery*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (message == WM_NCCREATE)
    {
        gallery = ((CREATESTRUCTW*)lParam)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)gallery);
    }
    switch (message)
    {
    case WM_CREATE:
    {
        gallery->sizingInitializing = TRUE;
        INITCOMMONCONTROLSEX controls = { sizeof(controls), ICC_STANDARD_CLASSES | ICC_UPDOWN_CLASS };
        InitCommonControlsEx(&controls);
        UINT dpi = GetDpiForWindow(hwnd); if (!dpi) dpi = 96;
        DWORD light = 1, bytes = sizeof(light);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &light, &bytes);
        HIGHCONTRASTW contrast = { sizeof(contrast) };
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
        gallery->dark = !light && !(contrast.dwFlags & HCF_HIGHCONTRASTON);
        gallery->background = CreateSolidBrush(gallery->dark ? RGB(25, 32, 45) : RGB(255, 255, 255));
        HFONT font = CreateFontW(-MulDiv(13, (int)dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        gallery->font = font;
        HWND label = CreateWindowW(L"STATIC", L"Your icon library", WS_CHILD | WS_VISIBLE, 16, 16, 650, 24, hwnd, (HMENU)103, NULL, NULL);
        SendMessageW(label, WM_SETFONT, (WPARAM)font, TRUE);
        gallery->list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
            16, 48, 650, 338, hwnd, (HMENU)100, NULL, NULL);
        SendMessageW(gallery->list, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(gallery->list, LB_SETITEMHEIGHT, 0, MulDiv(72, (int)dpi, 96));
        HWND sizeLabel = CreateWindowW(L"STATIC", L"Size:", WS_CHILD | WS_VISIBLE, 16, 40, 38, 24, hwnd, (HMENU)108, NULL, NULL);
        gallery->sizeMode = CreateWindowW(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            58, 38, 180, 200, hwnd, (HMENU)109, NULL, NULL);
        const PCWSTR modes[] = { L"Automatic", L"Small (16 px)", L"Medium (24 px)", L"Large (32 px)", L"Custom (16-48 px)" };
        for (UINT i = 0; i < ARRAYSIZE(modes); ++i) SendMessageW(gallery->sizeMode, CB_ADDSTRING, 0, (LPARAM)modes[i]);
        gallery->customSize = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"24", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL,
            248, 40, 56, 26, hwnd, (HMENU)110, NULL, NULL);
        SendMessageW(gallery->customSize, EM_SETLIMITTEXT, 2, 0);
        gallery->sizeSpin = CreateWindowW(UPDOWN_CLASSW, NULL, WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_ARROWKEYS,
            290, 40, 14, 26, hwnd, (HMENU)111, NULL, NULL);
        SendMessageW(gallery->sizeSpin, UDM_SETBUDDY, (WPARAM)gallery->customSize, 0);
        SendMessageW(gallery->sizeSpin, UDM_SETRANGE32, 16, 48);
        HWND units = CreateWindowW(L"STATIC", L"px", WS_CHILD | WS_VISIBLE, 311, 43, 28, 24, hwnd, (HMENU)113, NULL, NULL);
        HWND reset = CreateWindowW(L"BUTTON", L"Reset to default", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 346, 38, 132, 30, hwnd, (HMENU)112, NULL, NULL);
        SendMessageW(sizeLabel, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(gallery->sizeMode, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(gallery->customSize, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(units, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(reset, WM_SETFONT, (WPARAM)font, TRUE);
        gallery->preview = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY, 420, 56, 230, 230, hwnd, (HMENU)102, NULL, NULL);
        SetWindowSubclass(gallery->preview, OrbPreviewSubclass, 1, (DWORD_PTR)gallery);
        HWND hint = CreateWindowW(L"STATIC", L"Actual-size preview on a standard taskbar. Hover or press the icon.", WS_CHILD | WS_VISIBLE | SS_CENTER, 420, 300, 230, 48, hwnd, (HMENU)107, NULL, NULL);
        SendMessageW(hint, WM_SETFONT, (WPARAM)font, TRUE);
        gallery->status = CreateWindowW(L"STATIC", L"Sizing uses logical pixels and follows each monitor's DPI. Larger icons are constrained to the Start button.", WS_CHILD | WS_VISIBLE, 16, 396, 650, 38, hwnd, (HMENU)115, NULL, NULL);
        SendMessageW(gallery->status, WM_SETFONT, (WPARAM)font, TRUE);
        HWND add = CreateWindowW(L"BUTTON", L"Add images...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 16, 444, 130, 32, hwnd, (HMENU)101, NULL, NULL);
        gallery->useButton = CreateWindowW(L"BUTTON", L"Use selected", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 396, 444, 130, 32, hwnd, (HMENU)IDOK, NULL, NULL);
        HWND close = gallery->embedded ? NULL : CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 536, 444, 130, 32, hwnd, (HMENU)IDCANCEL, NULL, NULL);
        SendMessageW(add, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(gallery->useButton, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(close, WM_SETFONT, (WPARAM)font, TRUE);
        OrbGalleryReadSelection(gallery);
        OrbGalleryPopulate(gallery);
        OrbGalleryReadSizing(gallery);
        OrbGalleryLayout(hwnd, gallery);
        if (!gallery->embedded) SetFocus(gallery->list);
        return 0;
    }
    case WM_SIZE:
        if (gallery && gallery->list) OrbGalleryLayout(hwnd, gallery);
        return 0;
    case WM_DPICHANGED_AFTERPARENT:
        if (gallery && gallery->list)
        {
            LRESULT index = SendMessageW(gallery->list, LB_GETCURSEL, 0, 0);
            if (index != LB_ERR)
            {
                OrbGalleryItem* selected = (OrbGalleryItem*)SendMessageW(gallery->list, LB_GETITEMDATA, index, 0);
                if (selected && selected != (void*)LB_ERR)
                { gallery->selectedStyle = selected->style; wcscpy_s(gallery->selectedPath, MAX_PATH, selected->path); }
            }
            UINT dpi = GetDpiForWindow(hwnd); if (!dpi) dpi = 96;
            HFONT font = CreateFontW(-MulDiv(13, (int)dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            if (font)
            {
                for (HWND child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
                    SendMessageW(child, WM_SETFONT, (WPARAM)font, TRUE);
                if (gallery->font) DeleteObject(gallery->font);
                gallery->font = font;
            }
            SendMessageW(gallery->list, LB_SETITEMHEIGHT, 0, MulDiv(72, (int)dpi, 96));
            OrbGalleryPopulate(gallery);
            OrbGalleryLayout(hwnd, gallery);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    case WM_ERASEBKGND:
        if (gallery && gallery->background)
        {
            RECT rc; GetClientRect(hwnd, &rc);
            FillRect((HDC)wParam, &rc, gallery->background);
            return TRUE;
        }
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORLISTBOX:
        if (gallery && gallery->background)
        {
            SetBkColor((HDC)wParam, gallery->dark ? RGB(25, 32, 45) : RGB(255, 255, 255));
            SetTextColor((HDC)wParam, gallery->dark ? RGB(224, 234, 248) : RGB(29, 44, 66));
            return (LRESULT)gallery->background;
        }
        break;
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT*)lParam)->itemHeight = (UINT)MulDiv(72, (int)(GetDpiForWindow(hwnd) ? GetDpiForWindow(hwnd) : 96), 96);
        return TRUE;
    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT* draw = (DRAWITEMSTRUCT*)lParam;
        if (draw->CtlType == ODT_BUTTON)
        {
            COLORREF color = draw->CtlID == IDOK ? RGB(30, 114, 168) : (gallery->dark ? RGB(46, 59, 77) : RGB(224, 234, 247));
            if (draw->itemState & ODS_SELECTED) color = RGB(23, 88, 135);
            FillRect(draw->hDC, &draw->rcItem, gallery->background);
            HBRUSH brush = CreateSolidBrush(color);
            HGDIOBJ oldBrush = SelectObject(draw->hDC, brush), oldPen = SelectObject(draw->hDC, GetStockObject(NULL_PEN));
            RoundRect(draw->hDC, draw->rcItem.left, draw->rcItem.top, draw->rcItem.right, draw->rcItem.bottom, 10, 10);
            SelectObject(draw->hDC, oldBrush); SelectObject(draw->hDC, oldPen); DeleteObject(brush);
            SetTextColor(draw->hDC, draw->CtlID == IDOK || gallery->dark ? RGB(255, 255, 255) : RGB(29, 44, 66));
            SetBkMode(draw->hDC, TRANSPARENT);
            WCHAR label[80]; GetWindowTextW(draw->hwndItem, label, ARRAYSIZE(label));
            DrawTextW(draw->hDC, label, -1, &draw->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (draw->itemState & ODS_FOCUS) { RECT focus = draw->rcItem; InflateRect(&focus, -4, -4); DrawFocusRect(draw->hDC, &focus); }
            return TRUE;
        }
        if (draw->CtlID == 102)
        {
            FillRect(draw->hDC, &draw->rcItem, gallery->background);
            LRESULT index = SendMessageW(gallery->list, LB_GETCURSEL, 0, 0);
            if (index == LB_ERR) return TRUE;
            OrbGalleryItem* selected = (OrbGalleryItem*)SendMessageW(gallery->list, LB_GETITEMDATA, index, 0);
            if (!selected || selected == (void*)LB_ERR) return TRUE;
            if (gallery->activeItem != selected)
            {
                OrbGalleryClearActive(gallery);
                gallery->activeItem = selected;
                if (!(selected->style == ORB_STYLE_CUSTOM ?
                    LoadCustomOrbImage(selected->path, &gallery->activeImage) :
                    CreateBuiltinOrbImage(selected->style, &gallery->activeImage))) return TRUE;
            }
            if (!gallery->activeImage.bitmap) return TRUE;
            UINT dpi = GetDpiForWindow(hwnd); if (!dpi) dpi = 96;
            int buttonWidth = MulDiv(48, (int)dpi, 96), buttonHeight = MulDiv(40, (int)dpi, 96);
            RECT button = { (draw->rcItem.right - buttonWidth) / 2, (draw->rcItem.bottom - buttonHeight) / 2, 0, 0 };
            button.right = button.left + buttonWidth; button.bottom = button.top + buttonHeight;
            RECT target = GRStartIconLayout(button, gallery->activeImage.contentBounds, dpi, GRReadStartIconSizing());
            GRDrawOrbFrame(draw->hDC, &gallery->activeImage, &gallery->activeCache, target, (int)gallery->previewState);
            return TRUE;
        }
        if (draw->itemID == (UINT)-1) return TRUE;
        OrbGalleryItem* item = (OrbGalleryItem*)draw->itemData;
        BOOL selected = (draw->itemState & ODS_SELECTED) != 0;
        HBRUSH selection = CreateSolidBrush(gallery->dark ? RGB(37, 61, 83) : RGB(211, 231, 247));
        FillRect(draw->hDC, &draw->rcItem, selected ? selection : gallery->background);
        DeleteObject(selection);
        if (item && !item->previewAttempted)
        {
            item->previewAttempted = TRUE;
            CustomOrbImage image = { 0 };
            if (item->style == ORB_STYLE_CUSTOM ? LoadCustomOrbImage(item->path, &image) : CreateBuiltinOrbImage(item->style, &image)) {
                UINT dpi = GetDpiForWindow(hwnd);
                item->preview = OrbGalleryPreview(&image, dpi ? dpi : 96);
                DeleteObject(image.bitmap);
            }
        }
        if (item && item->preview)
        {
            HDC source = CreateCompatibleDC(draw->hDC);
            if (source)
            {
                HGDIOBJ old = SelectObject(source, item->preview);
                BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                BITMAP preview = { 0 }; GetObjectW(item->preview, sizeof(preview), &preview);
                int height = preview.bmHeight / 3;
                int padding = MulDiv(8, (int)(GetDpiForWindow(hwnd) ? GetDpiForWindow(hwnd) : 96), 96);
                int slot = MulDiv(64, (int)(GetDpiForWindow(hwnd) ? GetDpiForWindow(hwnd) : 96), 96);
                GdiAlphaBlend(draw->hDC, draw->rcItem.left + padding + (slot - preview.bmWidth) / 2,
                    draw->rcItem.top + (draw->rcItem.bottom - draw->rcItem.top - height) / 2,
                    preview.bmWidth, height, source, 0, 0, preview.bmWidth, height, blend);
                SelectObject(source, old);
                DeleteDC(source);
            }
        }
        WCHAR name[MAX_PATH];
        SendMessageW(draw->hwndItem, LB_GETTEXT, draw->itemID, (LPARAM)name);
        RECT text = draw->rcItem;
        text.left += MulDiv(80, (int)(GetDpiForWindow(hwnd) ? GetDpiForWindow(hwnd) : 96), 96);
        text.right -= 12;
        SetBkMode(draw->hDC, TRANSPARENT);
        SetTextColor(draw->hDC, gallery->dark ? RGB(224, 234, 248) : RGB(29, 44, 66));
        DrawTextW(draw->hDC, name, -1, &text, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (draw->itemState & ODS_FOCUS) DrawFocusRect(draw->hDC, &draw->rcItem);
        return TRUE;
    }
    case WM_DELETEITEM:
    {
        OrbGalleryItem* item = (OrbGalleryItem*)((DELETEITEMSTRUCT*)lParam)->itemData;
        if (item) { if (item->preview) DeleteObject(item->preview); free(item); }
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == 109 && HIWORD(wParam) == CBN_SELCHANGE) OrbGallerySaveSizing(hwnd, gallery, TRUE);
        else if (LOWORD(wParam) == 110 && (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == EN_KILLFOCUS))
            OrbGallerySaveSizing(hwnd, gallery, HIWORD(wParam) == EN_KILLFOCUS);
        else if (LOWORD(wParam) == 112)
        {
            gallery->sizingInitializing = TRUE;
            SendMessageW(gallery->sizeMode, CB_SETCURSEL, 0, 0);
            SetDlgItemInt(hwnd, 110, 24, FALSE);
            gallery->sizingInitializing = FALSE;
            OrbGallerySaveSizing(hwnd, gallery, TRUE);
        }
        else if (LOWORD(wParam) == 101) OrbGalleryImport(hwnd, gallery);
        else if (LOWORD(wParam) == 100 && HIWORD(wParam) == LBN_SELCHANGE)
        { gallery->previewState = 0; InvalidateRect(gallery->preview, NULL, TRUE); }
        else if (LOWORD(wParam) == IDOK || (LOWORD(wParam) == 100 && HIWORD(wParam) == LBN_DBLCLK)) OrbGalleryApply(hwnd, gallery);
        else if (LOWORD(wParam) == IDCANCEL) DestroyWindow(hwnd);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        OrbGalleryClearActive(gallery);
        gallery->done = TRUE;
        return 0;
    case WM_NCDESTROY:
        if (gallery)
        {
            if (gallery->font) DeleteObject(gallery->font);
            if (gallery->background) DeleteObject(gallery->background);
            if (gallery->embedded) free(gallery);
        }
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static HWND CreateEmbeddedIconGallery(HWND owner)
{
    OrbGallery* gallery = calloc(1, sizeof(*gallery));
    if (!gallery) return NULL;
    gallery->embedded = TRUE;
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, gallery->directory)) ||
        !PathAppendW(gallery->directory, L"GR-Taskbar\\Orbs")) { free(gallery); return NULL; }
    int result = SHCreateDirectoryExW(owner, gallery->directory, NULL);
    if (result != ERROR_SUCCESS && result != ERROR_ALREADY_EXISTS && result != ERROR_FILE_EXISTS) { free(gallery); return NULL; }
    WNDCLASSW windowClass = { 0 };
    windowClass.lpfnWndProc = OrbGalleryWindowProc;
    windowClass.hInstance = GetModuleHandleW(NULL);
    windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = L"GRTaskbar_StartIconGallery";
    if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { free(gallery); return NULL; }
    HWND panel = CreateWindowExW(WS_EX_CONTROLPARENT, windowClass.lpszClassName, L"Start icon library",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 700, 500, owner, NULL, windowClass.hInstance, gallery);
    // WM_NCDESTROY owns cleanup if creation reaches the window procedure.
    return panel;
}

static void ChooseCustomOrb(HWND owner)
{
    OrbGallery gallery = { 0 };
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, gallery.directory)) ||
        !PathAppendW(gallery.directory, L"GR-Taskbar\\Orbs")) return;
    int result = SHCreateDirectoryExW(owner, gallery.directory, NULL);
    if (result != ERROR_SUCCESS && result != ERROR_ALREADY_EXISTS && result != ERROR_FILE_EXISTS)
    {
        MessageBoxW(owner, L"Could not create your Start icon library.", L"Start icon", MB_OK | MB_ICONERROR);
        return;
    }
    HINSTANCE module = GetModuleHandleW(NULL);
    WNDCLASSW windowClass = { 0 };
    windowClass.lpfnWndProc = OrbGalleryWindowProc;
    windowClass.hInstance = module;
    windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = L"GRTaskbar_StartIconGallery";
    if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
    RECT bounds = { 0, 0, 684, 492 };
    AdjustWindowRectEx(&bounds, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    RECT parent;
    GetWindowRect(owner, &parent);
    int x = max(0, parent.left + (parent.right - parent.left - (bounds.right - bounds.left)) / 2);
    int y = max(0, parent.top + (parent.bottom - parent.top - (bounds.bottom - bounds.top)) / 2);
    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, windowClass.lpszClassName, L"Start icon gallery",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x, y, bounds.right - bounds.left, bounds.bottom - bounds.top,
        owner, NULL, module, &gallery);
    if (!hwnd) return;
    EnableWindow(owner, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    MSG message;
    while (!gallery.done)
    {
        int result = GetMessageW(&message, NULL, 0, 0);
        if (result <= 0)
        {
            if (!result) PostQuitMessage((int)message.wParam);
            break;
        }
        if (!IsDialogMessageW(hwnd, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    if (IsWindow(hwnd)) DestroyWindow(hwnd);
    UnregisterClassW(windowClass.lpszClassName, module);
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);
    InvalidateRect(owner, NULL, FALSE);
}
