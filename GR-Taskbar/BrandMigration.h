#pragma once
#include <shlobj.h>
#include <shlwapi.h>

// GR Studios, 2026-11-25: import legacy settings without overwriting values
// already chosen in GR Taskbar. Compatibility is keyed by the retained CLSID.
static void GRImportMissingSettings(PCWSTR oldPath, PCWSTR newPath, UINT depth)
{
    if (depth > 16) return;
    HKEY oldKey = NULL, newKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, oldPath, 0, KEY_READ | KEY_WOW64_64KEY, &oldKey) != ERROR_SUCCESS) return;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, newPath, 0, NULL, 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, NULL, &newKey, NULL) != ERROR_SUCCESS)
    { RegCloseKey(oldKey); return; }
    DWORD maxName = 0, maxData = 0;
    RegQueryInfoKeyW(oldKey, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &maxName, &maxData, NULL, NULL);
    if (maxName < 32768 && maxData <= 1024 * 1024)
    {
        WCHAR* name = HeapAlloc(GetProcessHeap(), 0, (maxName + 2) * sizeof(WCHAR));
        BYTE* data = HeapAlloc(GetProcessHeap(), 0, max(1, maxData));
        if (name && data)
        {
            for (DWORD index = 0;; ++index)
            {
                DWORD length = maxName + 1, bytes = maxData, type = 0;
                LSTATUS result = RegEnumValueW(oldKey, index, name, &length, NULL, &type, data, &bytes);
                if (result == ERROR_NO_MORE_ITEMS) break;
                if (result == ERROR_SUCCESS && RegQueryValueExW(newKey, name, NULL, NULL, NULL, NULL) == ERROR_FILE_NOT_FOUND)
                    RegSetValueExW(newKey, name, 0, type, data, bytes);
            }
        }
        if (name) HeapFree(GetProcessHeap(), 0, name);
        if (data) HeapFree(GetProcessHeap(), 0, data);
    }
    for (DWORD index = 0;; ++index)
    {
        WCHAR child[256]; DWORD length = ARRAYSIZE(child);
        LSTATUS result = RegEnumKeyExW(oldKey, index, child, &length, NULL, NULL, NULL, NULL);
        if (result == ERROR_NO_MORE_ITEMS) break;
        if (result != ERROR_SUCCESS) continue;
        WCHAR oldChild[1024], newChild[1024];
        if (wcslen(oldPath) + length + 2 < ARRAYSIZE(oldChild) && wcslen(newPath) + length + 2 < ARRAYSIZE(newChild))
        {
            swprintf_s(oldChild, ARRAYSIZE(oldChild), L"%s\\%s", oldPath, child);
            swprintf_s(newChild, ARRAYSIZE(newChild), L"%s\\%s", newPath, child);
            GRImportMissingSettings(oldChild, newChild, depth + 1);
        }
    }
    RegCloseKey(newKey); RegCloseKey(oldKey);
}

static void GRMigrateBrand(BOOL archiveLegacyFolder)
{
    DWORD migrated = 0, bytes = sizeof(migrated);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\GR-Taskbar", L"BrandMigrationComplete", RRF_RT_REG_DWORD, NULL, &migrated, &bytes);
    if (!migrated)
    {
        GRImportMissingSettings(L"Software\\ExplorerPatcher", L"Software\\GR-Taskbar", 0);
        GRImportMissingSettings(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ExplorerPatcher",
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\GR-Taskbar", 0);
        WCHAR base[MAX_PATH], oldRoot[MAX_PATH], newRoot[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, base)))
        {
            wcscpy_s(oldRoot, ARRAYSIZE(oldRoot), base); PathAppendW(oldRoot, L"ExplorerPatcher");
            wcscpy_s(newRoot, ARRAYSIZE(newRoot), base); PathAppendW(newRoot, L"GR-Taskbar");
            const PCWSTR folders[] = { L"Orbs", L"Themes" };
            for (UINT i = 0; i < ARRAYSIZE(folders); ++i)
            {
                WCHAR oldDir[MAX_PATH], newDir[MAX_PATH], pattern[MAX_PATH];
                wcscpy_s(oldDir, ARRAYSIZE(oldDir), oldRoot); PathAppendW(oldDir, folders[i]);
                wcscpy_s(newDir, ARRAYSIZE(newDir), newRoot); PathAppendW(newDir, folders[i]);
                SHCreateDirectoryExW(NULL, newDir, NULL);
                wcscpy_s(pattern, ARRAYSIZE(pattern), oldDir); PathAppendW(pattern, L"*");
                WIN32_FIND_DATAW found;
                HANDLE search = FindFirstFileW(pattern, &found);
                if (search != INVALID_HANDLE_VALUE)
                {
                    do
                    {
                        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                        WCHAR source[MAX_PATH], target[MAX_PATH];
                        wcscpy_s(source, ARRAYSIZE(source), oldDir); wcscpy_s(target, ARRAYSIZE(target), newDir);
                        if (PathAppendW(source, found.cFileName) && PathAppendW(target, found.cFileName)) CopyFileW(source, target, TRUE);
                    } while (FindNextFileW(search, &found));
                    FindClose(search);
                }
            }
            WCHAR selected[MAX_PATH] = L""; bytes = sizeof(selected);
            if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\GR-Taskbar", L"CustomOrbImage", RRF_RT_REG_SZ, NULL, selected, &bytes) == ERROR_SUCCESS && PathIsPrefixW(oldRoot, selected))
            {
                WCHAR replacement[MAX_PATH]; wcscpy_s(replacement, ARRAYSIZE(replacement), newRoot);
                PathAppendW(replacement, L"Orbs"); PathAppendW(replacement, PathFindFileNameW(selected));
                if (PathFileExistsW(replacement)) RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\GR-Taskbar", L"CustomOrbImage", REG_SZ, replacement, (DWORD)((wcslen(replacement) + 1) * sizeof(WCHAR)));
            }
        }
        // An explicit URL override must also follow the repository rename.
        const PCWSTR names[] = { L"UpdateURL", L"UpdateURLStaging" };
        for (UINT i = 0; i < ARRAYSIZE(names); ++i)
        {
            WCHAR url[1024] = L""; bytes = sizeof(url);
            if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\GR-Taskbar", names[i], RRF_RT_REG_SZ, NULL, url, &bytes) == ERROR_SUCCESS &&
                (StrStrIW(url, L"GRStudiosAI/ExplorerPatcher") || StrStrIW(url, L"valinet/ExplorerPatcher")))
            {
                PCWSTR replacement = i ? L"https://api.github.com/repos/GRStudiosAI/GR-Taskbar/releases?per_page=1" : L"https://github.com/GRStudiosAI/GR-Taskbar/releases/latest";
                RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\GR-Taskbar", names[i], REG_SZ, replacement, (DWORD)((wcslen(replacement) + 1) * sizeof(WCHAR)));
            }
        }
        migrated = 1;
        RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\GR-Taskbar", L"BrandMigrationComplete", REG_DWORD, &migrated, sizeof(migrated));
    }
    // The caller only requests this once the newly installed shell is running.
    // Preserve legacy data as a rollback backup instead of deleting it.
    if (archiveLegacyFolder)
    {
        WCHAR oldRoot[MAX_PATH], backup[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, oldRoot)))
        {
            wcscpy_s(backup, ARRAYSIZE(backup), oldRoot);
            PathAppendW(oldRoot, L"ExplorerPatcher");
            PathAppendW(backup, L"GR-Taskbar\\LegacyBackup");
            MoveFileW(oldRoot, backup);
        }
    }
}
