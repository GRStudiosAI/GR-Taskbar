#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <string>
#include <stdio.h>
#include <wil/resource.h>
#include <wil/result_macros.h>
#define GR_STABLE_HELPERS_ONLY
#include "../../GR-Taskbar/StableUpdate.h"
#pragma comment(lib,"wininet.lib")
static int failed=0;
#define CHECK(x) do {if(!(x)){printf("FAIL: %s\n",#x);++failed;}}while(0)
int wmain(int argc,wchar_t** argv)
{
    ULONGLONG a=0,b=0;
    CHECK(GRStableParseVersion(L"v26100.9711.71.16",&a));
    CHECK(GRStableParseVersion(L"26100.9711.71.9",&b));CHECK(a>b);
    for(auto invalid:{L"1.2.3",L"1.2.3.4beta",L"1.2.3.65536",L"1.2.-3.4",L"1.2.3.4/evil",L"999999999999999999999.2.3.4",L"1.2.3.4?",L""}) CHECK(!GRStableParseVersion(invalid,&a));
    CHECK(GRStableHost(L"https://api.github.com/repos/GRStudiosAI/GR-Taskbar/releases/latest"));
    CHECK(GRStableHost(L"https://release-assets.githubusercontent.com/asset"));
    for(auto invalid:{L"http://github.com/file",L"https://github.com.evil.test/file",L"https://evil.test/file",L"https://github.com:444/file",L"https://user:pass@github.com/file"}) CHECK(!GRStableHost(invalid));
    CHECK(!GRTrustedUpdate(L"C:\\Windows\\System32\\notepad.exe"));
    if(argc>1){CHECK(GRStableFileVersion(argv[1],&a));HANDLE file=CreateFileW(argv[1],GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        CHECK(file!=INVALID_HANDLE_VALUE);if(file!=INVALID_HANDLE_VALUE){BYTE digest[32],expected[32];CHECK(GRStableDigest(file,digest));CHECK(GRSha256File(argv[1],expected));CHECK(!memcmp(digest,expected,32));CHECK(GRStableArchitecture(file));CloseHandle(file);}
        // The development certificate remains self-signed. Do not install it into
        // a trust store or make an exception to Windows signature policy here.
        printf("Existing installer Windows-trusted and identity-pinned: %s\n",GRTrustedUpdate(argv[1])?"yes":"no (automatic installation will reject it)");
    }
    printf("Stable updater verification: %d failures\n",failed);return failed?1:0;
}
