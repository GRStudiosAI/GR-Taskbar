#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <tchar.h>
#define EP_CLSID "test"
#define VER_WITH_DOTS "test"
static int queued;
static BOOL TestMove(PCWSTR from,PCWSTR to,DWORD flags){if(flags&MOVEFILE_DELAY_UNTIL_REBOOT){queued++;return TRUE;}return MoveFileExW(from,to,flags);}
#define MoveFileExW TestMove
static BOOL rejectQueue;static WCHAR queuedCommand[2*MAX_PATH];
static LSTATUS TestCreateKey(HKEY root,LPCWSTR path,DWORD reserved,LPWSTR cls,DWORD options,REGSAM access,const LPSECURITY_ATTRIBUTES sa,PHKEY out,LPDWORD disp){
 (void)reserved;(void)cls;(void)options;(void)access;(void)sa;(void)disp;assert(root==HKEY_LOCAL_MACHINE);assert(wcsstr(path,L"RunOnce"));if(rejectQueue)return ERROR_ACCESS_DENIED;*out=(HKEY)(UINT_PTR)0x1234;return ERROR_SUCCESS;
}
static LSTATUS TestSetValue(HKEY key,LPCWSTR name,DWORD reserved,DWORD type,const BYTE* data,DWORD bytes){(void)name;(void)reserved;assert(key==(HKEY)(UINT_PTR)0x1234);assert(type==REG_SZ);assert(bytes==(wcslen((LPCWSTR)data)+1)*sizeof(WCHAR));wcscpy_s(queuedCommand,2*MAX_PATH,(LPCWSTR)data);return ERROR_SUCCESS;}
static LSTATUS TestCloseKey(HKEY key){assert(key==(HKEY)(UINT_PTR)0x1234);return ERROR_SUCCESS;}
#define RegCreateKeyExW TestCreateKey
#define RegSetValueExW TestSetValue
#define RegCloseKey TestCloseKey
#include "../../gr_setup/Servicing.h"
#undef MoveFileExW
static WCHAR root[MAX_PATH], a[MAX_PATH], b[MAX_PATH], stage[MAX_PATH];
static void put(PCWSTR p,const char* content){HANDLE h=CreateFileW(p,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);assert(h!=INVALID_HANDLE_VALUE);DWORD n;assert(WriteFile(h,content,(DWORD)strlen(content),&n,NULL));CloseHandle(h);}
static void expect(PCWSTR p,const char* content){char bytes[100]={0};DWORD n;HANDLE h=CreateFileW(p,GENERIC_READ,7,NULL,OPEN_EXISTING,0,NULL);assert(h!=INVALID_HANDLE_VALUE);assert(ReadFile(h,bytes,99,&n,NULL));CloseHandle(h);assert(!strcmp(bytes,content));}
static void reset(void){GRFreeChanges();grFailureCode=0;grFailureOperation[0]=0;grFailurePath[0]=0;assert(GRBeginFiles());}
static void rollback(void){assert(GRRollbackFiles());GRCloseJournal(TRUE);GRFreeChanges();}
static DWORD WINAPI releaseLock(void* h){Sleep(400);CloseHandle((HANDLE)h);return 0;}
int wmain(void){
 assert(GetFullPathNameW(L"servicing-tests",MAX_PATH,root,NULL));CreateDirectoryW(root,NULL);wcscpy_s(grInstallRoot,MAX_PATH,root);
 swprintf_s(a,MAX_PATH,L"%s\\payload.dll",root);swprintf_s(b,MAX_PATH,L"%s\\second.dll",root);swprintf_s(stage,MAX_PATH,L"%s\\staged.tmp",root);
 WCHAR log[MAX_PATH];swprintf_s(log,MAX_PATH,L"%s\\test.log",root);grLog=CreateFileW(log,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 DeleteFileW(a);DeleteFileW(b);reset();put(stage,"v1");assert(GRPublishFile(a,stage,FALSE));assert(GRCommitFiles());expect(a,"v1");puts("PASS fresh install");
 reset();put(stage,"v2");assert(GRPublishFile(a,stage,FALSE));assert(GRCommitFiles());expect(a,"v2");puts("PASS upgrade with old file backup");
 reset();put(stage,"v2");assert(GRPublishFile(a,stage,FALSE));assert(GRCommitFiles());expect(a,"v2");puts("PASS repair/reinstall");
 reset();put(stage,"v3");assert(GRPublishFile(a,stage,FALSE));put(b,"old-second");HANDLE lock=CreateFileW(b,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);assert(lock!=INVALID_HANDLE_VALUE);
 put(stage,"new-second");assert(!GRPublishFile(b,stage,FALSE));assert(grFailureCode==ERROR_SHARING_VIOLATION);assert(!_wcsicmp(grFailurePath,b));rollback();expect(a,"v2");expect(b,"old-second");CloseHandle(lock);DeleteFileW(stage);puts("PASS actual native error 32 captured with failing path; earlier update rolled back");
 reset();lock=CreateFileW(a,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);HANDLE thread=CreateThread(NULL,0,releaseLock,lock,0,NULL);put(stage,"v4");assert(GRPublishFile(a,stage,FALSE));assert(GRCommitFiles());WaitForSingleObject(thread,INFINITE);CloseHandle(thread);expect(a,"v4");puts("PASS transient real lock released automatically during bounded retry");
 reset();assert(!GRPublishFile(a,L"missing-stage.tmp",FALSE));rollback();expect(a,"v4");puts("PASS publish failure preserves old file");
 reset();put(stage,"crash-version");assert(GRPublishFile(a,stage,FALSE));GRCloseJournal(FALSE);GRFreeChanges();assert(GRBeginFiles());expect(a,"v4");GRCloseJournal(TRUE);puts("PASS interrupted transaction recovered from durable journal");
 reset();assert(GRPublishFile(a,NULL,TRUE));put(stage,"crash-again");assert(GRPublishFile(a,stage,FALSE));GRCloseJournal(FALSE);GRFreeChanges();assert(GRBeginFiles());expect(a,"v4");GRCloseJournal(TRUE);puts("PASS repeated target recovery in reverse transaction order");
 reset();WCHAR prev[MAX_PATH];swprintf_s(prev,MAX_PATH,L"%s\\payload.prev",root);put(prev,"stale");lock=CreateFileW(prev,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);put(stage,"v5");assert(GRPublishFile(a,stage,FALSE));assert(GRCommitFiles());assert(queued==1);CloseHandle(lock);DeleteFileW(prev);expect(a,"v5");puts("PASS locked stale .prev does not block upgrade; reboot cleanup mocked");
 reset();assert(GRPublishFile(a,NULL,TRUE));assert(GRPublishFile(b,NULL,TRUE));assert(GRCommitFiles());assert(GetFileAttributesW(a)==INVALID_FILE_ATTRIBUTES);puts("PASS uninstall of known payload files");
 grFailureCode=0;assert(!GRValidateTarget(L"C:\\Windows\\System32\\kernel32.dll"));assert(grFailureCode==ERROR_ACCESS_DENIED);puts("PASS native Windows target rejected without modification");
 reset();WCHAR uni[MAX_PATH];swprintf_s(uni,MAX_PATH,L"%s\\\x732B\x0020\x00E9.dll",root);put(stage,"unicode");assert(GRPublishFile(uni,stage,FALSE));assert(GRCommitFiles());expect(uni,"unicode");DeleteFileW(uni);puts("PASS Unicode file paths");
 reset();put(a,"before-loaded-image");WCHAR system[MAX_PATH];GetSystemDirectoryW(system,MAX_PATH);wcscat_s(system,MAX_PATH,L"\\version.dll");assert(CopyFileW(system,stage,FALSE));assert(GRPublishFile(a,stage,FALSE));
 HMODULE mapped=LoadLibraryExW(a,NULL,DONT_RESOLVE_DLL_REFERENCES);assert(mapped);assert(!DeleteFileW(a));assert(GetLastError()==ERROR_ACCESS_DENIED);rollback();expect(a,"before-loaded-image");FreeLibrary(mapped);DeleteFileW(a);puts("PASS rollback of a mapped DLL that denies DeleteFile (rename/displace recovery)");
 grFailureCode=0;assert(GRScheduleRebootRetry(TRUE));assert(wcsstr(queuedCommand,L"/resume_install"));
 WCHAR pending[MAX_PATH];WCHAR* end=wcschr(queuedCommand+1,L'"');assert(end);wcsncpy_s(pending,MAX_PATH,queuedCommand+1,end-queuedCommand-1);assert(GRBelowRoot(pending,root));assert(GetFileAttributesW(pending)!=INVALID_FILE_ATTRIBUTES);DeleteFileW(pending);
 assert(GRScheduleRebootRetry(FALSE));assert(wcsstr(queuedCommand,L"/resume_uninstall"));end=wcschr(queuedCommand+1,L'"');wcsncpy_s(pending,MAX_PATH,queuedCommand+1,end-queuedCommand-1);DeleteFileW(pending);
 rejectQueue=TRUE;assert(!GRScheduleRebootRetry(TRUE));puts("PASS full install/uninstall reboot-resume commands and queue failure (registry mocked)");
 swprintf_s(pending,MAX_PATH,L"%s\\servicing.rollback",root);put(pending,"corrupt");grFailureCode=0;assert(!GRBeginFiles());GRCloseJournal(TRUE);assert(GetFileAttributesW(pending)!=INVALID_FILE_ATTRIBUTES);DeleteFileW(pending);puts("PASS corrupt recovery journal is retained rather than erased");
 GRFreeChanges();CloseHandle(grLog);puts("All file-servicing checks passed. No Explorer shutdown or live registry changes performed.");return 0;
}
