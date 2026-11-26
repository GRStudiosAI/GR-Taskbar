#pragma once
// GR Studios AI LLC: recoverable installer file servicing, 2026.
#include <restartmanager.h>
#include <sfc.h>
#include <stdarg.h>
#pragma comment(lib, "Rstrtmgr.lib")
#pragma comment(lib, "sfc.lib")

typedef struct GRFileChange {
    struct GRFileChange* next;
    WCHAR path[MAX_PATH], backup[MAX_PATH];
    BOOL hadOld, installed;
} GRFileChange;
static GRFileChange* grChanges;
static BOOL grNeedsReboot;
static BOOL grServicingDeferred;
static DWORD grFailureCode;
static WCHAR grFailurePath[MAX_PATH], grFailureOperation[96], grLogPath[MAX_PATH];
static WCHAR grInstallRoot[MAX_PATH];
static HANDLE grLog = INVALID_HANDLE_VALUE;
static BOOL grStoppedShell;
static HANDLE grJournal = INVALID_HANDLE_VALUE;
static BOOL grJournalReady;
static BOOL grLockOwnerFound, grFailureIsLock;
static WCHAR grJournalPath[MAX_PATH];
typedef struct GRJournalEntry {
    WCHAR path[MAX_PATH], backup[MAX_PATH];
    DWORD hadOld;
} GRJournalEntry;

static void GRCloseSettings(void)
{
    HWND window = FindWindowW(L"GRTaskbar_GUI_" _T(EP_CLSID), NULL);
    if (!window) return;
    DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    PostMessageW(window, WM_CLOSE, 0, 0);
    if (process) { WaitForSingleObject(process, 5000); CloseHandle(process); }
}

static void GRLog(PCWSTR operation, PCWSTR path, DWORD code)
{
    DWORD saved = GetLastError();
    SYSTEMTIME now; GetLocalTime(&now);
    WCHAR message[512] = L"", line[2048];
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, code, 0, message, ARRAYSIZE(message), NULL);
    swprintf_s(line, ARRAYSIZE(line), L"%04u-%02u-%02u %02u:%02u:%02u pid=%lu operation=%s path=\"%s\" win32=%lu (0x%08lX) %s\r\n",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
        GetCurrentProcessId(), operation, path ? path : L"", code, code, message);
    if (grLog != INVALID_HANDLE_VALUE) {
        char utf8[8192]; DWORD written;
        int length = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), NULL, NULL);
        if (length > 0) WriteFile(grLog, utf8, (DWORD)length - 1, &written, NULL);
        FlushFileBuffers(grLog);
    }
    OutputDebugStringW(line);
    SetLastError(saved);
}

static BOOL GRFail(PCWSTR operation, PCWSTR path, DWORD code)
{
    if (!code) code = ERROR_GEN_FAILURE;
    GRLog(operation, path, code);
    if (!grFailureCode) {
        grFailureCode = code;
        grFailureIsLock = code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION ||
            code == ERROR_USER_MAPPED_FILE || (code == ERROR_ACCESS_DENIED && grLockOwnerFound);
        wcscpy_s(grFailurePath, MAX_PATH, path ? path : L"");
        wcscpy_s(grFailureOperation, ARRAYSIZE(grFailureOperation), operation);
    }
    SetLastError(code);
    return FALSE;
}

static BOOL GRStopExplorer(HWND tray)
{
    if (!tray) return TRUE;
    DWORD pid = 0, session = 0, ownSession = 0;
    GetWindowThreadProcessId(tray, &pid);
    if (!pid || !ProcessIdToSessionId(pid, &session) ||
        !ProcessIdToSessionId(GetCurrentProcessId(), &ownSession) || session != ownSession)
        return GRFail(L"validate shell session", L"explorer.exe", ERROR_ACCESS_DENIED);
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, pid);
    if (!process) return GRFail(L"open shell process", L"explorer.exe", GetLastError());
    WCHAR image[MAX_PATH], expected[MAX_PATH]; DWORD size = MAX_PATH;
    GetWindowsDirectoryW(expected, MAX_PATH); wcscat_s(expected, MAX_PATH, L"\\explorer.exe");
    if (!QueryFullProcessImageNameW(process, 0, image, &size) || _wcsicmp(image, expected)) {
        CloseHandle(process); return GRFail(L"validate shell executable", L"explorer.exe", ERROR_ACCESS_DENIED);
    }
    DWORD_PTR result = 0;
    SendMessageTimeoutW(tray, 1460, 0, 0, SMTO_ABORTIFHUNG, 2000, &result);
    if (WaitForSingleObject(process, 4000) != WAIT_OBJECT_0) {
        GRLog(L"graceful shell shutdown timed out; terminate validated shell PID", image, WAIT_TIMEOUT);
        if (!TerminateProcess(process, 0) || WaitForSingleObject(process, 5000) != WAIT_OBJECT_0) {
            DWORD error = GetLastError(); CloseHandle(process); return GRFail(L"stop shell", image, error);
        }
    }
    CloseHandle(process); grStoppedShell = TRUE;
    GRLog(L"shell stopped and process exit confirmed", image, 0);
    return TRUE;
}

static void GRInitLog(void)
{
    WCHAR directory[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, directory))) return;
    wcscat_s(directory, MAX_PATH, L"\\GR-Taskbar\\Logs");
    SHCreateDirectoryExW(NULL, directory, NULL);
    SYSTEMTIME now; GetLocalTime(&now);
    swprintf_s(grLogPath, MAX_PATH, L"%s\\Setup-%04u%02u%02u-%02u%02u%02u-%lu.log",
        directory, now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, GetCurrentProcessId());
    grLog = CreateFileW(grLogPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    GRLog(L"begin servicing", L"GR Taskbar " _T(VER_WITH_DOTS), 0);
}

static BOOL GRTemporaryLock(DWORD error)
{
    return error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION ||
        error == ERROR_USER_MAPPED_FILE || error == ERROR_ACCESS_DENIED;
}

// Query only. Never force-close unrelated applications or services holding a DLL.
static void GRLogLocks(PCWSTR path)
{
    DWORD session; WCHAR key[CCH_RM_SESSION_KEY + 1] = L"";
    DWORD result = RmStartSession(&session, 0, key);
    if (result != ERROR_SUCCESS) { GRLog(L"RmStartSession", path, result); return; }
    result = RmRegisterResources(session, 1, &path, 0, NULL, 0, NULL);
    if (result == ERROR_SUCCESS) {
        UINT needed = 0, count = 0; DWORD reasons = 0;
        result = RmGetList(session, &needed, &count, NULL, &reasons);
        if (result == ERROR_MORE_DATA && needed && needed <= 4096) {
            RM_PROCESS_INFO* list = (RM_PROCESS_INFO*)calloc(needed, sizeof(*list));
            if (list) {
                count = needed;
                result = RmGetList(session, &needed, &count, list, &reasons);
                if (result == ERROR_SUCCESS) for (UINT i = 0; i < count; ++i) {
                    grLockOwnerFound = TRUE;
                    WCHAR operation[256];
                    swprintf_s(operation, ARRAYSIZE(operation), L"lock owner pid=%lu session=%lu app=%s restartable=%d",
                        list[i].Process.dwProcessId, list[i].TSSessionId, list[i].strAppName, list[i].bRestartable);
                    GRLog(operation, path, 0);
                }
                free(list);
            }
        }
        if (result != ERROR_SUCCESS) GRLog(L"RmGetList", path, result);
        if (reasons) GRLog(L"Restart Manager reboot reasons", path, reasons);
    } else GRLog(L"RmRegisterResources", path, result);
    RmEndSession(session);
}

static BOOL GRMoveWithRetry(PCWSTR source, PCWSTR destination)
{
    grLockOwnerFound = FALSE;
    DWORD error = ERROR_SUCCESS;
    for (UINT attempt = 0; attempt < 6; ++attempt) {
        if (MoveFileExW(source, destination, MOVEFILE_WRITE_THROUGH)) return TRUE;
        error = GetLastError();
        GRLog(L"MoveFileExW retry", source, error);
        if (!GRTemporaryLock(error)) break;
        if (!attempt) GRLogLocks(source);
        Sleep(100 * (attempt + 1));
    }
    SetLastError(error); return FALSE;
}

static BOOL GRBelowRoot(PCWSTR path, PCWSTR root)
{
    size_t length = wcslen(root);
    return !_wcsnicmp(path, root, length) && path[length] == L'\\';
}

static BOOL GRValidateTarget(PCWSTR path)
{
    WCHAR full[MAX_PATH], parent[MAX_PATH], windows[MAX_PATH];
    DWORD length = GetFullPathNameW(path, MAX_PATH, full, NULL);
    if (!length || length >= MAX_PATH || _wcsicmp(full, path) ||
        wcschr(path, L'\n') || wcschr(path + 2, L':'))
        return GRFail(L"reject noncanonical target", path, ERROR_INVALID_NAME);
    if (!GetWindowsDirectoryW(windows, MAX_PATH)) return GRFail(L"GetWindowsDirectoryW", path, GetLastError());
    WCHAR shortcut[MAX_PATH] = L"";
    SHGetFolderPathW(NULL, CSIDL_COMMON_PROGRAMS, NULL, SHGFP_TYPE_CURRENT, shortcut);
    wcscat_s(shortcut, MAX_PATH, L"\\GR Taskbar\\GR Taskbar Settings (GR Taskbar).lnk");
    if (!GRBelowRoot(path, grInstallRoot) && _wcsicmp(path, shortcut)) {
        WCHAR allowed[MAX_PATH]; BOOL permitted = FALSE;
        const PCWSTR roots[] = { L"", L"\\SystemApps\\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy",
            L"\\SystemApps\\ShellExperienceHost_cw5n1h2txyewy" };
        for (UINT i = 0; i < ARRAYSIZE(roots); ++i) {
            swprintf_s(allowed, MAX_PATH, L"%s%s", windows, roots[i]);
            wcscpy_s(parent, MAX_PATH, path); PathRemoveFileSpecW(parent);
            if (!_wcsicmp(parent, allowed)) { permitted = TRUE; break; }
        }
        if (!permitted) return GRFail(L"reject target outside managed locations", path, ERROR_ACCESS_DENIED);
        // Only product-owned injection files may be replaced outside Program Files.
        if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) {
            DWORD ignored, infoLength = GetFileVersionInfoSizeW(path, &ignored);
            BYTE* info = infoLength ? (BYTE*)malloc(infoLength) : NULL;
            BOOL owned = FALSE;
            if (info && GetFileVersionInfoW(path, 0, infoLength, info)) {
                PCWSTR product = NULL; UINT characters = 0;
                if (VerQueryValueW(info, L"\\StringFileInfo\\040904b0\\ProductName", (void**)&product, &characters))
                    owned = !_wcsicmp(product, L"GR Taskbar") || !_wcsicmp(product, L"ExplorerPatcher");
            }
            free(info);
            // The original wincorlib link is removed as a link, never followed.
            DWORD attributes = GetFileAttributesW(path);
            BOOL ownLink = (attributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
                !_wcsicmp(PathFindFileNameW(path), L"wincorlib_orig.dll");
            WCHAR startHost[MAX_PATH];
            swprintf_s(startHost, MAX_PATH, L"%s\\SystemApps\\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy", windows);
            // These are the product's side-by-side compatibility copies, not
            // Windows' original JumpViewUI.dll or StartUI.dll. Keep WRP below.
            BOOL compatibilityCopy = !_wcsicmp(parent, startHost) &&
                (!_wcsicmp(PathFindFileNameW(path), L"JumpViewUI_.dll") ||
                 !_wcsicmp(PathFindFileNameW(path), L"StartUI_.dll"));
            if (!owned && !ownLink && !compatibilityCopy) return GRFail(L"refuse unowned Windows component", path, ERROR_ACCESS_DENIED);
        }
    }
    wcscpy_s(parent, MAX_PATH, path); PathRemoveFileSpecW(parent);
    do {
        DWORD attributes = GetFileAttributesW(parent);
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            return GRFail(L"reject reparse directory", parent, ERROR_ACCESS_DENIED);
    } while (PathRemoveFileSpecW(parent) && wcslen(parent) > 3);
    if (SfcIsFileProtected(NULL, path)) return GRFail(L"refuse Windows protected file", path, ERROR_ACCESS_DENIED);
    return TRUE;
}

static BOOL GRUniqueSibling(PCWSTR target, PWSTR temporary)
{
    WCHAR parent[MAX_PATH]; wcscpy_s(parent, MAX_PATH, target); PathRemoveFileSpecW(parent);
    return GetTempFileNameW(parent, L"grt", 0, temporary) != 0;
}

static BOOL GRFilesEqual(PCWSTR first, PCWSTR second)
{
    HANDLE a = CreateFileW(first, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (a == INVALID_HANDLE_VALUE) return FALSE;
    HANDLE b = CreateFileW(second, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (b == INVALID_HANDLE_VALUE) { CloseHandle(a); return FALSE; }
    LARGE_INTEGER x, y; BOOL equal = GetFileSizeEx(a, &x) && GetFileSizeEx(b, &y) && x.QuadPart == y.QuadPart;
    BYTE left[32768], right[32768];
    while (equal) {
        DWORD n = 0, m = 0;
        if (!ReadFile(a, left, sizeof(left), &n, NULL) || !ReadFile(b, right, sizeof(right), &m, NULL) ||
            n != m || memcmp(left, right, n)) { equal = FALSE; break; }
        if (!n) break;
    }
    CloseHandle(b); CloseHandle(a); return equal;
}

static void GRCleanupBackup(PCWSTR path);

// Loaded DLLs can deny deletion yet permit same-volume renaming. Restore the
// old name first and dispose of the displaced image only after it is released.
static BOOL GRDiscardPublished(PCWSTR path)
{
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES &&
        (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND)) return TRUE;
    WCHAR discard[MAX_PATH];
    if (!GRUniqueSibling(path, discard)) return FALSE;
    if (!DeleteFileW(discard)) return FALSE;
    if (!GRMoveWithRetry(path, discard)) return FALSE;
    GRLog(L"displace new file for rollback", path, 0);
    GRCleanupBackup(discard);
    return TRUE;
}

static BOOL GRAppendJournal(const GRFileChange* change)
{
    if (grJournal == INVALID_HANDLE_VALUE) return TRUE; // /extract has no desktop transaction.
    GRJournalEntry entry = { 0 }; DWORD written;
    wcscpy_s(entry.path, MAX_PATH, change->path); wcscpy_s(entry.backup, MAX_PATH, change->backup);
    entry.hadOld = (DWORD)change->hadOld;
    if (!WriteFile(grJournal, &entry, sizeof(entry), &written, NULL) || written != sizeof(entry) || !FlushFileBuffers(grJournal))
        return GRFail(L"persist rollback journal", grJournalPath, GetLastError());
    return TRUE;
}

// Journal is in the ACL-protected installation folder. Recovery is idempotent:
// a missing backup means either the old file was never moved, or was restored.
static BOOL GRBeginFiles(void)
{
    grJournalReady = FALSE;
    swprintf_s(grJournalPath, MAX_PATH, L"%s\\servicing.rollback", grInstallRoot);
    if (!GRValidateTarget(grJournalPath)) return FALSE;
    DWORD attributes = GetFileAttributesW(grJournalPath);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return GRFail(L"reject reparse journal", grJournalPath, ERROR_ACCESS_DENIED);
    HANDLE previous = CreateFileW(grJournalPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (previous != INVALID_HANDLE_VALUE) {
        DWORD committed = 0, read = 0; BOOL ok = ReadFile(previous, &committed, sizeof(committed), &read, NULL) && read == sizeof(committed);
        DWORD recoveryError = ok ? ERROR_SUCCESS : ERROR_INVALID_DATA;
        LARGE_INTEGER length; if (!GetFileSizeEx(previous, &length) || length.QuadPart < 4 ||
            (length.QuadPart - 4) % sizeof(GRJournalEntry) != 0 || committed > 1) { ok = FALSE; recoveryError = ERROR_INVALID_DATA; }
        GRJournalEntry entry;
        // Reverse order also handles a file removed and recreated within one run.
        for (LONGLONG position = ok ? length.QuadPart - sizeof(entry) : 0; ok && position >= 4; position -= sizeof(entry)) {
            LARGE_INTEGER offset; offset.QuadPart = position;
            if (!SetFilePointerEx(previous, offset, NULL, FILE_BEGIN) ||
                !ReadFile(previous, &entry, sizeof(entry), &read, NULL) || read != sizeof(entry)) { recoveryError = ERROR_INVALID_DATA; ok = FALSE; break; }
            entry.path[MAX_PATH - 1] = 0; entry.backup[MAX_PATH - 1] = 0;
            WCHAR parent[MAX_PATH], backupParent[MAX_PATH];
            wcscpy_s(parent, MAX_PATH, entry.path); PathRemoveFileSpecW(parent);
            wcscpy_s(backupParent, MAX_PATH, entry.backup); PathRemoveFileSpecW(backupParent);
            if (!GRValidateTarget(entry.path) || (entry.hadOld && (_wcsicmp(parent, backupParent) ||
                _wcsnicmp(PathFindFileNameW(entry.backup), L"grt", 3)))) { recoveryError = grFailureCode ? grFailureCode : ERROR_INVALID_DATA; ok = FALSE; break; }
            if (committed) {
                if (entry.hadOld && GetFileAttributesW(entry.backup) != INVALID_FILE_ATTRIBUTES && !DeleteFileW(entry.backup))
                    GRLog(L"retain committed backup", entry.backup, GetLastError());
            } else if (entry.hadOld) {
                if (GetFileAttributesW(entry.backup) != INVALID_FILE_ATTRIBUTES) {
                    if (!GRDiscardPublished(entry.path)) { recoveryError = GetLastError(); GRFail(L"recover: displace published file", entry.path, recoveryError); ok = FALSE; break; }
                    if (!GRMoveWithRetry(entry.backup, entry.path)) { recoveryError = GetLastError(); GRFail(L"recover: restore backup", entry.path, recoveryError); ok = FALSE; break; }
                    GRLog(L"recover interrupted transaction", entry.path, 0);
                }
            } else if (!GRDiscardPublished(entry.path)) { recoveryError = GetLastError(); GRFail(L"recover: displace published file", entry.path, recoveryError); ok = FALSE; break; }
        }
        CloseHandle(previous);
        if (!ok) return GRFail(L"recover interrupted file transaction; journal retained", grJournalPath, recoveryError ? recoveryError : ERROR_GEN_FAILURE);
        if (!DeleteFileW(grJournalPath)) return GRFail(L"remove recovered journal", grJournalPath, GetLastError());
    } else if (GetLastError() != ERROR_FILE_NOT_FOUND) return GRFail(L"open rollback journal", grJournalPath, GetLastError());
    grJournal = CreateFileW(grJournalPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (grJournal == INVALID_HANDLE_VALUE) return GRFail(L"create rollback journal", grJournalPath, GetLastError());
    DWORD committed = 0, written;
    if (!WriteFile(grJournal, &committed, 4, &written, NULL) || written != 4 || !FlushFileBuffers(grJournal))
        return GRFail(L"initialize rollback journal", grJournalPath, GetLastError());
    grJournalReady = TRUE;
    return TRUE;
}

static void GRCloseJournal(BOOL remove)
{
    BOOL owned = grJournal != INVALID_HANDLE_VALUE;
    if (grJournal != INVALID_HANDLE_VALUE) { CloseHandle(grJournal); grJournal = INVALID_HANDLE_VALUE; }
    if (remove && owned && grJournalPath[0] && !DeleteFileW(grJournalPath)) GRLog(L"remove completed journal", grJournalPath, GetLastError());
    grJournalReady = FALSE;
}

// The complete new file is written/flushed before touching the installed file.
// Backups stay on the destination volume, avoiding the old AppData cross-volume move.
static BOOL GRPublishFile(PCWSTR path, PCWSTR staged, BOOL remove)
{
    if (!GRValidateTarget(path)) return FALSE;
    DWORD attributes = GetFileAttributesW(path);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
        return GRFail(L"refuse directory as payload file", path, ERROR_DIRECTORY);
    if (attributes == INVALID_FILE_ATTRIBUTES && GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND)
        return GRFail(L"GetFileAttributesW", path, GetLastError());
    if (remove && attributes == INVALID_FILE_ATTRIBUTES) return TRUE;
    if (!remove && attributes != INVALID_FILE_ATTRIBUTES && GRFilesEqual(path, staged)) {
        GRLog(L"unchanged file retained", path, 0);
        return TRUE;
    }
    // One journal entry per destination. A delete followed by a create must
    // retain the original backup, including across interrupted recovery.
    for (GRFileChange* existing = grChanges; existing; existing = existing->next) {
        if (_wcsicmp(existing->path, path)) continue;
        if (existing->installed && !GRDiscardPublished(path))
            return GRFail(L"replace staged transaction version", path, GetLastError());
        existing->installed = FALSE;
        if (!remove) {
            if (!GRMoveWithRetry(staged, path)) return GRFail(L"publish staged transaction version", path, GetLastError());
            existing->installed = TRUE;
        }
        return TRUE;
    }
    GRFileChange* change = (GRFileChange*)calloc(1, sizeof(*change));
    if (!change) return GRFail(L"allocate transaction entry", path, ERROR_OUTOFMEMORY);
    wcscpy_s(change->path, MAX_PATH, path);
    change->hadOld = attributes != INVALID_FILE_ATTRIBUTES;
    if (change->hadOld) {
        if (!GRUniqueSibling(path, change->backup)) { free(change); return GRFail(L"reserve backup", path, GetLastError()); }
        if (!DeleteFileW(change->backup)) { DWORD error = GetLastError(); free(change); return GRFail(L"prepare backup name", path, error); }
    }
    if (!GRAppendJournal(change)) { free(change); return FALSE; }
    if (change->hadOld) {
        if (!GRMoveWithRetry(path, change->backup)) {
            DWORD error = GetLastError(); free(change); return GRFail(L"backup installed file", path, error);
        }
        GRLog(L"backup created", change->backup, 0);
    }
    change->next = grChanges; grChanges = change;
    if (!remove) {
        if (!GRMoveWithRetry(staged, path)) return GRFail(L"publish staged file", path, GetLastError());
        change->installed = TRUE;
    }
    GRLog(remove ? L"stage removal" : L"publish staged file", path, 0);
    return TRUE;
}

static BOOL GRRollbackFiles(void)
{
    BOOL ok = TRUE;
    for (GRFileChange* change = grChanges; change; change = change->next) {
        if (change->installed && !GRDiscardPublished(change->path)) {
            GRLog(L"rollback remove new file FAILED; backup retained", change->path, GetLastError()); ok = FALSE; continue;
        }
        if (change->hadOld && !GRMoveWithRetry(change->backup, change->path)) {
            GRLog(L"rollback restore FAILED; backup retained", change->backup, GetLastError()); ok = FALSE;
        } else GRLog(L"rollback restored", change->path, 0);
    }
    return ok;
}

static BOOL GRInstallSymbolicLink(PCWSTR path, PCWSTR target)
{
    WCHAR staged[MAX_PATH];
    if (!GRUniqueSibling(path, staged)) return GRFail(L"reserve staged link", path, GetLastError());
    if (!DeleteFileW(staged)) return GRFail(L"prepare staged link", staged, GetLastError());
    if (!CreateSymbolicLinkW(staged, target, 0)) return GRFail(L"create staged link", staged, GetLastError());
    BOOL ok = GRPublishFile(path, staged, FALSE);
    if (!ok) DeleteFileW(staged);
    return ok;
}

static void GRCleanupBackup(PCWSTR path)
{
    if (DeleteFileW(path) || GetLastError() == ERROR_FILE_NOT_FOUND) return;
    DWORD error = GetLastError(); GRLog(L"backup cleanup deferred", path, error); GRLogLocks(path);
    if (MoveFileExW(path, NULL, MOVEFILE_DELAY_UNTIL_REBOOT)) grNeedsReboot = TRUE;
    else GRLog(L"schedule backup deletion FAILED; retained", path, GetLastError());
}

static BOOL GRCommitFiles(void)
{
    if (grJournal != INVALID_HANDLE_VALUE) {
        DWORD committed = 1, written;
        SetFilePointer(grJournal, 0, NULL, FILE_BEGIN);
        if (!WriteFile(grJournal, &committed, 4, &written, NULL) || written != 4 || !FlushFileBuffers(grJournal))
            return GRFail(L"commit rollback journal", grJournalPath, GetLastError());
    }
    for (GRFileChange* change = grChanges; change; change = change->next) {
        if (change->hadOld) GRCleanupBackup(change->backup);
        // Stale legacy .prev files are cleanup, never a prerequisite for installation.
        WCHAR previous[MAX_PATH]; wcscpy_s(previous, MAX_PATH, change->path);
        PathRemoveExtensionW(previous); wcscat_s(previous, MAX_PATH, L".prev");
        if (GRBelowRoot(previous, grInstallRoot) && GetFileAttributesW(previous) != INVALID_FILE_ATTRIBUTES) GRCleanupBackup(previous);
    }
    GRLog(L"file transaction committed", grInstallRoot, 0);
    GRCloseJournal(TRUE);
    return TRUE;
}

static void GRFreeChanges(void)
{
    while (grChanges) { GRFileChange* next = grChanges->next; free(grChanges); grChanges = next; }
}

// Defer the entire operation, not individual DLLs, so a failed install never
// queues a mixture of old and new binaries. RunOnce executes after reboot/logon.
static BOOL GRScheduleRebootRetry(BOOL install)
{
    WCHAR source[MAX_PATH], pending[MAX_PATH], command[2 * MAX_PATH];
    WCHAR target[MAX_PATH]; swprintf_s(target, MAX_PATH, L"%s\\pending.exe", grInstallRoot);
    if (!GetModuleFileNameW(NULL, source, MAX_PATH) || !GRUniqueSibling(target, pending))
        return FALSE;
    // Reserve in the protected Program Files parent, not the user's temp folder.
    if (!CopyFileW(source, pending, FALSE)) { DeleteFileW(pending); return FALSE; }
    swprintf_s(command, ARRAYSIZE(command), L"\"%s\" /resume_%s", pending, install ? L"install" : L"uninstall");
    HKEY key;
    LSTATUS error = RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
        0, NULL, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, NULL, &key, NULL);
    if (error == ERROR_SUCCESS) {
        error = RegSetValueExW(key, L"!GRTaskbarServicing", 0, REG_SZ, (const BYTE*)command,
            (DWORD)((wcslen(command) + 1) * sizeof(WCHAR)));
        RegCloseKey(key);
    }
    if (error != ERROR_SUCCESS) { GRLog(L"schedule full servicing after reboot FAILED", pending, (DWORD)error); DeleteFileW(pending); return FALSE; }
    GRLog(L"full servicing scheduled after reboot/logon", pending, 0);
    grNeedsReboot = TRUE;
    grServicingDeferred = TRUE;
    return TRUE;
}
