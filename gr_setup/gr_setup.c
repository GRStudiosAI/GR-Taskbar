#include <Windows.h>
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#include <stdio.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include "resources/resource.h"
#include "../GR-Taskbar/utility.h"
#include "../version.h"
#include <zlib.h>
#include <minizip/unzip.h>
#ifdef WITH_ENCRYPTION
#include "rijndael-alg-fst.c" // Include the C file for __forceinline to work
#endif
#pragma comment(lib, "zs.lib")

#include "Servicing.h"
#include "InstallerBranding.h"

static UINT g_uFailureLine;

#define CHECK_OK(x) ((!(x) && !g_uFailureLine) ? (g_uFailureLine = __LINE__, GRFail(L"servicing step", wszPath, GetLastError())) : (x))

static BOOL GRRestartShell(HANDLE token)
{
    if (!grStoppedShell) return TRUE;
    // Windows can auto-restart Explorer during publication. An already
    // responding tray may still have the pre-update DLL mapped. Only accept a
    // process created after this post-publication boundary.
    FILETIME published;
    GetSystemTimePreciseAsFileTime(&published);
    UINT staleRestarts = 0;
    if (!FindWindowW(L"Shell_TrayWnd", NULL)) StartExplorerWithDelay(250, token);
    for (UINT attempt = 0; attempt < 40; ++attempt) {
        HWND tray = FindWindowW(L"Shell_TrayWnd", NULL); DWORD_PTR result;
        if (tray) {
            DWORD pid = 0; BOOL fresh = FALSE;
            GetWindowThreadProcessId(tray, &pid);
            if (!GRProcessCreatedSince(pid, &published, &fresh))
                return GRFail(L"verify shell process creation time", L"explorer.exe", GetLastError());
            if (!fresh) {
                if (staleRestarts++ >= 2)
                    return GRFail(L"stale shell persisted after publication", L"explorer.exe", ERROR_RETRY);
                GRLog(L"shell auto-restarted before publication finished; reload current binaries", L"explorer.exe", 0);
                if (!GRStopExplorer(tray)) return FALSE;
                if (!FindWindowW(L"Shell_TrayWnd", NULL)) StartExplorerWithDelay(250, token);
                continue;
            }
            if (SendMessageTimeoutW(tray, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 250, &result)) {
                GRLog(L"Explorer created after publication and taskbar responding", L"explorer.exe", 0);
                grStoppedShell = FALSE; return TRUE;
            }
        }
        Sleep(250);
    }
    return GRFail(L"verify Explorer restart", L"explorer.exe", ERROR_TIMEOUT);
}
BOOL SetupShortcut(BOOL install, WCHAR* application, WCHAR* arguments)
{
    WCHAR directory[MAX_PATH], path[MAX_PATH], staged[MAX_PATH], icon[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_COMMON_PROGRAMS, NULL, SHGFP_TYPE_CURRENT, directory)))
        return GRFail(L"locate Start menu Programs folder", L"", ERROR_PATH_NOT_FOUND);
    wcscat_s(directory, MAX_PATH, L"\\GR Taskbar");
    swprintf_s(path, MAX_PATH, L"%s\\GR Taskbar Settings (GR Taskbar).lnk", directory);
    if (!install) return GRPublishFile(path, NULL, TRUE);
    if (!CreateDirectoryW(directory, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return GRFail(L"create shortcut directory", directory, GetLastError());
    if (!GRUniqueSibling(path, staged)) return GRFail(L"create staged shortcut", path, GetLastError());
    HRESULT apartment = CoInitialize(NULL), result = apartment;
    IShellLinkW* link = NULL; IPersistFile* persist = NULL;
    if (SUCCEEDED(apartment) || apartment == RPC_E_CHANGED_MODE) {
        result = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void**)&link);
        if (SUCCEEDED(result)) result = link->lpVtbl->SetPath(link, application);
        if (SUCCEEDED(result)) result = link->lpVtbl->SetArguments(link, arguments);
        swprintf_s(icon, MAX_PATH, L"%s\\GR-Settings.dll", grInstallRoot);
        if (SUCCEEDED(result)) result = link->lpVtbl->SetIconLocation(link, icon, 0);
        if (SUCCEEDED(result)) result = link->lpVtbl->SetDescription(link, L"GR Taskbar settings");
        if (SUCCEEDED(result)) result = link->lpVtbl->QueryInterface(link, &IID_IPersistFile, (void**)&persist);
        if (SUCCEEDED(result)) result = persist->lpVtbl->Save(persist, staged, TRUE);
    }
    if (persist) persist->lpVtbl->Release(persist);
    if (link) link->lpVtbl->Release(link);
    if (SUCCEEDED(apartment)) CoUninitialize();
    BOOL ok = SUCCEEDED(result) ? GRPublishFile(path, staged, FALSE) :
        GRFail(L"create ShellLink (HRESULT logged)", path, (DWORD)result);
    if (GetFileAttributesW(staged) != INVALID_FILE_ATTRIBUTES) DeleteFileW(staged);
    return ok;
}
BOOL SetupUninstallEntry(BOOL bInstall, WCHAR* wszPath)
{
    DWORD dwLastError = ERROR_SUCCESS;
    HKEY hKey = NULL;

    if (bInstall)
    {

        if (!dwLastError)
        {
            dwLastError = RegCreateKeyExW(
                HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" _T(EP_CLSID) L"_" _T(PRODUCT_NAME),
                0,
                NULL,
                REG_OPTION_NON_VOLATILE,
                KEY_WRITE | KEY_WOW64_64KEY,
                NULL,
                &hKey,
                NULL
            );
            if (hKey == NULL || hKey == INVALID_HANDLE_VALUE)
            {
                hKey = NULL;
            }
            if (hKey)
            {
                if (!dwLastError)
                {
                    dwLastError = RegSetValueExW(
                        hKey,
                        L"UninstallString",
                        0,
                        REG_SZ,
                        (const BYTE*)wszPath,
                        (DWORD)(wcslen(wszPath) + 1) * sizeof(wchar_t)
                    );
                }
                if (!dwLastError)
                {
                    dwLastError = RegSetValueExW(
                        hKey,
                        L"DisplayName",
                        0,
                        REG_SZ,
                        (const BYTE*)_T(PRODUCT_DISPLAY_NAME),
                        (DWORD)(wcslen(_T(PRODUCT_DISPLAY_NAME)) + 1) * sizeof(wchar_t)
                    );
                }
                if (!dwLastError)
                {
                    dwLastError = RegSetValueExW(
                        hKey,
                        L"Publisher",
                        0,
                        REG_SZ,
                        (const BYTE*)_T(PRODUCT_PUBLISHER),
                        (DWORD)(wcslen(_T(PRODUCT_PUBLISHER)) + 1) * sizeof(wchar_t)
                    );
                }
                if (!dwLastError)
                {
                    DWORD dw1 = TRUE;
                    dwLastError = RegSetValueExW(
                        hKey,
                        L"NoModify",
                        0,
                        REG_DWORD,
                        (const BYTE*)&dw1,
                        sizeof(DWORD)
                    );
                }
                if (!dwLastError)
                {
                    DWORD dw1 = TRUE;
                    dwLastError = RegSetValueExW(
                        hKey,
                        L"NoRepair",
                        0,
                        REG_DWORD,
                        (const BYTE*)&dw1,
                        sizeof(DWORD)
                    );
                }
                if (!dwLastError)
                {
                    PathRemoveFileSpecW(wszPath + 1);
#if defined(_M_X64)
                    wcscat_s(wszPath + 1, MAX_PATH - 2, L"\\" _T(PRODUCT_NAME) L".amd64.dll");
#elif defined(_M_ARM64)
                    wcscat_s(wszPath + 1, MAX_PATH - 2, L"\\" _T(PRODUCT_NAME) L".arm64.dll");
#endif
                    HMODULE hEP = LoadLibraryExW(wszPath + 1, NULL, LOAD_LIBRARY_AS_DATAFILE);
                    if (hEP)
                    {
                        DWORD dwLeftMost = 0;
                        DWORD dwSecondLeft = 0;
                        DWORD dwSecondRight = 0;
                        DWORD dwRightMost = 0;

                        QueryVersionInfo(hEP, VS_VERSION_INFO, &dwLeftMost, &dwSecondLeft, &dwSecondRight, &dwRightMost);

                        WCHAR wszBuf[30];
                        swprintf_s(wszBuf, 30, L"%d.%d.%d.%d", dwLeftMost, dwSecondLeft, dwSecondRight, dwRightMost);

                        if (!dwLastError)
                        {
                            dwLastError = RegSetValueExW(
                                hKey,
                                L"DisplayVersion",
                                0,
                                REG_SZ,
                                (const BYTE*)wszBuf,
                                (DWORD)(wcslen(wszBuf) + 1) * sizeof(wchar_t)
                            );
                            if (!dwLastError)
                            {
                                dwLastError = RegSetValueExW(
                                    hKey,
                                    L"VersionMajor",
                                    0,
                                    REG_DWORD,
                                    (const BYTE*)&dwSecondRight,
                                    sizeof(DWORD)
                                );
                                if (!dwLastError)
                                {
                                    dwLastError = RegSetValueExW(
                                        hKey,
                                        L"VersionMinor",
                                        0,
                                        REG_DWORD,
                                        (const BYTE*)&dwRightMost,
                                        sizeof(DWORD)
                                    );
                                }
                            }
                        }

                        FreeLibrary(hEP);
                    }
                }
                if (!dwLastError)
                {
                    GetWindowsDirectoryW(wszPath, MAX_PATH);
                    wcscat_s(wszPath, MAX_PATH, L"\\explorer.exe");
                    dwLastError = RegSetValueExW(
                        hKey,
                        L"DisplayIcon",
                        0,
                        REG_SZ,
                        (const BYTE*)wszPath,
                        (DWORD)((wcslen(wszPath) + 1) * sizeof(wchar_t))
                    );
                }
                RegCloseKey(hKey);
            }
        }
    }
    else
    {
        if (!dwLastError)
        {
            dwLastError = RegOpenKeyW(
                HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" _T(EP_CLSID) L"_" _T(PRODUCT_NAME),
                &hKey
            );
            if (hKey == NULL || hKey == INVALID_HANDLE_VALUE)
            {
                hKey = NULL;
            }
            if (hKey)
            {
                dwLastError = RegDeleteTreeW(hKey, NULL);
                RegCloseKey(hKey);
            }
        }
     }
     if (bInstall && !dwLastError)
     {
         // GR Studios AI LLC product and support links.
         PCWSTR currentEntry = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" _T(EP_CLSID) L"_" _T(PRODUCT_NAME);
         RegSetKeyValueW(HKEY_LOCAL_MACHINE, currentEntry, L"URLInfoAbout", REG_SZ,
             _T(PRODUCT_WEBSITE), sizeof(_T(PRODUCT_WEBSITE)));
         RegSetKeyValueW(HKEY_LOCAL_MACHINE, currentEntry, L"HelpLink", REG_SZ,
             _T(PRODUCT_WEBSITE), sizeof(_T(PRODUCT_WEBSITE)));
     }
     if (dwLastError) return GRFail(L"update uninstall registration", wszPath, dwLastError);
     return TRUE;
}

typedef struct
{
    PBYTE base;
    ZPOS64_T size;
    ZPOS64_T curOffset;
} MemoryBuffer;

void MemoryBuffer_Destroy(MemoryBuffer** mem)
{
    if (*mem)
    {
        if ((*mem)->base)
            free((*mem)->base);
        free(*mem);
        *mem = NULL;
    }
}

voidpf ZCALLBACK MemOpenFile(voidpf opaque, const void* filename, int mode)
{
    MemoryBuffer* pMem = (MemoryBuffer*)opaque;
    return pMem;
}

uLong ZCALLBACK MemReadFile(voidpf opaque, voidpf stream, void* buf, uLong size)
{
    MemoryBuffer* pMem = (MemoryBuffer*)stream;
    uLong toRead = size;

    if (pMem->curOffset + toRead > pMem->size)
    {
        toRead = (uLong)(pMem->size - pMem->curOffset);
    }

    if (toRead > 0)
    {
        memcpy(buf, pMem->base + pMem->curOffset, toRead);
        pMem->curOffset += toRead;
    }

    return toRead;
}

uLong ZCALLBACK MemWriteFile(voidpf opaque, voidpf stream, const void* buf, uLong size)
{
    return 0;
}

ZPOS64_T ZCALLBACK MemTellFile(voidpf opaque, voidpf stream)
{
    MemoryBuffer* pMem = (MemoryBuffer*)stream;
    return pMem->curOffset;
}

long ZCALLBACK MemSeekFile(voidpf opaque, voidpf stream, ZPOS64_T offset, int origin)
{
    MemoryBuffer* pMem = (MemoryBuffer*)stream;
    ZPOS64_T newOffset;

    switch (origin)
    {
        case ZLIB_FILEFUNC_SEEK_CUR:
            newOffset = pMem->curOffset + offset;
            break;
        case ZLIB_FILEFUNC_SEEK_END:
            newOffset = pMem->size + offset;
            break;
        case ZLIB_FILEFUNC_SEEK_SET:
            newOffset = offset;
            break;
        default:
            return -1;
    }

    if (newOffset > pMem->size)
    {
        return -1;
    }

    pMem->curOffset = newOffset;
    return 0;
}

int ZCALLBACK MemCloseFile(voidpf opaque, voidpf stream)
{
    return 0;
}

int ZCALLBACK MemErrorFile(voidpf opaque, voidpf stream)
{
    return 0;
}

void FillMemoryFileIOFunctions(zlib_filefunc64_def* pFileFunc, MemoryBuffer* pMem)
{
    pFileFunc->zopen64_file = MemOpenFile;
    pFileFunc->zread_file = MemReadFile;
    pFileFunc->zwrite_file = MemWriteFile;
    pFileFunc->ztell64_file = MemTellFile;
    pFileFunc->zseek64_file = MemSeekFile;
    pFileFunc->zclose_file = MemCloseFile;
    pFileFunc->zerror_file = MemErrorFile;
    pFileFunc->opaque = pMem;
}

#define AES_KEYBITS				256

#define KEYLENGTH( keybits )		( ( keybits ) / 8 )
#define RKLENGTH( keybits )		( ( keybits ) / 8 + 28 )
#define NROUNDS( keybits )			( ( keybits ) / 32 + 6 )

#if defined(WITH_ENCRYPTION) && !defined(ZIP_ENCRYPTION_KEY)
#define ZIP_ENCRYPTION_KEY 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
#endif

unzFile LoadZipFileFromResources(MemoryBuffer** outMem)
{
    *outMem = NULL;

    HRSRC hRsrc = FindResourceW(NULL, MAKEINTRESOURCE(IDR_EP_ZIP), RT_RCDATA);
    if (!hRsrc)
    {
        return NULL;
    }

    HGLOBAL hGlobal = LoadResource(NULL, hRsrc);
    if (!hGlobal)
    {
        return NULL;
    }

    PBYTE pRsrc = (PBYTE)LockResource(hGlobal);
    DWORD cbRsrc = SizeofResource(NULL, hRsrc);
    if (!pRsrc || !cbRsrc)
    {
        return NULL;
    }

#ifdef WITH_ENCRYPTION
    if ((cbRsrc % 16) != 0)
    {
        return NULL;
    }
#endif

    MemoryBuffer* pMem = (MemoryBuffer*)malloc(sizeof(MemoryBuffer));
    if (!pMem)
    {
        return NULL;
    }

    pMem->base = (PBYTE)malloc(cbRsrc);
    pMem->size = cbRsrc;
    pMem->curOffset = 0;
    if (!pMem->base)
    {
        free(pMem);
        return NULL;
    }

    *outMem = pMem;

#ifdef WITH_ENCRYPTION
    BYTE keyBytes[32] = { ZIP_ENCRYPTION_KEY };

    UINT rk[RKLENGTH(AES_KEYBITS)] = { 0 };
    int nrounds = rijndaelKeySetupDec(rk, keyBytes, AES_KEYBITS);

    // Decrypt the data a block at a time
    for (UINT offset = 0; offset < cbRsrc; offset += 16)
    {
        rijndaelDecrypt(rk, nrounds, pRsrc + offset, pMem->base + offset);
    }
#else
    memcpy(pMem->base, pRsrc, cbRsrc);
#endif

    zlib_filefunc64_def fileFunc = { 0 };
    FillMemoryFileIOFunctions(&fileFunc, pMem);

    return unzOpen2_64(NULL, &fileFunc);
}

// No live file is deleted until its replacement is complete and recoverable.
__declspec(noinline) BOOL InstallResourceHelper(BOOL bInstall, HMODULE hModule, unzFile zipFile, const WCHAR* wszPath)
{
    if (!GRValidateTarget(wszPath)) return FALSE;
    if (!bInstall) return GRPublishFile(wszPath, NULL, TRUE);
    WCHAR staged[MAX_PATH] = L"";
    if (!GRUniqueSibling(wszPath, staged)) return GRFail(L"create staging file", wszPath, GetLastError());
    BOOL ok = FALSE;
    if (!zipFile) {
        WCHAR source[MAX_PATH];
        if (!GetModuleFileNameW(hModule, source, MAX_PATH)) GRFail(L"GetModuleFileNameW", wszPath, GetLastError());
        else if (!_wcsicmp(source, wszPath)) { DeleteFileW(staged); return TRUE; }
        else if (!CopyFileW(source, staged, FALSE)) GRFail(L"copy installer to staging", source, GetLastError());
        else {
            HANDLE copy = CreateFileW(staged, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (copy == INVALID_HANDLE_VALUE) GRFail(L"open staged installer", staged, GetLastError());
            else {
                ok = FlushFileBuffers(copy);
                if (!ok) GRFail(L"flush staged installer", staged, GetLastError());
                CloseHandle(copy);
            }
        }
    } else {
        unz_file_info64 info = { 0 };
        if (unzGetCurrentFileInfo64(zipFile, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK || info.uncompressed_size > MAXDWORD)
            GRFail(L"read archive entry metadata", wszPath, ERROR_INVALID_DATA);
        else {
            HANDLE file = CreateFileW(staged, GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (file == INVALID_HANDLE_VALUE) GRFail(L"open staged file", staged, GetLastError());
            else {
                BYTE buffer[65536]; ULONGLONG total = 0; int count; ok = TRUE;
                while ((count = unzReadCurrentFile(zipFile, buffer, sizeof(buffer))) > 0) {
                    DWORD written = 0;
                    if (!WriteFile(file, buffer, (DWORD)count, &written, NULL) || written != (DWORD)count) {
                        GRFail(L"write staged file", staged, GetLastError()); ok = FALSE; break;
                    }
                    total += (DWORD)count;
                }
                if (ok && (count < 0 || total != info.uncompressed_size)) ok = GRFail(L"decompress archive entry", wszPath, ERROR_INVALID_DATA);
                if (ok && !FlushFileBuffers(file)) ok = GRFail(L"flush staged file", staged, GetLastError());
                CloseHandle(file);
            }
        }
    }
    if (ok) ok = GRPublishFile(wszPath, staged, FALSE);
    if (!ok || GetFileAttributesW(staged) != INVALID_FILE_ATTRIBUTES) DeleteFileW(staged);
    return ok;
}
__declspec(noinline) BOOL InstallResource(BOOL bInstall, HMODULE hInstance, unzFile zipFile, const char* pszFileNameInZip, LPCWSTR pwszDirectory, LPCWSTR pwszFileName)
{
    if (bInstall && zipFile && pszFileNameInZip)
    {
        int resultLocateFile = unzLocateFile(zipFile, pszFileNameInZip, 0);
        if (resultLocateFile != UNZ_OK)
        {
            return resultLocateFile == UNZ_END_OF_LIST_OF_FILE; // Don't touch this file, we don't pack this file in the setup
        }

        if (unzOpenCurrentFile(zipFile) != UNZ_OK)
        {
            return FALSE;
        }
    }

    WCHAR wszPath[MAX_PATH];
    wcscpy_s(wszPath, MAX_PATH, pwszDirectory);
    wcscat_s(wszPath, MAX_PATH, L"\\");
    wcscat_s(wszPath, MAX_PATH, pwszFileName);
    BOOL bRet = InstallResourceHelper(bInstall, hInstance, zipFile, wszPath);
    if (bInstall && zipFile && pszFileNameInZip)
        if (unzCloseCurrentFile(zipFile) != UNZ_OK) bRet = GRFail(L"archive CRC verification", wszPath, ERROR_CRC);
    return bRet;
}

const WCHAR* GetSystemLanguages()
{
    wchar_t* wszLanguagesBuffer = NULL;
    ULONG ulNumLanguages = 0;
    ULONG cchLanguagesBuffer = 0;
    if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &ulNumLanguages, NULL, &cchLanguagesBuffer))
    {
        wszLanguagesBuffer = (wchar_t*)malloc(cchLanguagesBuffer * sizeof(wchar_t));
        if (wszLanguagesBuffer)
        {
            if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &ulNumLanguages, wszLanguagesBuffer, &cchLanguagesBuffer))
            {
                free(wszLanguagesBuffer);
                wszLanguagesBuffer = NULL;
            }
        }
    }
    return wszLanguagesBuffer ? wszLanguagesBuffer : L"en-US";
}

BOOL SystemHasLanguageInstalled(const WCHAR* languages, const char* langCode, int cchLangCode)
{
    WCHAR szLangCode[100];
    MultiByteToWideChar(CP_UTF8, 0, langCode, cchLangCode, szLangCode, ARRAYSIZE(szLangCode));
    szLangCode[cchLangCode] = 0;
    for (const WCHAR* wszLang = languages; *wszLang; wszLang += wcslen(wszLang) + 1)
    {
        if (!_wcsicmp(wszLang, szLangCode))
        {
            return TRUE;
        }
    }
    return FALSE;
}

typedef enum LanguageCodeTreatment
{
    LCT_None,
    LCT_MUI, // module\en-US\module.dll.pri
    LCT_PRI, // resource\pris\resource.en-US.pri
} LanguageCodeTreatment;

// Service only artwork entries owned by the embedded payload.
static BOOL GRServiceAssets(unzFile zip, BOOL install, const char* prefix)
{
    if (!zip) return GRFail(L"load orb payload", grInstallRoot, ERROR_INVALID_DATA);
    int result = unzGoToFirstFile(zip);
    for (; result == UNZ_OK; result = unzGoToNextFile(zip)) {
        char name[1024]; unz_file_info64 info = { 0 };
        if (unzGetCurrentFileInfo64(zip, &info, name, sizeof(name), NULL, 0, NULL, 0) != UNZ_OK || info.size_filename >= sizeof(name))
            return GRFail(L"read orb archive entry", grInstallRoot, ERROR_INVALID_DATA);
        name[info.size_filename] = 0;
        if (strncmp(name, prefix, strlen(prefix)) || !info.uncompressed_size) continue;
        WCHAR relative[MAX_PATH], path[MAX_PATH];
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, relative, MAX_PATH))
            return GRFail(L"decode orb path", grInstallRoot, GetLastError());
        for (WCHAR* c = relative; *c; ++c) if (*c == L'/') *c = L'\\';
        PCWSTR ext = PathFindExtensionW(relative);
        if (!strcmp(prefix,"Cursors/")) {
            if (_wcsicmp(ext,L".cur") && _wcsicmp(ext,L".ani") && _wcsicmp(ext,L".grcursors")) continue;
        } else if (_wcsicmp(ext,L".bmp") && _wcsicmp(ext,L".png") && _wcsicmp(ext,L".ico") &&
            _wcsicmp(ext,L".jpg") && _wcsicmp(ext,L".jpeg")) continue;
        if (swprintf_s(path, MAX_PATH, L"%s\\%s", grInstallRoot, relative) < 0 || !GRValidateTarget(path))
            return GRFail(L"validate orb path", grInstallRoot, ERROR_INVALID_NAME);
        if (!install) { if (!GRPublishFile(path, NULL, TRUE)) return FALSE; continue; }
        WCHAR parent[MAX_PATH]; wcscpy_s(parent, MAX_PATH, path); PathRemoveFileSpecW(parent);
        int created = SHCreateDirectoryExW(NULL, parent, NULL);
        if (created != ERROR_SUCCESS && created != ERROR_ALREADY_EXISTS && created != ERROR_FILE_EXISTS)
            return GRFail(L"create orb directory", parent, created);
        if (unzOpenCurrentFile(zip) != UNZ_OK) return GRFail(L"open orb archive entry", path, ERROR_INVALID_DATA);
        BOOL ok = InstallResourceHelper(TRUE, NULL, zip, path);
        if (unzCloseCurrentFile(zip) != UNZ_OK) return GRFail(L"verify orb CRC", path, ERROR_CRC);
        if (!ok) return FALSE;
    }
    return result == UNZ_END_OF_LIST_OF_FILE || GRFail(L"enumerate orb archive", grInstallRoot, ERROR_INVALID_DATA);
}

__declspec(noinline) BOOL ExtractDirectory(unzFile zipFile, const char* dirNameInZip, LPCWSTR pwszDirectory, const WCHAR* languages, LanguageCodeTreatment langCodeTreatment)
{
    if (!zipFile)
    {
        return FALSE;
    }

    if (unzGoToFirstFile(zipFile) != UNZ_OK)
    {
        return FALSE;
    }

    BOOL bRet = TRUE;
    size_t dirNameLen = dirNameInZip ? strlen(dirNameInZip) : 0;

    do
    {
        char szFileNameInZip[260];
        unz_file_info64 fileInfo = { 0 };
        if (unzGetCurrentFileInfo64(zipFile, &fileInfo, szFileNameInZip, ARRAYSIZE(szFileNameInZip), NULL, 0, NULL, 0) != UNZ_OK)
        {
            return FALSE;
        }
        szFileNameInZip[fileInfo.size_filename] = 0;

        if (fileInfo.uncompressed_size == 0 || (fileInfo.external_fa & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            continue;
        }

        if (dirNameInZip && strncmp(szFileNameInZip, dirNameInZip, dirNameLen) != 0)
        {
            continue;
        }

        // Examples:
        // - "module/en-US/module.dll.mui" -> "en-US/module.dll.mui"
        // - "resource/pris/resource.en-US.pri" -> "pris/resource.en-US.pri"
        const char* filePathInDir = szFileNameInZip + dirNameLen;
        const char* lastSlash = strrchr(filePathInDir, '/');
        const char* fileName = lastSlash ? filePathInDir + (lastSlash - filePathInDir) + 1 : filePathInDir;
        const char* lastDot = strrchr(fileName, '.');
        const char* fileExt = lastDot ? fileName + (lastDot - fileName) + 1 : NULL;
        if (langCodeTreatment == LCT_MUI)
        {
            if (fileExt && !_stricmp(fileExt, "mui"))
            {
                if (!SystemHasLanguageInstalled(languages, filePathInDir, (int)(strchr(filePathInDir, '/') - filePathInDir)))
                {
                    continue;
                }
            }
        }
        else if (langCodeTreatment == LCT_PRI)
        {
            if (fileExt && !_stricmp(fileExt, "pri") && strchr(fileName, '-') != NULL)
            {
                // Check if we're a language pri
                const char* secondLastDot = NULL;
                for (const char* p = lastDot - 1; p >= fileName; p--)
                {
                    if (*p == '.')
                    {
                        secondLastDot = p;
                        break;
                    }
                }
                if (secondLastDot != lastDot)
                {
                    const char* langCode = secondLastDot + 1;
                    if (!SystemHasLanguageInstalled(languages, langCode, (int)(lastDot - langCode)))
                    {
                        continue;
                    }
                }
            }
        }

        if (unzOpenCurrentFile(zipFile) != UNZ_OK)
        {
            return FALSE;
        }

        WCHAR wszFileNameInZip[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, szFileNameInZip, -1, wszFileNameInZip, MAX_PATH);
        for (size_t i = 0; i < MAX_PATH && wszFileNameInZip[i] != 0; i++)
        {
            if (wszFileNameInZip[i] == '/')
            {
                wszFileNameInZip[i] = '\\';
            }
        }

        WCHAR wszPath[MAX_PATH];
        wcscpy_s(wszPath, MAX_PATH, pwszDirectory);
        wcscat_s(wszPath, MAX_PATH, L"\\");
        WCHAR* pwszPathInDir = wszPath + wcslen(wszPath);
        if (dirNameInZip)
        {
            wcscat_s(wszPath, MAX_PATH, wcschr(wszFileNameInZip, '\\') + 1); // Skip the directory name in the zip file
        }
        else
        {
            wcscat_s(wszPath, MAX_PATH, wszFileNameInZip);
        }

        for (WCHAR* p = pwszPathInDir; *p; p++)
        {
            if (*p == '\\')
            {
                *p = 0;
                CreateDirectoryW(wszPath, NULL);
                *p = '\\';
            }
        }

        bRet = InstallResourceHelper(TRUE, NULL, zipFile, wszPath);

        if (unzCloseCurrentFile(zipFile) != UNZ_OK) bRet = GRFail(L"archive CRC verification", wszPath, ERROR_CRC);
    } while (bRet && unzGoToNextFile(zipFile) == UNZ_OK);

    return bRet;
}

BOOL DeleteResource(LPCWSTR pwszDirectory, LPCWSTR pwszFileName)
{
    WCHAR wszPath[MAX_PATH];
    wcscpy_s(wszPath, MAX_PATH, pwszDirectory);
    wcscat_s(wszPath, MAX_PATH, L"\\");
    wcscat_s(wszPath, MAX_PATH, pwszFileName);
    return InstallResourceHelper(FALSE, NULL, NULL, wszPath);
}

/*BOOL ShouldDownloadOrDelete(BOOL bInstall, WCHAR* wszPath, LPCSTR chash)
{
    if (FileExistsW(wszPath))
    {
        if (bInstall)
        {
            char hash[100];
            ZeroMemory(hash, sizeof(char) * 100);
            ComputeFileHash(wszPath, hash, 100);
            bInstall = _stricmp(hash, chash) != 0;
        }
        else
        {
            InstallResourceHelper(FALSE, NULL, NULL, wszPath); // Delete
        }
    }

    return bInstall;
}

BOOL DownloadResource(BOOL bInstall, LPCWSTR pwszURL, DWORD dwSize, LPCSTR chash, LPCWSTR pwszDirectory, LPCWSTR pwszFileName)
{
    BOOL bOk = TRUE;
    WCHAR wszPath[MAX_PATH];
    wcscpy_s(wszPath, MAX_PATH, pwszDirectory);
    wcscat_s(wszPath, MAX_PATH, L"\\");
    wcscat_s(wszPath, MAX_PATH, pwszFileName);
    if (ShouldDownloadOrDelete(bInstall, wszPath, chash) && IsConnectedToInternet() == TRUE)
    {
        bOk = DownloadFile(pwszURL, dwSize, wszPath);
    }
    return bOk;
}*/

void ProcessTaskbarDlls(BOOL* bInOutOk, BOOL bInstall, BOOL bExtractMode, HINSTANCE hInstance, unzFile zipFile, WCHAR wszPath[260])
{
    // Delete ep_taskbars with old naming scheme
    DeleteResource(wszPath, L"ep_taskbar.0.dll");
    DeleteResource(wszPath, L"ep_taskbar.1.dll");
    DeleteResource(wszPath, L"ep_taskbar.2.dll");
    DeleteResource(wszPath, L"ep_taskbar.3.dll");
    DeleteResource(wszPath, L"ep_taskbar.4.dll");
    DeleteResource(wszPath, L"ep_taskbar.5.dll");

    LPCWSTR pwszTaskbarDllName = bExtractMode ? NULL : PickTaskbarDll();
    if (*bInOutOk) *bInOutOk = InstallResource(bInstall && (bExtractMode || pwszTaskbarDllName && !wcscmp(pwszTaskbarDllName, L"ep_taskbar.rs2.dll")), hInstance, zipFile, "ep_taskbar.rs2.dll", wszPath, L"ep_taskbar.rs2.dll");
    if (*bInOutOk) *bInOutOk = InstallResource(bInstall && (bExtractMode || pwszTaskbarDllName && !wcscmp(pwszTaskbarDllName, L"ep_taskbar.fe.dll")), hInstance, zipFile, "ep_taskbar.fe.dll", wszPath, L"ep_taskbar.fe.dll");
    if (*bInOutOk) *bInOutOk = InstallResource(bInstall && (bExtractMode || pwszTaskbarDllName && !wcscmp(pwszTaskbarDllName, L"ep_taskbar.co.dll")), hInstance, zipFile, "ep_taskbar.co.dll", wszPath, L"ep_taskbar.co.dll");
    if (*bInOutOk) *bInOutOk = InstallResource(bInstall && (bExtractMode || pwszTaskbarDllName && !wcscmp(pwszTaskbarDllName, L"ep_taskbar.ni.dll")), hInstance, zipFile, "ep_taskbar.ni.dll", wszPath, L"ep_taskbar.ni.dll");
    if (*bInOutOk) *bInOutOk = InstallResource(bInstall && (bExtractMode || pwszTaskbarDllName && !wcscmp(pwszTaskbarDllName, L"ep_taskbar.zn.dll")), hInstance, zipFile, "ep_taskbar.zn.dll", wszPath, L"ep_taskbar.zn.dll");
    if (*bInOutOk) *bInOutOk = InstallResource(bInstall && (bExtractMode || pwszTaskbarDllName && !wcscmp(pwszTaskbarDllName, L"ep_taskbar.ge.dll")), hInstance, zipFile, "ep_taskbar.ge.dll", wszPath, L"ep_taskbar.ge.dll");
}

BOOL RemoveDirectoryRecursive(const WCHAR* directory)
{
    WCHAR pattern[MAX_PATH]; swprintf_s(pattern, MAX_PATH, L"%s\\*", directory);
    WIN32_FIND_DATAW data; HANDLE search = FindFirstFileW(pattern, &data);
    if (search == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND;
    BOOL ok = TRUE;
    do {
        if (!wcscmp(data.cFileName, L".") || !wcscmp(data.cFileName, L"..")) continue;
        WCHAR path[MAX_PATH]; swprintf_s(path, MAX_PATH, L"%s\\%s", directory, data.cFileName);
        BOOL backup = FALSE;
        for (GRFileChange* change = grChanges; change; change = change->next)
            if (!_wcsicmp(change->backup, path)) { backup = TRUE; break; }
        if (backup) continue;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { ok = GRFail(L"refuse recursive reparse traversal", path, ERROR_ACCESS_DENIED); break; }
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ok = RemoveDirectoryRecursive(path);
        else ok = GRPublishFile(path, NULL, TRUE);
        if (!ok) break;
    } while (FindNextFileW(search, &data));
    DWORD error = GetLastError(); FindClose(search);
    if (ok && error != ERROR_NO_MORE_FILES) ok = GRFail(L"enumerate obsolete payload directory", directory, error);
    // Keep directories until backups have been committed; empty directories are harmless.
    return ok;
}
int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd
)
{
    GRInitializeInstallerBranding(hInstance);
    global_ubr = VnGetOSVersionAndUBR(&global_rovi);

    BOOL bResume = FALSE;
    BOOL bOk = TRUE, bInstall = TRUE, bWasShellExt = FALSE, bIsUpdate = FALSE, bForcePromptForUninstall = FALSE;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    int argc = 0;
    LPWSTR* wargv = CommandLineToArgvW(
        lpCmdLine,
        &argc
    );

    WCHAR wszPath[MAX_PATH];
    ZeroMemory(wszPath, MAX_PATH * sizeof(WCHAR));

    if (argc >= 1 && !_wcsicmp(wargv[0], L"/extract"))
    {
        if (argc >= 2)
        {
            wcsncpy_s(wszPath, MAX_PATH, wargv[1], MAX_PATH);
            CreateDirectoryW(wargv[1], NULL);
        }
        else
        {
            GetCurrentDirectoryW(MAX_PATH, wszPath);
        }
        wcscpy_s(grInstallRoot, MAX_PATH, wszPath);
        MemoryBuffer* pMem = NULL;
        unzFile zipFile = LoadZipFileFromResources(&pMem);
        bOk = zipFile != NULL;
        if (bOk)
        {
            bOk = ExtractDirectory(zipFile, NULL, wszPath, NULL, LCT_None);
        }
        if (zipFile)
            unzClose(zipFile);
        if (pMem)
            MemoryBuffer_Destroy(&pMem);
        if (bOk) bOk = GRCommitFiles();
        if (!bOk) GRRollbackFiles();
        GRFreeChanges();
        return !bOk;
    }

#if defined(_M_X64)
    typedef BOOL (WINAPI *IsWow64Process2_t)(HANDLE hProcess, USHORT* pProcessMachine, USHORT* pNativeMachine);
    IsWow64Process2_t pfnIsWow64Process2 = (IsWow64Process2_t)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "IsWow64Process2");
    if (pfnIsWow64Process2)
    {
        USHORT processMachine, nativeMachine;
        if (pfnIsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine))
        {
            if (nativeMachine == IMAGE_FILE_MACHINE_ARM64)
            {
                WCHAR szFormat[256];
                szFormat[0] = 0;
                int written = LoadStringW(hInstance, IDS_SETUP_UNSUPPORTED_ARCH, szFormat, ARRAYSIZE(szFormat));
                if (written > 0 && written < ARRAYSIZE(szFormat))
                {
                    WCHAR szMessage[256];
                    szMessage[0] = 0;
                    _swprintf_p(szMessage, ARRAYSIZE(szMessage), szFormat, L"ARM64", L"x64");
                    GRInstallerMessageBox(NULL, szMessage, _T(PRODUCT_DISPLAY_NAME), MB_OK | MB_ICONERROR);
                }
                exit(0);
            }
        }
    }
#endif

    WCHAR wszOwnPath[MAX_PATH];
    ZeroMemory(wszOwnPath, ARRAYSIZE(wszOwnPath));
    if (!GetModuleFileNameW(NULL, wszOwnPath, ARRAYSIZE(wszOwnPath)))
    {
        exit(0);
    }

    bInstall = !(argc >= 1 && (!_wcsicmp(wargv[0], L"/uninstall") || !_wcsicmp(wargv[0], L"/uninstall_silent")));
    bResume = argc >= 1 && (!_wcsicmp(wargv[0], L"/resume_install") || !_wcsicmp(wargv[0], L"/resume_uninstall"));
    if (bResume) bInstall = _wcsicmp(wargv[0], L"/resume_uninstall") != 0;
    PathStripPathW(wszOwnPath);
    if (!_wcsicmp(wszOwnPath, L"ep_uninstall.exe"))
    {
        bInstall = FALSE;
        bForcePromptForUninstall = _wcsicmp(wargv[0], L"/uninstall_silent");
    }
    if (!GetModuleFileNameW(NULL, wszOwnPath, ARRAYSIZE(wszOwnPath)))
    {
        exit(0);
    }
    bIsUpdate = (argc >= 1 && !_wcsicmp(wargv[0], L"/update_silent"));
    if (!bInstall && (!_wcsicmp(wargv[0], L"/uninstall") || bForcePromptForUninstall))
    {
        wchar_t mbText[256];
        mbText[0] = 0;
        LoadStringW(hInstance, IDS_SETUP_UNINSTALL_PROMPT, mbText, ARRAYSIZE(mbText));
        if (GRInstallerMessageBox(NULL, mbText, _T(PRODUCT_DISPLAY_NAME), MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDNO)
        {
            exit(0);
        }
    }

    if (!IsAppRunningAsAdminMode())
    {
        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(SHELLEXECUTEINFOW));
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"runas";
        sei.lpFile = wszOwnPath;
        sei.lpParameters = bResume ? lpCmdLine : !bInstall ? L"/uninstall_silent" : lpCmdLine;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;
        if (!ShellExecuteExW(&sei))
        {
            DWORD dwError = GetLastError();
            if (dwError == ERROR_CANCELLED)
            {
            }
        }
        exit(0);
    }

    HANDLE servicingMutex = CreateMutexW(NULL, TRUE, L"Global\\GRTaskbar-Servicing");
    if (!servicingMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        GRInstallerMessageBox(NULL, L"Another GR Taskbar installer is running. Wait for it to finish, then try again.",
            _T(PRODUCT_DISPLAY_NAME), MB_OK | MB_ICONINFORMATION);
        if (servicingMutex) CloseHandle(servicingMutex);
        return ERROR_INSTALL_ALREADY_RUNNING;
    }
    GRInitLog();
    MemoryBuffer* pMem = NULL;
    unzFile zipFile = NULL;
    {
        zipFile = LoadZipFileFromResources(&pMem);
        if (!zipFile)
        {
            GRFail(L"load embedded payload archive", wszOwnPath, ERROR_INVALID_DATA);
            if (grLog != INVALID_HANDLE_VALUE) CloseHandle(grLog);
            ReleaseMutex(servicingMutex); CloseHandle(servicingMutex);
            return ERROR_INVALID_DATA;
        }
    }

    DWORD bIsUndockingDisabled = FALSE, dwSize = sizeof(DWORD);
    RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell\\Update\\Packages", L"UndockingDisabled", RRF_RT_DWORD, NULL, &bIsUndockingDisabled, &dwSize);
    if (bIsUndockingDisabled)
    {
        wchar_t mbText[256];
        mbText[0] = 0;
        LoadStringW(hInstance, bInstall ? IDS_SETUP_INSTALL_LOGOFF : IDS_SETUP_UNINSTALL_LOGOFF, mbText, ARRAYSIZE(mbText));
        if (GRInstallerMessageBox(NULL, mbText, _T(PRODUCT_DISPLAY_NAME), MB_YESNO | MB_DEFBUTTON1 | MB_ICONQUESTION) == IDYES)
        {
            RegDeleteKeyValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell\\Update\\Packages", L"UndockingDisabled");
        }
        else
        {
            exit(0);
        }
    }

    CreateEventW(NULL, FALSE, FALSE, _T(EP_SETUP_EVENTNAME));

    SHGetFolderPathW(NULL, SPECIAL_FOLDER, NULL, SHGFP_TYPE_CURRENT, wszPath);
    wcscat_s(wszPath, MAX_PATH, _T(APP_RELATIVE_PATH));
    bOk = CreateDirectoryW(wszPath, NULL);
    if (!bOk && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        bOk = TRUE;
    }
    if (!bOk) GRFail(L"create installation directory", wszPath, GetLastError());
    if (bOk)
    {
        wcscpy_s(grInstallRoot, MAX_PATH, wszPath);
        WCHAR journalProbe[MAX_PATH];
        swprintf_s(journalProbe, MAX_PATH, L"%s\\servicing.rollback", grInstallRoot);
        bOk = GRValidateTarget(journalProbe);
        HANDLE userToken = INVALID_HANDLE_VALUE;

        HWND hShellTrayWnd = FindWindowW(L"Shell_TrayWnd", NULL);
        if (hShellTrayWnd)
        {
            DWORD explorerProcessId = 0;
            GetWindowThreadProcessId(hShellTrayWnd, &explorerProcessId);
            if (explorerProcessId != 0)
            {
                HANDLE explorerProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, explorerProcessId);
                if (explorerProcess != NULL)
                {
                    if (!OpenProcessToken(explorerProcess, TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY, &userToken)) {
                        DWORD tokenError = GetLastError();
                        if (bOk) bOk = GRFail(L"OpenProcessToken: interactive shell", L"explorer.exe", tokenError);
                    }
                    CloseHandle(explorerProcess);
                } else if (bOk) bOk = GRFail(L"OpenProcess: interactive shell", L"explorer.exe", GetLastError());

            }
        }
        if (bOk) GRCloseSettings();
        if (bOk && hShellTrayWnd && (!userToken || userToken == INVALID_HANDLE_VALUE))
            bOk = GRFail(L"capture interactive shell token", L"explorer.exe", ERROR_ACCESS_DENIED);
        if (bOk && GetFileAttributesW(journalProbe) != INVALID_FILE_ATTRIBUTES)
            bOk = GRStopExplorer(hShellTrayWnd);
        BOOL bAreRoundedCornersDisabled = FALSE;
        HANDLE h_exists = CreateEventW(NULL, FALSE, FALSE, _T(EP_DWM_EVENTNAME));
        if (h_exists)
        {
            bAreRoundedCornersDisabled = GetLastError() == ERROR_ALREADY_EXISTS;
            CloseHandle(h_exists);
        }
        else
        {
            bAreRoundedCornersDisabled = GetLastError() == ERROR_ACCESS_DENIED;
        }
        // Registration changes wait until payload files have been serviced.

        WCHAR wszSCPath[MAX_PATH];
        GetSystemDirectoryW(wszSCPath, MAX_PATH);
        wcscat_s(wszSCPath, MAX_PATH, L"\\sc.exe");
        SHELLEXECUTEINFOW ShExecInfo = { 0 };
        ShExecInfo.cbSize = sizeof(ShExecInfo);
        ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
        ShExecInfo.hwnd = NULL;
        ShExecInfo.lpVerb = L"runas";
        ShExecInfo.lpFile = wszSCPath;
        ShExecInfo.lpParameters = L"stop " _T(EP_DWM_SERVICENAME);
        ShExecInfo.lpDirectory = NULL;
        ShExecInfo.nShow = SW_HIDE;
        ShExecInfo.hInstApp = NULL;
        if (bOk && ShellExecuteExW(&ShExecInfo) && ShExecInfo.hProcess)
        {
            if (WaitForSingleObject(ShExecInfo.hProcess, 30000) != WAIT_OBJECT_0) GRLog(L"service helper wait timed out", ShExecInfo.lpFile, WAIT_TIMEOUT);
            DWORD dwExitCode = 0;
            GetExitCodeProcess(ShExecInfo.hProcess, &dwExitCode);
            CloseHandle(ShExecInfo.hProcess);
        }

        // Recover only after our settings, shell and service have released their files.
        if (bOk) bOk = GRBeginFiles();
        if (bOk && grStoppedShell) bOk = GRRestartShell(userToken);
        // Artwork is serviced while the desktop is available; only binaries need a shell stop.
        if (bOk) bOk = GRServiceAssets(zipFile, bInstall, "Orbs/");
        if (bOk) bOk = GRServiceAssets(zipFile, bInstall, "Cursors/");
        if (bOk) GRCloseSettings();
        if (bOk) bOk = GRStopExplorer(FindWindowW(L"Shell_TrayWnd", NULL));

        // --------------------------------------------------------------------------------

        // C:\Program Files\ExplorerPatcher
        SHGetFolderPathW(NULL, SPECIAL_FOLDER, NULL, SHGFP_TYPE_CURRENT, wszPath);
        wcscat_s(wszPath, MAX_PATH, _T(APP_RELATIVE_PATH));
        if (CHECK_OK(bOk) && bInstall) bOk = InstallResource(bInstall, hInstance, NULL, NULL, wszPath, _T(SETUP_UTILITY_NAME));
        if (CHECK_OK(bOk))
        {
            if (!bInstall)
            {
                HKEY hKey;
                RegOpenKeyExW(
                    HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\Classes\\CLSID\\" TEXT(EP_CLSID) L"\\InProcServer32",
                    REG_OPTION_NON_VOLATILE,
                    KEY_READ,
                    &hKey
                );
                if (hKey == NULL || hKey == INVALID_HANDLE_VALUE)
                {
                    hKey = NULL;
                }
                if (hKey)
                {
                    bWasShellExt = TRUE;
                    RegCloseKey(hKey);
                }
                if (bWasShellExt)
                {
                    WCHAR wszArgs[MAX_PATH];
                    wszArgs[0] = L'/';
                    wszArgs[1] = L'u';
                    wszArgs[2] = L' ';
                    wszArgs[3] = L'"';
                    SHGetFolderPathW(NULL, SPECIAL_FOLDER, NULL, SHGFP_TYPE_CURRENT, wszArgs + 4);
#if defined(_M_X64)
                    wcscat_s(wszArgs, MAX_PATH, _T(APP_RELATIVE_PATH) L"\\" _T(PRODUCT_NAME) L".amd64.dll\"");
#elif defined(_M_ARM64)
                    wcscat_s(wszArgs, MAX_PATH, _T(APP_RELATIVE_PATH) L"\\" _T(PRODUCT_NAME) L".arm64.dll\"");
#endif
                    wprintf(L"%s\n", wszArgs);
                    WCHAR wszApp[MAX_PATH * 2];
                    GetSystemDirectoryW(wszApp, MAX_PATH * 2);
                    wcscat_s(wszApp, MAX_PATH * 2, L"\\regsvr32.exe");
                    wprintf(L"%s\n", wszApp);
                    SHELLEXECUTEINFOW sei;
                    ZeroMemory(&sei, sizeof(SHELLEXECUTEINFOW));
                    sei.cbSize = sizeof(sei);
                    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
                    sei.hwnd = NULL;
                    sei.hInstApp = NULL;
                    sei.lpVerb = NULL;
                    sei.lpFile = wszApp;
                    sei.lpParameters = wszArgs;
                    sei.hwnd = NULL;
                    sei.nShow = SW_NORMAL;
                    if (ShellExecuteExW(&sei) && sei.hProcess)
                    {
                        if (WaitForSingleObject(sei.hProcess, 30000) != WAIT_OBJECT_0) GRLog(L"registration helper wait timed out", sei.lpFile, WAIT_TIMEOUT);
                        DWORD dwExitCode = 0;
                        GetExitCodeProcess(sei.hProcess, &dwExitCode);
                        SetLastError(dwExitCode);
                        CloseHandle(sei.hProcess);
                    }
                }
            }
        }
        if (CHECK_OK(bOk)) bOk = InstallResource(bInstall, hInstance, zipFile, PRODUCT_NAME ".IA-32.dll", wszPath, _T(PRODUCT_NAME) L".IA-32.dll");
#if defined(_M_X64)
        if (CHECK_OK(bOk)) bOk = InstallResource(bInstall, hInstance, zipFile, PRODUCT_NAME ".amd64.dll", wszPath, _T(PRODUCT_NAME) L".amd64.dll");
#elif defined(_M_ARM64)
        if (CHECK_OK(bOk)) bOk = InstallResource(bInstall, hInstance, zipFile, PRODUCT_NAME ".arm64.dll", wszPath, _T(PRODUCT_NAME) L".arm64.dll");
#endif
        if (CHECK_OK(bOk)) bOk = InstallResource(bInstall, hInstance, zipFile, "GR-Settings.dll", wszPath, L"GR-Settings.dll");
        if (CHECK_OK(bOk)) bOk = InstallResource(bInstall, hInstance, zipFile, "LICENSE", wszPath, L"LICENSE.txt");
        if (CHECK_OK(bOk)) bOk = InstallResource(bInstall, hInstance, zipFile, "GR-Taskbar.cat", wszPath, L"GR-Taskbar.cat");
        if (CHECK_OK(bOk)) bOk = DeleteResource(wszPath, L"ep_dwm.exe"); // We renamed it to GR-Taskbar-DwmService.exe due to Microsoft imposing 24H2 upgrade blocks
        if (CHECK_OK(bOk)) bOk = InstallResource(bInstall, hInstance, zipFile, "GR-Taskbar-DwmService.exe", wszPath, L"GR-Taskbar-DwmService.exe");
        if (bInstall)
        {
            if (CHECK_OK(bOk)) bOk = InstallResource(bInstall, hInstance, zipFile, "GR-Taskbar-WeatherHost.dll", wszPath, L"GR-Taskbar-WeatherHost.dll");
            if (CHECK_OK(bOk)) bOk = InstallResource(bInstall, hInstance, zipFile, "GR-Taskbar-WeatherProxy.dll", wszPath, L"GR-Taskbar-WeatherProxy.dll");
            if (CHECK_OK(bOk)) bOk = InstallResource(bInstall, hInstance, zipFile, "WebView2Loader.dll", wszPath, L"WebView2Loader.dll");
        }
        ProcessTaskbarDlls(&bOk, bInstall, FALSE, hInstance, zipFile, wszPath);
        const WCHAR* const possibleDirs[] =
        {
            L"ar-SA", L"bg-BG", L"ca-ES", L"cs-CZ", L"da-DK", L"de-DE", L"el-GR", L"en-GB", L"en-US", L"es-ES",
            L"es-MX", L"et-EE", L"eu-ES", L"fi-FI", L"fr-CA", L"fr-FR", L"gl-ES", L"he-IL", L"hr-HR", L"hu-HU",
            L"id-ID", L"it-IT", L"ja-JP", L"ko-KR", L"lt-LT", L"lv-LV", L"nb-NO", L"nl-NL", L"pl-PL", L"pt-BR",
            L"pt-PT", L"ro-RO", L"ru-RU", L"sk-SK", L"sl-SI", L"sr-Latn-RS", L"sv-SE", L"th-TH", L"tr-TR", L"uk-UA",
            L"vi-VN", L"zh-CN", L"zh-TW", L"pris", L"StartUI",
        };
        for (size_t i = 0; CHECK_OK(bOk) && i < ARRAYSIZE(possibleDirs); i++)
        {
            WCHAR wszDirectoryPath[MAX_PATH];
            wcscpy_s(wszDirectoryPath, MAX_PATH, wszPath);
            wcscat_s(wszDirectoryPath, MAX_PATH, L"\\");
            wcscat_s(wszDirectoryPath, MAX_PATH, possibleDirs[i]);
            if (FileExistsW(wszDirectoryPath))
            {
                bOk = RemoveDirectoryRecursive(wszDirectoryPath);
            }
        }
        DeleteResource(wszPath, L"Windows.UI.ShellCommon.pri");
        BOOL bUnpackCustomStartUI = (global_rovi.dwBuildNumber >= 22621 && global_rovi.dwBuildNumber <= 22635) || global_rovi.dwBuildNumber >= 25169;
        BOOL bNoPniduiInThisBuild = global_rovi.dwBuildNumber >= 25236;
        if (bInstall)
        {
            const WCHAR* languages = GetSystemLanguages();
            if (bNoPniduiInThisBuild)
            {
                if (CHECK_OK(bOk)) bOk = ExtractDirectory(zipFile, "pnidui/", wszPath, languages, LCT_MUI);
            }
            if (bUnpackCustomStartUI)
            {
                if (CHECK_OK(bOk)) bOk = ExtractDirectory(zipFile, "Windows.UI.ShellCommon/", wszPath, languages, LCT_PRI);
            }
        }

        if (CHECK_OK(bOk)) bOk = InstallResource(bInstall && bNoPniduiInThisBuild, hInstance, zipFile, "pnidui/pnidui.dll", wszPath, L"pnidui.dll");

        if (CHECK_OK(bOk) && bNoPniduiInThisBuild)
        {
            // Windows Registry Editor Version 5.00
            //
            // [HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ShellServiceObjects\{C2796011-81BA-4148-8FCA-C6643245113F}]
            // "AutoStart"=""
            if (bInstall)
            {
                HKEY hKey;
                RegCreateKeyExW(
                    HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellServiceObjects\\{C2796011-81BA-4148-8FCA-C6643245113F}",
                    0,
                    NULL,
                    REG_OPTION_NON_VOLATILE,
                    KEY_READ | KEY_WRITE,
                    NULL,
                    &hKey,
                    NULL
                );
                if (hKey == NULL || hKey == INVALID_HANDLE_VALUE)
                {
                    hKey = NULL;
                }
                if (hKey)
                {
                    RegSetValueExW(hKey, L"AutoStart", 0, REG_SZ, (const BYTE*)L"", 1 * sizeof(WCHAR));
                    RegCloseKey(hKey);
                }
            }
            else
            {
                RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellServiceObjects\\{C2796011-81BA-4148-8FCA-C6643245113F}");
            }
        }

        if (CHECK_OK(bOk)) bOk = InstallResource(bInstall && IsWindows11(), hInstance, zipFile, "GR-Taskbar-StartTileData.dll", wszPath, L"GR-Taskbar-StartTileData.dll");

        // --------------------------------------------------------------------------------

        // C:\Windows
        // + dxgi.dll
        if (CHECK_OK(bOk)) GetWindowsDirectoryW(wszPath, MAX_PATH);
#if defined(_M_X64)
        if (CHECK_OK(bOk)) bOk = InstallResource(bInstall, hInstance, zipFile, PRODUCT_NAME ".amd64.dll", wszPath, L"dxgi.dll");
#elif defined(_M_ARM64)
        if (CHECK_OK(bOk)) bOk = InstallResource(bInstall, hInstance, zipFile, PRODUCT_NAME ".arm64.dll", wszPath, L"dxgi.dll");
#endif

        // --------------------------------------------------------------------------------

        // C:\Windows\SystemApps\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy
        // + dxgi.dll
        // + JumpViewUI_.dll (download, optional)
        // + StartUI_.dll (download, optional)
        // + wincorlib.dll
        // + wincorlib_orig.dll (symlink)
        // - AppResolverLegacy.dll
        // - StartTileDataLegacy.dll
        // - Windows.UI.ShellCommon.pri
        // - en-US\StartTileDataLegacy.dll.mui
        // - pris2\Windows.UI.ShellCommon.en-US.pri
        if (CHECK_OK(bOk)) GetWindowsDirectoryW(wszPath, MAX_PATH);
        if (CHECK_OK(bOk)) wcscat_s(wszPath, MAX_PATH, L"\\SystemApps\\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy");
        if (CHECK_OK(bOk) && FileExistsW(wszPath))
        {
#if defined(_M_X64)
            if (CHECK_OK(bOk)) bOk = InstallResource(bInstall, hInstance, zipFile, PRODUCT_NAME ".amd64.dll", wszPath, L"dxgi.dll");
#elif defined(_M_ARM64)
            if (CHECK_OK(bOk)) bOk = InstallResource(bInstall, hInstance, zipFile, PRODUCT_NAME ".arm64.dll", wszPath, L"dxgi.dll");
#endif
            if (CHECK_OK(bOk)) bOk = InstallResource(bInstall && IsWindows11(), hInstance, zipFile, "GR-Taskbar-StartMenu.dll", wszPath, L"wincorlib.dll");
            if (CHECK_OK(bOk)) bOk = DeleteResource(wszPath, L"wincorlib_orig.dll");
            if (CHECK_OK(bOk) && IsWindows11() && bInstall)
            {
                // Symlink wincorlib_orig.dll to wincorlib.dll in System32
                WCHAR wszOrigPath[MAX_PATH];
                GetSystemDirectoryW(wszOrigPath, MAX_PATH);
                wcscat_s(wszOrigPath, MAX_PATH, L"\\wincorlib.dll");

                WCHAR wszSymLinkPath[MAX_PATH];
                wcscpy_s(wszSymLinkPath, MAX_PATH, wszPath);
                wcscat_s(wszSymLinkPath, MAX_PATH, L"\\wincorlib_orig.dll");
                bOk = GRInstallSymbolicLink(wszSymLinkPath, wszOrigPath);
            }

            if (CHECK_OK(bOk)) bOk = InstallResource(bInstall && bUnpackCustomStartUI, hInstance, zipFile, "JumpViewUI/JumpViewUI.dll", wszPath, L"JumpViewUI_.dll");
            if (CHECK_OK(bOk)) bOk = InstallResource(bInstall && bUnpackCustomStartUI, hInstance, zipFile, "StartUI/StartUI.dll", wszPath, L"StartUI_.dll");

            // Delete remnants from earlier versions
            if (CHECK_OK(bOk)) bOk = DeleteResource(wszPath, L"AppResolverLegacy.dll");
            if (CHECK_OK(bOk)) bOk = DeleteResource(wszPath, L"StartTileDataLegacy.dll");
            if (CHECK_OK(bOk) && IsWindows11()) bOk = DeleteResource(wszPath, L"Windows.UI.ShellCommon.pri");

            // .\en-US
            if (CHECK_OK(bOk) && IsWindows11())
            {
                WCHAR wszSubPath[MAX_PATH];
                wcscpy_s(wszSubPath, MAX_PATH, wszPath);
                wcscat_s(wszSubPath, MAX_PATH, L"\\en-US");
                if (FileExistsW(wszSubPath))
                {
                    bOk = DeleteResource(wszSubPath, L"StartTileDataLegacy.dll.mui");
                    /* Backups stay in place until commit; empty directories can remain. */
                }
            }

            // .\pris2
            if (CHECK_OK(bOk) && IsWindows11())
            {
                WCHAR wszSubPath[MAX_PATH];
                wcscpy_s(wszSubPath, MAX_PATH, wszPath);
                wcscat_s(wszSubPath, MAX_PATH, L"\\pris2");
                if (FileExistsW(wszSubPath))
                {
                    bOk = DeleteResource(wszSubPath, L"Windows.UI.ShellCommon.en-US.pri");
                    /* Backups stay in place until commit; empty directories can remain. */
                }
            }

            // End remnant deletion
        }

        // --------------------------------------------------------------------------------

        // C:\Windows\SystemApps\ShellExperienceHost_cw5n1h2txyewy
        // + dxgi.dll
        if (CHECK_OK(bOk)) GetWindowsDirectoryW(wszPath, MAX_PATH);
        if (CHECK_OK(bOk)) wcscat_s(wszPath, MAX_PATH, L"\\SystemApps\\ShellExperienceHost_cw5n1h2txyewy");
        if (CHECK_OK(bOk) && FileExistsW(wszPath))
        {
#if defined(_M_X64)
            if (CHECK_OK(bOk) && IsWindows11()) bOk = InstallResource(bInstall, hInstance, zipFile, PRODUCT_NAME ".amd64.dll", wszPath, L"dxgi.dll");
#elif defined(_M_ARM64)
            if (CHECK_OK(bOk) && IsWindows11()) bOk = InstallResource(bInstall, hInstance, zipFile, PRODUCT_NAME ".arm64.dll", wszPath, L"dxgi.dll");
#endif
        }

        // --------------------------------------------------------------------------------

        if (CHECK_OK(bOk))
        {
            GetSystemDirectoryW(wszPath, MAX_PATH);
            WCHAR* pArgs = NULL;
            DWORD dwLen = (DWORD)wcslen(wszPath);
            wcscat_s(wszPath, MAX_PATH - dwLen, L"\\rundll32.exe \"");
            dwLen = (DWORD)wcslen(wszPath);
            pArgs = wszPath + dwLen - 2;
            SHGetFolderPathW(NULL, SPECIAL_FOLDER, NULL, SHGFP_TYPE_CURRENT, wszPath + dwLen);
            wcscat_s(wszPath, MAX_PATH, _T(APP_RELATIVE_PATH) L"\\GR-Settings.dll\",ZZGUI");
            pArgs[0] = 0;
            bOk = SetupShortcut(bInstall, wszPath, pArgs + 1);
            ZeroMemory(wszPath, MAX_PATH);
        }
        if (CHECK_OK(bOk))
        {
            wszPath[0] = L'"';
            SHGetFolderPathW(NULL, SPECIAL_FOLDER, NULL, SHGFP_TYPE_CURRENT, wszPath + 1);
            wcscat_s(wszPath, MAX_PATH, _T(APP_RELATIVE_PATH) L"\\" _T(SETUP_UTILITY_NAME) L"\" /uninstall");
            bOk = SetupUninstallEntry(bInstall, wszPath);
        }
        if (bOk && bInstall && bAreRoundedCornersDisabled) RegisterDWMService(0, 3);
        ShExecInfo.lpParameters = bInstall ? L"start " _T(EP_DWM_SERVICENAME) : L"delete " _T(EP_DWM_SERVICENAME);
        if (bOk && ShellExecuteExW(&ShExecInfo) && ShExecInfo.hProcess)
        {
            if (WaitForSingleObject(ShExecInfo.hProcess, 30000) != WAIT_OBJECT_0) GRLog(L"service helper wait timed out", ShExecInfo.lpFile, WAIT_TIMEOUT);
            DWORD dwExitCode = 0;
            GetExitCodeProcess(ShExecInfo.hProcess, &dwExitCode);
            CloseHandle(ShExecInfo.hProcess);
        }
        if (CHECK_OK(bOk))
        {
            WCHAR wszArgs[MAX_PATH];
            wszArgs[0] = L'/';
            wszArgs[1] = L's';
            wszArgs[2] = L' ';
            wszArgs[3] = L'"';
            if (!bInstall)
            {
                wszArgs[3] = L'/';
                wszArgs[4] = L'u';
                wszArgs[5] = L' ';
                wszArgs[6] = L'"';
            }
            SHGetFolderPathW(NULL, SPECIAL_FOLDER, NULL, SHGFP_TYPE_CURRENT, wszArgs + 4 + (bInstall ? 0 : 3));
            wcscat_s(wszArgs, MAX_PATH, _T(APP_RELATIVE_PATH) L"\\GR-Taskbar-WeatherHost.dll\"");
            wprintf(L"%s\n", wszArgs);
            WCHAR wszApp[MAX_PATH * 2];
            GetSystemDirectoryW(wszApp, MAX_PATH * 2);
            wcscat_s(wszApp, MAX_PATH * 2, L"\\regsvr32.exe");
            wprintf(L"%s\n", wszApp);
            SHELLEXECUTEINFOW sei;
            ZeroMemory(&sei, sizeof(SHELLEXECUTEINFOW));
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.hwnd = NULL;
            sei.hInstApp = NULL;
            sei.lpVerb = NULL;
            sei.lpFile = wszApp;
            sei.lpParameters = wszArgs;
            sei.hwnd = NULL;
            sei.nShow = SW_NORMAL;
            if (ShellExecuteExW(&sei) && sei.hProcess)
            {
                if (WaitForSingleObject(sei.hProcess, 30000) != WAIT_OBJECT_0) GRLog(L"registration helper wait timed out", sei.lpFile, WAIT_TIMEOUT);
                DWORD dwExitCode = 0;
                GetExitCodeProcess(sei.hProcess, &dwExitCode);
                SetLastError(dwExitCode);
                CloseHandle(sei.hProcess);
            }
        }
        if (CHECK_OK(bOk))
        {
            WCHAR wszArgs[MAX_PATH];
            wszArgs[0] = L'/';
            wszArgs[1] = L's';
            wszArgs[2] = L' ';
            wszArgs[3] = L'"';
            if (!bInstall)
            {
                wszArgs[3] = L'/';
                wszArgs[4] = L'u';
                wszArgs[5] = L' ';
                wszArgs[6] = L'"';
            }
            SHGetFolderPathW(NULL, SPECIAL_FOLDER, NULL, SHGFP_TYPE_CURRENT, wszArgs + 4 + (bInstall ? 0 : 3));
            wcscat_s(wszArgs, MAX_PATH, _T(APP_RELATIVE_PATH) L"\\GR-Taskbar-WeatherProxy.dll\"");
            wprintf(L"%s\n", wszArgs);
            WCHAR wszApp[MAX_PATH * 2];
            GetSystemDirectoryW(wszApp, MAX_PATH * 2);
            wcscat_s(wszApp, MAX_PATH * 2, L"\\regsvr32.exe");
            wprintf(L"%s\n", wszApp);
            SHELLEXECUTEINFOW sei;
            ZeroMemory(&sei, sizeof(SHELLEXECUTEINFOW));
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.hwnd = NULL;
            sei.hInstApp = NULL;
            sei.lpVerb = NULL;
            sei.lpFile = wszApp;
            sei.lpParameters = wszArgs;
            sei.hwnd = NULL;
            sei.nShow = SW_NORMAL;
            if (ShellExecuteExW(&sei) && sei.hProcess)
            {
                if (WaitForSingleObject(sei.hProcess, 30000) != WAIT_OBJECT_0) GRLog(L"registration helper wait timed out", sei.lpFile, WAIT_TIMEOUT);
                DWORD dwExitCode = 0;
                GetExitCodeProcess(sei.hProcess, &dwExitCode);
                SetLastError(dwExitCode);
                CloseHandle(sei.hProcess);
            }
        }
        if (CHECK_OK(bOk) && bInstall)
        {
            HKEY hKey = NULL;
            RegCreateKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Policies\\Microsoft\\Windows\\Explorer", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &hKey, NULL);
            if (hKey && hKey != INVALID_HANDLE_VALUE)
            {
                RegCloseKey(hKey);
            }
            RegCreateKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &hKey, NULL);
            if (hKey && hKey != INVALID_HANDLE_VALUE)
            {
                RegCloseKey(hKey);
            }
        }
        if (!bInstall)
        {
            SHGetFolderPathW(NULL, SPECIAL_FOLDER, NULL, SHGFP_TYPE_CURRENT, wszPath);
            wcscat_s(wszPath, MAX_PATH, _T(APP_RELATIVE_PATH));
            if (CHECK_OK(bOk)) bOk = DeleteResource(wszPath, L"GR-Taskbar-WeatherHost.dll");
            if (CHECK_OK(bOk)) bOk = DeleteResource(wszPath, L"GR-Taskbar-WeatherProxy.dll");
            if (CHECK_OK(bOk)) bOk = DeleteResource(wszPath, L"WebView2Loader.dll");
        }

        if (CHECK_OK(bOk))
        {
            if (!bInstall)
            {
                SHGetFolderPathW(NULL, SPECIAL_FOLDER, NULL, SHGFP_TYPE_CURRENT, wszPath);
                wcscat_s(wszPath, MAX_PATH, _T(APP_RELATIVE_PATH));
                GRLog(L"preserve user files; remove known payload only", wszPath, 0);
            }
            if (bInstall) {
                if (bIsUpdate)
                {
                    HKEY hKey = NULL;
                    DWORD dwSize = 0;

                    RegCreateKeyExW(
                        HKEY_CURRENT_USER,
                        TEXT(REGPATH),
                        0,
                        NULL,
                        REG_OPTION_NON_VOLATILE,
                        KEY_READ | KEY_WOW64_64KEY | KEY_WRITE,
                        NULL,
                        &hKey,
                        NULL
                    );
                    if (hKey == NULL || hKey == INVALID_HANDLE_VALUE)
                    {
                        hKey = NULL;
                    }
                    if (hKey)
                    {
                        dwSize = TRUE;
                        RegSetValueExW(
                            hKey,
                            TEXT("IsUpdatePending"),
                            0,
                            REG_DWORD,
                            (const BYTE*)&dwSize,
                            sizeof(DWORD)
                        );
                        RegCloseKey(hKey);
                    }
                }
                //ZZRestartExplorer(0, 0, 0, 0);
            }
        }
        if (bOk) bOk = GRRestartShell(userToken);
        if (bOk) bOk = GRCommitFiles();
        if (!bOk) {
            if (!grFailureCode) GRFail(L"servicing step", wszPath, GetLastError());
            if (grJournalReady && FindWindowW(L"Shell_TrayWnd", NULL) && grChanges)
                GRStopExplorer(FindWindowW(L"Shell_TrayWnd", NULL));
            BOOL restored = grJournalReady && GRRollbackFiles();
            GRCloseJournal(restored);
            BOOL deferred = restored && !bResume && grFailureIsLock && GRScheduleRebootRetry(bInstall);
            if (grStoppedShell) GRRestartShell(userToken);
            WCHAR message[2048], system[512] = L"";
            FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                grFailureCode, 0, system, ARRAYSIZE(system), NULL);
            swprintf_s(message, ARRAYSIZE(message),
                L"GR Taskbar could not complete servicing.\n\nOperation: %s\nFile: %s\nWindows error: %lu (0x%08lX)\n%s\n\n%s\n\n\x2022 Settings and custom icons were preserved.\n\x2022 Diagnostic log: %s\n\nSource checkpoint: %u",
                grFailureOperation, grFailurePath, grFailureCode, grFailureCode, system,
                restored ? L"Previous application files were restored." : L"Some files could not be restored. Backups were retained; see the log.",
                grLogPath, g_uFailureLine);
            if (deferred) wcscat_s(message, ARRAYSIZE(message), L"\n\nThe complete operation is queued for after reboot and sign-in. No partial DLL replacements were queued.");
            GRInstallerMessageBox(NULL, message, _T(PRODUCT_DISPLAY_NAME), MB_ICONERROR | MB_OK);
        }
        GRFreeChanges();
        if (grStoppedShell) GRRestartShell(userToken);
        if (bOk && !bInstall && !bResume && !(argc >= 1 && !_wcsicmp(wargv[0], L"/uninstall_silent"))) {
            WCHAR text[512] = L"";
            LoadStringW(hInstance, bWasShellExt || grNeedsReboot ? IDS_SETUP_UNINSTALL_RESTART : IDS_SETUP_UNINSTALL_FINISH,
                text, ARRAYSIZE(text));
            if (GRInstallerMessageBox(NULL, text, _T(PRODUCT_DISPLAY_NAME), bWasShellExt || grNeedsReboot ? MB_YESNO | MB_ICONQUESTION : MB_OK | MB_ICONINFORMATION) == IDYES)
                SystemShutdown(TRUE);
        }
        if (bOk && bIsUndockingDisabled)
        {
            ExitWindowsEx(EWX_LOGOFF, SHTDN_REASON_FLAG_PLANNED);
            exit(0);
        }

        if (grStoppedShell) GRRestartShell(userToken);
        if (userToken && userToken != INVALID_HANDLE_VALUE) CloseHandle(userToken);
    }

    if (zipFile)
        unzClose(zipFile);
    if (pMem)
        MemoryBuffer_Destroy(&pMem);

    if (bResume && GRBelowRoot(wszOwnPath, grInstallRoot) && !DeleteFileW(wszOwnPath))
        MoveFileExW(wszOwnPath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    GRLog(L"servicing finished", grInstallRoot, grNeedsReboot ? ERROR_SUCCESS_REBOOT_REQUIRED : grFailureCode);
    if (grLog != INVALID_HANDLE_VALUE) CloseHandle(grLog);
    ReleaseMutex(servicingMutex); CloseHandle(servicingMutex);
    return grFailureCode ? (grServicingDeferred ? ERROR_SUCCESS_REBOOT_REQUIRED : grFailureCode) :
        grNeedsReboot ? ERROR_SUCCESS_REBOOT_REQUIRED : ERROR_SUCCESS;
}
