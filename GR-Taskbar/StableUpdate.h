#pragma once
// Original GR Studios AI LLC stable-release servicing, 2026.
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#include <vector>
#include "CryptoFile.h"
#pragma comment(lib,"wintrust.lib")
#pragma comment(lib,"crypt32.lib")
#pragma comment(lib,"version.lib")

static bool GRStableDigest(HANDLE file, BYTE digest[32])
{
    LARGE_INTEGER zero = {}; if (!SetFilePointerEx(file, zero, nullptr, FILE_BEGIN)) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    auto closeAlgorithm = wil::scope_exit([&]{BCryptCloseAlgorithmProvider(algorithm,0);});
    if (BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0) return false;
    auto closeHash = wil::scope_exit([&]{BCryptDestroyHash(hash);});
    BYTE block[65536]; DWORD count=0;
    for (;;) { if (!ReadFile(file,block,sizeof(block),&count,nullptr)) return false;
        if (!count) break; if (BCryptHashData(hash,block,count,0)<0) return false; }
    return BCryptFinishHash(hash,digest,32,0)>=0;
}

static bool GRTrustedUpdate(PCWSTR path)
{
    WINTRUST_FILE_INFO file = { sizeof(file) }; file.pcwszFilePath = path;
    WINTRUST_DATA trust = { sizeof(trust) };
    trust.dwUIChoice = WTD_UI_NONE; trust.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    trust.dwUnionChoice = WTD_CHOICE_FILE; trust.pFile = &file;
    trust.dwStateAction = WTD_STATEACTION_VERIFY;
    trust.dwProvFlags = WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG result = WinVerifyTrust(NULL, &action, &trust);
    bool valid = false;
    auto provider = WTHelperProvDataFromStateData(trust.hWVTStateData);
    auto signer = provider ? WTHelperGetProvSignerFromChain(provider, 0, FALSE, 0) : nullptr;
    if (result == ERROR_SUCCESS && signer && signer->csCertChain && signer->csCounterSigners) {
        BYTE hash[32]; DWORD bytes = sizeof(hash); WCHAR hex[65];
        if (CertGetCertificateContextProperty(signer->pasCertChain[0].pCert, CERT_SHA256_HASH_PROP_ID, hash, &bytes) && bytes == 32) {
            GRHashHex(hash, 32, hex);
            valid = !_wcsicmp(hex, L"3b6de2749b601fdbc077e0c228b43191f0675ac829620360b0598244e751da1c");
        }
    }
    trust.dwStateAction = WTD_STATEACTION_CLOSE; WinVerifyTrust(NULL, &action, &trust);
    return valid;
}

static void GRStableStatus(PCWSTR text, DWORD error = 0)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\GR-Taskbar\\Updater", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(key, L"Status", 0, REG_SZ, (const BYTE*)text, (DWORD)((wcslen(text)+1)*sizeof(WCHAR)));
        RegSetValueExW(key, L"NativeError", 0, REG_DWORD, (const BYTE*)&error, sizeof(error));
        RegCloseKey(key);
    }
}

static bool GRStableHost(const std::wstring& url)
{
    URL_COMPONENTSW parts = { sizeof(parts) }; WCHAR host[256] = {};
    parts.lpszHostName = host; parts.dwHostNameLength = ARRAYSIZE(host);
    parts.dwUserNameLength = (DWORD)-1; parts.dwPasswordLength = (DWORD)-1;
    if (!InternetCrackUrlW(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS || parts.nPort != 443 || parts.dwUserNameLength || parts.dwPasswordLength) return false;
    return !_wcsicmp(host,L"api.github.com") || !_wcsicmp(host,L"github.com") || !_wcsicmp(host,L"release-assets.githubusercontent.com") || !_wcsicmp(host,L"objects.githubusercontent.com");
}

static HINTERNET GRStableOpen(HINTERNET session, std::wstring url)
{
    for (int hop = 0; hop < 6; ++hop) {
        if (!GRStableHost(url)) return nullptr;
        HINTERNET request = InternetOpenUrlW(session, url.c_str(), L"Accept: application/vnd.github+json\r\n", (DWORD)-1,
            INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_AUTO_REDIRECT | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_COOKIES, 0);
        if (!request) return nullptr;
        DWORD status = 0, size = sizeof(status);
        if (!HttpQueryInfoW(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &size, nullptr)) { InternetCloseHandle(request); return nullptr; }
        if (status == 200) return request;
        WCHAR next[4096]; size = sizeof(next);
        bool redirect = (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) && HttpQueryInfoW(request, HTTP_QUERY_LOCATION, next, &size, nullptr);
        InternetCloseHandle(request); if (!redirect) return nullptr; url = next;
    }
    return nullptr;
}

static bool GRStableFileVersion(PCWSTR path, ULONGLONG* value)
{
    DWORD ignored = 0, size = GetFileVersionInfoSizeW(path, &ignored);
    if (!size || size > 1024*1024) return false;
    std::vector<BYTE> buffer(size); VS_FIXEDFILEINFO* version = nullptr; UINT bytes = 0;
    if (!GetFileVersionInfoW(path,0,size,buffer.data()) || !VerQueryValueW(buffer.data(),L"\\",(void**)&version,&bytes) || bytes < sizeof(*version) || version->dwSignature != 0xfeef04bd) return false;
    *value = ((ULONGLONG)version->dwFileVersionMS << 32) | version->dwFileVersionLS; return true;
}

static bool GRStableParseVersion(PCWSTR text, ULONGLONG* result)
{
    if (!text || !result) return false;
    if (*text==L'v') ++text;
    ULONGLONG version=0;
    for (int part=0;part<4;++part) {
        if (*text<L'0'||*text>L'9') return false;
        unsigned number=0;
        do { number=number*10+(*text++-L'0'); if(number>65535) return false; }
        while(*text>=L'0'&&*text<=L'9');
        version=(version<<16)|number;
        if(part<3) { if(*text++!=L'.') return false; }
    }
    if(*text) return false;
    *result=version; return true;
}

static bool GRStableArchitecture(HANDLE file)
{
    IMAGE_DOS_HEADER dos={}; DWORD count=0; LARGE_INTEGER position={};
    if(!SetFilePointerEx(file,position,nullptr,FILE_BEGIN)||!ReadFile(file,&dos,sizeof(dos),&count,nullptr)||count!=sizeof(dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||dos.e_lfanew>1024*1024) return false;
    position.QuadPart=dos.e_lfanew;
    struct { DWORD signature; IMAGE_FILE_HEADER header; } pe={};
    if(!SetFilePointerEx(file,position,nullptr,FILE_BEGIN)||!ReadFile(file,&pe,sizeof(pe),&count,nullptr)||count!=sizeof(pe)||pe.signature!=IMAGE_NT_SIGNATURE) return false;
#ifdef _M_ARM64
    return pe.header.Machine==IMAGE_FILE_MACHINE_ARM64;
#else
    return pe.header.Machine==IMAGE_FILE_MACHINE_AMD64;
#endif
}

#ifndef GR_STABLE_HELPERS_ONLY
static bool GRStableCheck(bool install)
{
    using namespace ABI::Windows::Data::Json;
    GRStableStatus(L"Checking official stable releases");
    HINTERNET session = InternetOpenW(L"GR Taskbar", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!session) return false;
    auto closeSession = wil::scope_exit([&]{ InternetCloseHandle(session); });
    DWORD timeout = 30000;
    InternetSetOptionW(session, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionW(session, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    HINTERNET request = GRStableOpen(session,L"https://api.github.com/repos/GRStudiosAI/GR-Taskbar/releases/latest");
    if (!request) { GRStableStatus(L"No published stable release was found, or the connection failed",GetLastError()); return false; }
    std::string json; char block[16384]; DWORD read = 0; bool ok = true;
    for (;;) {
        if (!InternetReadFile(request,block,sizeof(block),&read)) { ok=false; break; }
        if (!read) break;
        if (json.size()+read > 2*1024*1024) { ok=false; break; }
        json.append(block,read);
    }
    InternetCloseHandle(request); if (!ok || json.empty()) return false;
    int length = MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,json.data(),(int)json.size(),nullptr,0);
    if (length <= 0) return false;
    std::wstring wide(length,L'\0'); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,json.data(),(int)json.size(),&wide[0],length);
    ComPtr<IJsonObjectStatics> parser; ComPtr<IJsonObject> release;
    if (FAILED(GetActivationFactory(Wrappers::HStringReference(RuntimeClass_Windows_Data_Json_JsonObject).Get(),&parser)) || FAILED(parser->Parse(Wrappers::HStringReference(wide.c_str()).Get(),&release))) return false;
    boolean draft=true, preview=true;
    if (FAILED(release->GetNamedBoolean(Wrappers::HStringReference(L"draft").Get(),&draft)) || FAILED(release->GetNamedBoolean(Wrappers::HStringReference(L"prerelease").Get(),&preview)) || draft || preview) return false;
    Wrappers::HString tag;
    if (FAILED(release->GetNamedString(Wrappers::HStringReference(L"tag_name").Get(),tag.ReleaseAndGetAddressOf()))) return false;
    ULONGLONG remote=0, local=0;
    if(!GRStableParseVersion(tag.GetRawBuffer(nullptr),&remote)) return false;
    WCHAR module[MAX_PATH]; if (!GetModuleFileNameW(hModule,module,MAX_PATH) || !GRStableFileVersion(module,&local)) return false;
    if (remote<=local) { GRStableStatus(L"GR Taskbar is up to date"); return true; }
    ComPtr<IJsonArray> assets; if (FAILED(release->GetNamedArray(Wrappers::HStringReference(L"assets").Get(),&assets))) return false;
#ifdef _M_ARM64
    PCWSTR expected=L"GR-Taskbar-Setup-ARM64.exe";
#else
    PCWSTR expected=L"GR-Taskbar-Setup.exe";
#endif
    UINT count=0; ComPtr<ABI::Windows::Foundation::Collections::IVector<IJsonValue*>> values;
    if (FAILED(assets.As(&values)) || FAILED(values->get_Size(&count)) || count>200) return false;
    std::wstring url,digest; double size=0;
    for (UINT i=0;i<count;++i) {
        ComPtr<IJsonObject> asset; Wrappers::HString name,link,hash;
        if (FAILED(assets->GetObjectAt(i,&asset)) || FAILED(asset->GetNamedString(Wrappers::HStringReference(L"name").Get(),name.ReleaseAndGetAddressOf()))) continue;
        if (wcscmp(name.GetRawBuffer(nullptr),expected)) continue;
        if (FAILED(asset->GetNamedString(Wrappers::HStringReference(L"browser_download_url").Get(),link.ReleaseAndGetAddressOf())) || FAILED(asset->GetNamedString(Wrappers::HStringReference(L"digest").Get(),hash.ReleaseAndGetAddressOf())) || FAILED(asset->GetNamedNumber(Wrappers::HStringReference(L"size").Get(),&size))) return false;
        url=link.GetRawBuffer(nullptr); digest=hash.GetRawBuffer(nullptr); break;
    }
    std::wstring canonical=L"https://github.com/GRStudiosAI/GR-Taskbar/releases/download/";
    canonical+=tag.GetRawBuffer(nullptr); canonical+=L"/"; canonical+=expected;
    if (url!=canonical || digest.size()!=71 || digest.compare(0,7,L"sha256:") || size<1024 || size>1024*1024*1024) { GRStableStatus(L"The release is missing valid verification information",ERROR_INVALID_DATA); return false; }
    if (!install) { GRStableStatus(L"A stable update is available. Select Install update."); return true; }
    WCHAR base[MAX_PATH]; if (FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,base))) return false;
    std::wstring folder=base; folder+=L"\\GR-Taskbar"; CreateDirectoryW(folder.c_str(),nullptr); folder+=L"\\Updates"; CreateDirectoryW(folder.c_str(),nullptr);
    WCHAR temporary[MAX_PATH]; if (!GetTempFileNameW(folder.c_str(),L"GRU",0,temporary)) return false;
    std::wstring path=temporary; path+=L".exe"; if (!MoveFileExW(temporary,path.c_str(),0)) { DeleteFileW(temporary); return false; }
    auto remove=wil::scope_exit([&]{DeleteFileW(path.c_str());});
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file==INVALID_HANDLE_VALUE) return false;
    auto closeFile=wil::scope_exit([&]{CloseHandle(file);});
    request=GRStableOpen(session,url); if (!request) return false;
    ULONGLONG total=0; GRStableStatus(L"Downloading official stable update");
    for (;;) {
        if (!InternetReadFile(request,block,sizeof(block),&read)) { ok=false; break; }
        if (!read) break;
        DWORD written=0; total+=read;
        if (total>(ULONGLONG)size || !WriteFile(file,block,read,&written,nullptr) || written!=read) { ok=false; break; }
    }
    InternetCloseHandle(request); FlushFileBuffers(file);
    // Reopen using a reduced-access duplicate. The file remains continuously locked
    // against modification and deletion until elevated installation has completed.
    HANDLE reader=INVALID_HANDLE_VALUE;
    if (!DuplicateHandle(GetCurrentProcess(),file,GetCurrentProcess(),&reader,GENERIC_READ,FALSE,0)) return false;
    CloseHandle(file); file=reader;
    BYTE hash[32]; WCHAR hex[65]; ULONGLONG actual=0;
    if (!ok || total!=(ULONGLONG)size || !GRStableDigest(file,hash)) return false;
    GRHashHex(hash,32,hex);
    if (_wcsicmp(hex,digest.c_str()+7) || !GRStableArchitecture(file) || !GRTrustedUpdate(path.c_str()) || !GRStableFileVersion(path.c_str(),&actual) || actual!=remote) { GRStableStatus(L"Update rejected: checksum, architecture, Windows signature trust, signing identity, timestamp or version failed verification",ERROR_INVALID_DATA); return false; }
    GRStableStatus(L"Installing verified GR Taskbar update");
    SHELLEXECUTEINFOW execute={sizeof(execute)}; execute.fMask=SEE_MASK_NOCLOSEPROCESS; execute.lpVerb=L"runas"; execute.lpFile=path.c_str(); execute.lpParameters=L"/update_silent"; execute.nShow=SW_SHOWNORMAL;
    if (!ShellExecuteExW(&execute)) { GRStableStatus(L"Installation was cancelled or could not start",GetLastError()); return false; }
    WaitForSingleObject(execute.hProcess,INFINITE); DWORD exit=1; GetExitCodeProcess(execute.hProcess,&exit); CloseHandle(execute.hProcess);
    GRStableStatus(exit==0||exit==3010?L"Update installation completed":L"Update installation failed; see installer recovery log",exit);
    return exit==0||exit==3010;
}

static DWORD GRStableWorker(LPVOID parameters)
{
    HRESULT initialized=RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(initialized)) return (DWORD)initialized;
    auto uninitialize=wil::scope_exit([&]{RoUninitialize();});
    HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\GRTaskbar_StableUpdater");
    if (!mutex) return GetLastError();
    auto closeMutex=wil::scope_exit([&]{CloseHandle(mutex);});
    DWORD acquired=WaitForSingleObject(mutex,0);
    if (acquired!=WAIT_OBJECT_0 && acquired!=WAIT_ABANDONED) return ERROR_ALREADY_EXISTS;
    auto releaseMutex=wil::scope_exit([&]{ReleaseMutex(mutex);});
    HANDLE events[3]={
        CreateEventW(nullptr,FALSE,FALSE,L"EP_Ev_CheckForUpdates_" _T(EP_CLSID)),
        CreateEventW(nullptr,FALSE,FALSE,L"EP_Ev_InstallUpdates_" _T(EP_CLSID)),
        CreateEventW(nullptr,FALSE,FALSE,L"EP_Ev_InstallUpdatesNoConfirm_" _T(EP_CLSID))};
    auto closeEvents=wil::scope_exit([&]{for(auto event:events)if(event)CloseHandle(event);});
    for(auto event:events)if(!event)return GetLastError();
    // Compatibility event names are private IPC; download sources cannot be supplied.
    DWORD wake=WaitForMultipleObjects(3,events,FALSE,min((DWORD)(UINT_PTR)parameters,60000UL));
    for (;;) {
        if(wake==WAIT_FAILED) return GetLastError();
        bool ok=false;
        for(int attempt=0;attempt<3&&!ok;++attempt){
            ok=GRStableCheck(wake!=WAIT_OBJECT_0);
            if(!ok&&attempt<2)Sleep((attempt+1)*5000);
        }
        wake=WaitForMultipleObjects(3,events,FALSE,6*60*60*1000);
    }
}

#endif
