#pragma once
#include "../GR-Taskbar/CryptoFile.h"

#define GR_CURSOR_ROLES 17
#define GR_CURSOR_STEPS 256
static const WCHAR* grCursorRoles[GR_CURSOR_ROLES] = {
    L"Arrow", L"Help", L"AppStarting", L"Wait", L"Crosshair", L"IBeam", L"NWPen", L"No",
    L"SizeNS", L"SizeWE", L"SizeNWSE", L"SizeNESW", L"SizeAll", L"UpArrow", L"Hand", L"Pin", L"Person"
};
static const WCHAR* grCursorLabels[GR_CURSOR_ROLES] = {
    L"Normal select", L"Help select", L"Working in background", L"Busy", L"Precision select", L"Text select",
    L"Handwriting", L"Unavailable", L"Vertical resize", L"Horizontal resize", L"Diagonal resize 1",
    L"Diagonal resize 2", L"Move", L"Alternate select", L"Link select", L"Location select", L"Person select"
};
typedef struct GRCursorScheme {
    WCHAR name[96], file[MAX_PATH];
    WCHAR paths[GR_CURSOR_ROLES][MAX_PATH];
    DWORD present[GR_CURSOR_ROLES], types[GR_CURSOR_ROLES], namePresent, nameType;
    BOOL snapshot;
} GRCursorScheme;
typedef struct GRCursorPreview {
    HCURSOR cursor;
    BOOL shared;
    UINT steps, frame[GR_CURSOR_STEPS], delay[GR_CURSOR_STEPS], step;
    ULONGLONG next;
} GRCursorPreview;

static void GRCursorFreePreview(GRCursorPreview* preview)
{
    if (preview->cursor && !preview->shared) DestroyCursor(preview->cursor);
    ZeroMemory(preview, sizeof(*preview));
}
static DWORD GRCursorDword(const BYTE* data) { DWORD value; memcpy(&value, data, 4); return value; }
static BOOL GRCursorLoadPreview(PCWSTR path, GRCursorPreview* preview)
{
    ZeroMemory(preview, sizeof(*preview));
    WCHAR expanded[MAX_PATH];
    DWORD expandedSize = ExpandEnvironmentStringsW(path, expanded, MAX_PATH);
    if (!expandedSize || expandedSize > MAX_PATH) return FALSE;
    path = expanded;
    PCWSTR ext = PathFindExtensionW(path);
    if (PathIsUNCW(path) || (_wcsicmp(ext, L".cur") && _wcsicmp(ext, L".ani"))) return FALSE;
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &attributes) || attributes.nFileSizeHigh ||
        !attributes.nFileSizeLow || attributes.nFileSizeLow > 16 * 1024 * 1024 ||
        (attributes.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) return FALSE;
    preview->steps = 1; preview->delay[0] = 100;
    if (!_wcsicmp(ext, L".ani")) {
        HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (file == INVALID_HANDLE_VALUE) return FALSE;
        DWORD size = attributes.nFileSizeLow, got = 0;
        BYTE* data = (BYTE*)HeapAlloc(GetProcessHeap(), 0, size);
        BOOL valid = data && ReadFile(file, data, size, &got, NULL) && got == size && size >= 12 &&
            !memcmp(data, "RIFF", 4) && !memcmp(data + 8, "ACON", 4) && GRCursorDword(data + 4) == size - 8;
        CloseHandle(file);
        DWORD frames = 0, steps = 0, jiffies = 6;
        const BYTE *rates = NULL, *sequence = NULL; DWORD rateBytes = 0, sequenceBytes = 0;
        if (valid) for (SIZE_T offset = 12; offset + 8 <= size;) {
            DWORD length = GRCursorDword(data + offset + 4);
            if (length > size - offset - 8) { valid = FALSE; break; }
            const BYTE* chunk = data + offset + 8;
            if (!memcmp(data + offset, "anih", 4) && length >= 36) {
                frames = GRCursorDword(chunk + 4); steps = GRCursorDword(chunk + 8);
                jiffies = GRCursorDword(chunk + 28);
            } else if (!memcmp(data + offset, "rate", 4)) { rates = chunk; rateBytes = length; }
            else if (!memcmp(data + offset, "seq ", 4)) { sequence = chunk; sequenceBytes = length; }
            offset += 8 + (SIZE_T)length + (length & 1);
        }
        if (!steps) steps = frames;
        valid = valid && frames && frames <= GR_CURSOR_STEPS && steps && steps <= GR_CURSOR_STEPS &&
            (!rates || rateBytes >= steps * 4) && (!sequence || sequenceBytes >= steps * 4);
        if (valid) for (DWORD i = 0; i < steps; ++i) {
            preview->frame[i] = sequence ? GRCursorDword(sequence + i * 4) : i % frames;
            DWORD rate = rates ? GRCursorDword(rates + i * 4) : jiffies;
            preview->delay[i] = max(16, min(5000, MulDiv((int)min(rate, 300u), 1000, 60)));
            if (preview->frame[i] >= frames) valid = FALSE;
        }
        if (data) HeapFree(GetProcessHeap(), 0, data);
        if (!valid) return FALSE;
        preview->steps = steps;
    }
    preview->cursor = (HCURSOR)LoadImageW(NULL, path, IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
    return preview->cursor != NULL;
}

static BOOL GRCursorUserDirectory(WCHAR directory[MAX_PATH])
{
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, directory)) ||
        !PathAppendW(directory, L"GR-Taskbar\\Cursors")) return FALSE;
    int result = SHCreateDirectoryExW(NULL, directory, NULL);
    return result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS || result == ERROR_FILE_EXISTS;
}
static BOOL GRCursorImportAsset(PCWSTR source, WCHAR destination[MAX_PATH])
{
    WCHAR expanded[MAX_PATH]; DWORD length = ExpandEnvironmentStringsW(source, expanded, MAX_PATH);
    if (!length || length > MAX_PATH) return FALSE;
    source = expanded;
    GRCursorPreview checked;
    if (!GRCursorLoadPreview(source, &checked)) return FALSE;
    GRCursorFreePreview(&checked);
    BYTE digest[32], existing[32]; WCHAR hex[65], directory[MAX_PATH];
    if (!GRSha256File(source, digest) || !GRCursorUserDirectory(directory)) return FALSE;
    GRHashHex(digest, 32, hex);
    if (swprintf_s(destination, MAX_PATH, L"%s\\%s%s", directory, hex, PathFindExtensionW(source)) < 0) return FALSE;
    if (!_wcsicmp(source, destination) || CopyFileW(source, destination, TRUE)) return TRUE;
    if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) return FALSE;
    return GRSha256File(destination, existing) && !memcmp(existing, digest, 32);
}

static BOOL GRCursorReadCurrent(GRCursorScheme* scheme)
{
    ZeroMemory(scheme, sizeof(*scheme));
    HKEY key; if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Cursors", 0, KEY_QUERY_VALUE, &key)) return FALSE;
    DWORD size = sizeof(scheme->name);
    scheme->snapshot = TRUE;
    scheme->namePresent = RegGetValueW(key, NULL, NULL, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND,
        &scheme->nameType, scheme->name, &size) == ERROR_SUCCESS;
    for (UINT i = 0; i < GR_CURSOR_ROLES; ++i) {
        size = sizeof(scheme->paths[i]);
        scheme->present[i] = RegGetValueW(key, NULL, grCursorRoles[i], RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND,
            &scheme->types[i], scheme->paths[i], &size) == ERROR_SUCCESS;
    }
    RegCloseKey(key); return TRUE;
}

static BOOL GRCursorReadScheme(PCWSTR file, GRCursorScheme* scheme, BOOL importAssets)
{
    ZeroMemory(scheme, sizeof(*scheme));
    if (PathIsUNCW(file) || wcslen(file) >= MAX_PATH) return FALSE;
    GetPrivateProfileStringW(L"GR Taskbar Cursor Scheme", L"Name", L"Custom cursor scheme", scheme->name, ARRAYSIZE(scheme->name), file);
    scheme->snapshot = GetPrivateProfileIntW(L"GR Taskbar Cursor Scheme", L"Snapshot", 0, file) == 1;
    scheme->namePresent = GetPrivateProfileIntW(L"Original values", L"NamePresent", 1, file);
    scheme->nameType = GetPrivateProfileIntW(L"Original values", L"NameType", REG_SZ, file);
    if (scheme->nameType != REG_SZ && scheme->nameType != REG_EXPAND_SZ && scheme->namePresent) return FALSE;
    WCHAR parent[MAX_PATH]; wcscpy_s(parent, MAX_PATH, file); PathRemoveFileSpecW(parent);
    for (UINT i = 0; i < GR_CURSOR_ROLES; ++i) {
        WCHAR value[MAX_PATH] = L"", expanded[MAX_PATH], absolute[MAX_PATH];
        GetPrivateProfileStringW(L"Cursors", grCursorRoles[i], L"", value, MAX_PATH, file);
        scheme->present[i] = GetPrivateProfileIntW(L"Original presence", grCursorRoles[i], 1, file);
        scheme->types[i] = GetPrivateProfileIntW(L"Original types", grCursorRoles[i], REG_SZ, file);
        if (scheme->types[i] != REG_SZ && scheme->types[i] != REG_EXPAND_SZ && scheme->present[i]) return FALSE;
        if (!value[0]) continue;
        DWORD n = ExpandEnvironmentStringsW(value, expanded, MAX_PATH);
        if (!n || n > MAX_PATH || PathIsUNCW(expanded)) return FALSE;
        if (PathIsRelativeW(expanded)) { if (!PathCombineW(absolute, parent, expanded)) return FALSE; }
        else wcscpy_s(absolute, MAX_PATH, expanded);
        if (importAssets) {
            if (!GRCursorImportAsset(absolute, scheme->paths[i])) return FALSE;
        } else {
            GRCursorPreview checked;
            if (!GRCursorLoadPreview(absolute, &checked)) return FALSE;
            GRCursorFreePreview(&checked);
            wcscpy_s(scheme->paths[i], MAX_PATH, scheme->snapshot ? value : absolute);
        }
    }
    wcscpy_s(scheme->file, MAX_PATH, file); return TRUE;
}

static BOOL GRCursorSaveScheme(PCWSTR file, const GRCursorScheme* scheme)
{
    WCHAR temporary[MAX_PATH];
    if (swprintf_s(temporary, MAX_PATH, L"%s.tmp", file) < 0) return FALSE;
    // A UTF-16 BOM makes all subsequent profile writes preserve Unicode names.
    HANDLE output = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) return FALSE;
    WCHAR bom = 0xFEFF; DWORD written;
    BOOL ok = WriteFile(output, &bom, sizeof(bom), &written, NULL) && written == sizeof(bom);
    CloseHandle(output);
    if (ok) ok = WritePrivateProfileStringW(L"GR Taskbar Cursor Scheme", L"Version", L"1", temporary) &&
        WritePrivateProfileStringW(L"GR Taskbar Cursor Scheme", L"Name", scheme->name, temporary);
    if (ok && scheme->snapshot) {
        WCHAR value[24];
        ok = WritePrivateProfileStringW(L"GR Taskbar Cursor Scheme", L"Snapshot", L"1", temporary);
        swprintf_s(value, ARRAYSIZE(value), L"%lu", scheme->namePresent);
        ok = ok && WritePrivateProfileStringW(L"Original values", L"NamePresent", value, temporary);
        swprintf_s(value, ARRAYSIZE(value), L"%lu", scheme->nameType);
        ok = ok && WritePrivateProfileStringW(L"Original values", L"NameType", value, temporary);
    }
    for (UINT i = 0; ok && i < GR_CURSOR_ROLES; ++i)
    {
        ok = WritePrivateProfileStringW(L"Cursors", grCursorRoles[i], scheme->paths[i], temporary);
        if (ok && scheme->snapshot) {
            WCHAR value[24]; swprintf_s(value, ARRAYSIZE(value), L"%lu", scheme->present[i]);
            ok = WritePrivateProfileStringW(L"Original presence", grCursorRoles[i], value, temporary);
            swprintf_s(value, ARRAYSIZE(value), L"%lu", scheme->types[i]);
            ok = ok && WritePrivateProfileStringW(L"Original types", grCursorRoles[i], value, temporary);
        }
    }
    WritePrivateProfileStringW(NULL, NULL, NULL, temporary);
    if (ok) ok = MoveFileExW(temporary, file, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) DeleteFileW(temporary);
    return ok;
}

static BOOL GRCursorWriteValue(HKEY key, PCWSTR name, PCWSTR value, BOOL present, DWORD type)
{
    LSTATUS status;
    if (!present) { status = RegDeleteValueW(key, name); return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND; }
    if (type != REG_SZ && type != REG_EXPAND_SZ) return FALSE;
    return RegSetValueExW(key, name, 0, type, (const BYTE*)value,
        (DWORD)((wcslen(value) + 1) * sizeof(WCHAR))) == ERROR_SUCCESS;
}

static BOOL GRCursorApply(const GRCursorScheme* scheme)
{
    // Validate all assets before changing any Windows role. Preview decoding
    // does not change the pointer, pointer size, colors, speed or accessibility.
    for (UINT i = 0; i < GR_CURSOR_ROLES; ++i) if (scheme->paths[i][0]) {
        GRCursorPreview checked;
        if (!GRCursorLoadPreview(scheme->paths[i], &checked)) return FALSE;
        GRCursorFreePreview(&checked);
    }
    GRCursorScheme before;
    if (!GRCursorReadCurrent(&before)) return FALSE;
    WCHAR directory[MAX_PATH], backup[MAX_PATH];
    if (!GRCursorUserDirectory(directory) || swprintf_s(backup, MAX_PATH, L"%s\\Previous.grcursors", directory) < 0 ||
        !GRCursorSaveScheme(backup, &before)) return FALSE;
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Cursors", 0, KEY_SET_VALUE, &key)) return FALSE;
    BOOL ok = TRUE;
    for (UINT i = 0; ok && i < GR_CURSOR_ROLES; ++i)
        ok = GRCursorWriteValue(key, grCursorRoles[i], scheme->paths[i],
            scheme->snapshot ? scheme->present[i] : TRUE, scheme->snapshot ? scheme->types[i] : REG_SZ);
    if (ok) ok = GRCursorWriteValue(key, NULL, scheme->name,
        scheme->snapshot ? scheme->namePresent : TRUE, scheme->snapshot ? scheme->nameType : REG_SZ);
    if (ok) ok = SystemParametersInfoW(SPI_SETCURSORS, 0, NULL, SPIF_SENDCHANGE | SPIF_UPDATEINIFILE);
    if (!ok) {
        for (UINT i = 0; i < GR_CURSOR_ROLES; ++i)
            GRCursorWriteValue(key, grCursorRoles[i], before.paths[i], before.present[i], before.types[i]);
        GRCursorWriteValue(key, NULL, before.name, before.namePresent, before.nameType);
        SystemParametersInfoW(SPI_SETCURSORS, 0, NULL, SPIF_SENDCHANGE | SPIF_UPDATEINIFILE);
    }
    RegCloseKey(key);
    if (ok) RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\GR-Taskbar\\MouseCursors", L"Scheme", REG_SZ,
        scheme->name, (DWORD)((wcslen(scheme->name) + 1) * sizeof(WCHAR)));
    return ok;
}

static BOOL GRCursorSystemScheme(PCWSTR name, GRCursorScheme* scheme)
{
    WCHAR values[MAX_PATH * (GR_CURSOR_ROLES + 2)]; DWORD bytes = sizeof(values);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Control Panel\\Cursors\\Schemes",
        name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, NULL, values, &bytes)) return FALSE;
    ZeroMemory(scheme, sizeof(*scheme)); wcscpy_s(scheme->name, ARRAYSIZE(scheme->name), name);
    WCHAR* next = values;
    for (UINT i = 0; i < GR_CURSOR_ROLES; ++i) {
        WCHAR* end = wcschr(next, L',');
        if (end) *end = 0;
        if (wcslen(next) >= MAX_PATH) return FALSE;
        wcscpy_s(scheme->paths[i], MAX_PATH, next);
        if (!end) break;
        next = end + 1;
    }
    return TRUE;
}
