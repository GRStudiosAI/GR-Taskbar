#pragma once
#include <commctrl.h>
#pragma comment(lib, "Comctl32.lib")

#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "Comdlg32.lib")

typedef struct OrbGalleryItem
{
    DWORD style;
    WCHAR path[MAX_PATH];
    HBITMAP preview;
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
    WCHAR directory[MAX_PATH];
    WCHAR selectedPath[MAX_PATH];
    DWORD selectedStyle;
    BOOL done;
} OrbGallery;

static HBITMAP OrbGalleryPreview(CustomOrbImage* image)
{
    BITMAPINFO info = { 0 };
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 64;
    info.bmiHeader.biHeight = -64;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* pixels = NULL;
    HBITMAP preview = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &pixels, NULL, 0);
    if (!preview) return NULL;
    ZeroMemory(pixels, 64 * 64 * 4);
    HDC target = CreateCompatibleDC(NULL), source = CreateCompatibleDC(NULL);
    if (!target || !source)
    {
        if (target) DeleteDC(target);
        if (source) DeleteDC(source);
        DeleteObject(preview);
        return NULL;
    }
    HGDIOBJ oldTarget = SelectObject(target, preview), oldSource = SelectObject(source, image->bitmap);
    int width = 56, height = MulDiv(56, image->frameHeight, image->width);
    if (height > 56) { height = 56; width = max(1, MulDiv(56, image->width, image->frameHeight)); }
    height = max(1, height);
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    GdiAlphaBlend(target, (64 - width) / 2, (64 - height) / 2, width, height,
        source, 0, 0, image->width, image->frameHeight, blend);
    SelectObject(target, oldTarget);
    SelectObject(source, oldSource);
    DeleteDC(target);
    DeleteDC(source);
    return preview;
}

static void OrbGalleryAdd(OrbGallery* gallery, DWORD style, PCWSTR path, PCWSTR name)
{
    CustomOrbImage image = { 0 };
    if (!(style == ORB_STYLE_CUSTOM ? LoadCustomOrbImage(path, &image) : CreateBuiltinOrbImage(style, &image))) return;
    OrbGalleryItem* item = calloc(1, sizeof(*item));
    if (item)
    {
        item->style = style;
        if (path) wcscpy_s(item->path, ARRAYSIZE(item->path), path);
        item->preview = OrbGalleryPreview(&image);
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
    DeleteObject(image.bitmap);
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

static void OrbGalleryPopulate(OrbGallery* gallery)
{
    SendMessageW(gallery->list, LB_RESETCONTENT, 0, 0);
    OrbGalleryAdd(gallery, 0, NULL, L"Windows 10");
    OrbGalleryAdd(gallery, 1, NULL, L"Windows 11");
    OrbGalleryAdd(gallery, ORB_STYLE_WINDOWS7, NULL, L"Windows 7");
    OrbGalleryAdd(gallery, ORB_STYLE_WINDOWSXP, NULL, L"Windows XP");
    WCHAR pattern[MAX_PATH];
    wcscpy_s(pattern, ARRAYSIZE(pattern), gallery->directory);
    PathAppendW(pattern, L"*");
    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileW(pattern, &data);
    BOOL foundSelected = FALSE;
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            PCWSTR extension = PathFindExtensionW(data.cFileName);
            if (_wcsicmp(extension, L".png") && _wcsicmp(extension, L".bmp") && _wcsicmp(extension, L".ico") &&
                _wcsicmp(extension, L".jpg") && _wcsicmp(extension, L".jpeg")) continue;
            WCHAR path[MAX_PATH];
            wcscpy_s(path, ARRAYSIZE(path), gallery->directory);
            if (!PathAppendW(path, data.cFileName)) continue;
            OrbGalleryAdd(gallery, ORB_STYLE_CUSTOM, path, data.cFileName);
            if (!_wcsicmp(path, gallery->selectedPath)) foundSelected = TRUE;
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }
    // Preserve selections made by the earlier single-image picker.
    if (gallery->selectedStyle == ORB_STYLE_CUSTOM && !foundSelected && gallery->selectedPath[0])
        OrbGalleryAdd(gallery, ORB_STYLE_CUSTOM, gallery->selectedPath, PathFindFileNameW(gallery->selectedPath));
    if (SendMessageW(gallery->list, LB_GETCURSEL, 0, 0) == LB_ERR)
        SendMessageW(gallery->list, LB_SETCURSEL, 0, 0);
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
        gallery->previewState = (wParam & MK_LBUTTON) ? 2 : 1;
        InvalidateRect(hwnd, NULL, FALSE);
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

static void OrbGalleryLayout(HWND hwnd, OrbGallery* gallery)
{
    RECT rc; GetClientRect(hwnd, &rc);
    int width = rc.right, height = rc.bottom;
    int listWidth = max(200, width - 270);
    MoveWindow(GetDlgItem(hwnd, 103), 16, 12, max(1, width - 32), 28, TRUE);
    MoveWindow(gallery->list, 16, 48, listWidth - 24, max(100, height - 156), TRUE);
    MoveWindow(gallery->preview, listWidth + 8, 56, 230, max(150, height - 206), TRUE);
    MoveWindow(GetDlgItem(hwnd, 107), listWidth + 12, max(220, height - 140), 230, 36, TRUE);
    MoveWindow(gallery->status, 16, height - 100, max(1, width - 32), 44, TRUE);
    MoveWindow(GetDlgItem(hwnd, 101), 16, height - 44, 140, 32, TRUE);
    MoveWindow(gallery->useButton, width - 164, height - 44, 148, 32, TRUE);
    if (!gallery->embedded) MoveWindow(GetDlgItem(hwnd, IDCANCEL), 170, height - 44, 100, 32, TRUE);
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
        DWORD light = 1, bytes = sizeof(light);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &light, &bytes);
        HIGHCONTRASTW contrast = { sizeof(contrast) };
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
        gallery->dark = !light && !(contrast.dwFlags & HCF_HIGHCONTRASTON);
        gallery->background = CreateSolidBrush(gallery->dark ? RGB(25, 32, 45) : RGB(255, 255, 255));
        HFONT font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        gallery->font = font;
        HWND label = CreateWindowW(L"STATIC", L"Your icon library", WS_CHILD | WS_VISIBLE, 16, 16, 650, 24, hwnd, (HMENU)103, NULL, NULL);
        SendMessageW(label, WM_SETFONT, (WPARAM)font, TRUE);
        gallery->list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
            16, 48, 650, 338, hwnd, (HMENU)100, NULL, NULL);
        SendMessageW(gallery->list, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(gallery->list, LB_SETITEMHEIGHT, 0, 80);
        gallery->preview = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY, 420, 56, 230, 230, hwnd, (HMENU)102, NULL, NULL);
        SetWindowSubclass(gallery->preview, OrbPreviewSubclass, 1, (DWORD_PTR)gallery);
        HWND hint = CreateWindowW(L"STATIC", L"Hover or press the orb to preview it.", WS_CHILD | WS_VISIBLE | SS_CENTER, 420, 300, 230, 48, hwnd, (HMENU)107, NULL, NULL);
        SendMessageW(hint, WM_SETFONT, (WPARAM)font, TRUE);
        gallery->status = CreateWindowW(L"STATIC", L"Add images to keep them in your library. Select an icon, then choose Use selected.", WS_CHILD | WS_VISIBLE, 16, 396, 650, 38, hwnd, NULL, NULL, NULL);
        SendMessageW(gallery->status, WM_SETFONT, (WPARAM)font, TRUE);
        HWND add = CreateWindowW(L"BUTTON", L"Add images...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 16, 444, 130, 32, hwnd, (HMENU)101, NULL, NULL);
        gallery->useButton = CreateWindowW(L"BUTTON", L"Use selected", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 396, 444, 130, 32, hwnd, (HMENU)IDOK, NULL, NULL);
        HWND close = gallery->embedded ? NULL : CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 536, 444, 130, 32, hwnd, (HMENU)IDCANCEL, NULL, NULL);
        SendMessageW(add, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(gallery->useButton, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(close, WM_SETFONT, (WPARAM)font, TRUE);
        OrbGalleryReadSelection(gallery);
        OrbGalleryPopulate(gallery);
        OrbGalleryLayout(hwnd, gallery);
        if (!gallery->embedded) SetFocus(gallery->list);
        return 0;
    }
    case WM_SIZE:
        if (gallery && gallery->list) OrbGalleryLayout(hwnd, gallery);
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
        ((MEASUREITEMSTRUCT*)lParam)->itemHeight = 80;
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
            CustomOrbImage image = { 0 };
            if (!(selected->style == ORB_STYLE_CUSTOM ? LoadCustomOrbImage(selected->path, &image) : CreateBuiltinOrbImage(selected->style, &image))) return TRUE;
            HDC source = CreateCompatibleDC(draw->hDC);
            if (source)
            {
                HGDIOBJ old = SelectObject(source, image.bitmap);
                int width = min(180, draw->rcItem.right - 32), height = MulDiv(width, image.frameHeight, image.width);
                if (height > draw->rcItem.bottom - 32) { height = max(1, draw->rcItem.bottom - 32); width = max(1, MulDiv(height, image.width, image.frameHeight)); }
                BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                GdiAlphaBlend(draw->hDC, (draw->rcItem.right - width) / 2, (draw->rcItem.bottom - height) / 2,
                    width, height, source, 0, gallery->previewState * image.frameHeight, image.width, image.frameHeight, blend);
                SelectObject(source, old); DeleteDC(source);
            }
            DeleteObject(image.bitmap);
            return TRUE;
        }
        if (draw->itemID == (UINT)-1) return TRUE;
        OrbGalleryItem* item = (OrbGalleryItem*)draw->itemData;
        BOOL selected = (draw->itemState & ODS_SELECTED) != 0;
        HBRUSH selection = CreateSolidBrush(gallery->dark ? RGB(37, 61, 83) : RGB(211, 231, 247));
        FillRect(draw->hDC, &draw->rcItem, selected ? selection : gallery->background);
        DeleteObject(selection);
        if (item && item->preview)
        {
            HDC source = CreateCompatibleDC(draw->hDC);
            if (source)
            {
                HGDIOBJ old = SelectObject(source, item->preview);
                BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                GdiAlphaBlend(draw->hDC, draw->rcItem.left + 8, draw->rcItem.top + 8, 64, 64, source, 0, 0, 64, 64, blend);
                SelectObject(source, old);
                DeleteDC(source);
            }
        }
        WCHAR name[MAX_PATH];
        SendMessageW(draw->hwndItem, LB_GETTEXT, draw->itemID, (LPARAM)name);
        RECT text = draw->rcItem;
        text.left += 88;
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
        if (LOWORD(wParam) == 101) OrbGalleryImport(hwnd, gallery);
        else if (LOWORD(wParam) == 100 && HIWORD(wParam) == LBN_SELCHANGE)
        { gallery->previewState = 0; InvalidateRect(gallery->preview, NULL, TRUE); }
        else if (LOWORD(wParam) == IDOK || (LOWORD(wParam) == 100 && HIWORD(wParam) == LBN_DBLCLK)) OrbGalleryApply(hwnd, gallery);
        else if (LOWORD(wParam) == IDCANCEL) DestroyWindow(hwnd);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
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
