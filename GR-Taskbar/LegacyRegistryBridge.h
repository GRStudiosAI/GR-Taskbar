#pragma once
#include "SettingsMigration.h"

typedef BOOL (WINAPI* GRCompareHandlesFn)(HANDLE,HANDLE);
static HKEY grLegacyBackendKeys[3];
static HKEY grBackendNotifyKeys[3];
static SRWLOCK grBackendRegistryLock=SRWLOCK_INIT;
static GRCompareHandlesFn grCompareRegistryHandles;

static HKEY GRBackendKey(HKEY key,REGSAM access,HKEY* temporary,BOOL notify)
{
    *temporary=NULL;
    if(!grCompareRegistryHandles)return key;
    for(UINT i=0;i<ARRAYSIZE(grLegacyBackendKeys);++i){
        if(!grLegacyBackendKeys[i]||!grCompareRegistryHandles(key,grLegacyBackendKeys[i]))continue;
        if(notify){
            AcquireSRWLockExclusive(&grBackendRegistryLock);
            if(grBackendNotifyKeys[i]&&RegQueryInfoKeyW(grBackendNotifyKeys[i],NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL)==ERROR_KEY_DELETED){RegCloseKey(grBackendNotifyKeys[i]);grBackendNotifyKeys[i]=NULL;}
            if(!grBackendNotifyKeys[i])RegOpenKeyExW(HKEY_CURRENT_USER,grMigratedSettingsKeys[i],0,KEY_NOTIFY|KEY_QUERY_VALUE,&grBackendNotifyKeys[i]);
            HKEY mapped=grBackendNotifyKeys[i];ReleaseSRWLockExclusive(&grBackendRegistryLock);return mapped?mapped:key;
        }
        if(!RegOpenKeyExW(HKEY_CURRENT_USER,grMigratedSettingsKeys[i],0,access,temporary))return *temporary;
        return key;
    }
    return key;
}
static PCWSTR GRBackendSubkey(HKEY key,PCWSTR subkey,WCHAR mapped[1024])
{
    return key==HKEY_CURRENT_USER||key==HKEY_LOCAL_MACHINE?GRMapLegacyProductKey(subkey,mapped):subkey;
}
#define GR_BRIDGE_KEY(access) HKEY temporary=NULL;HKEY mappedKey=GRBackendKey(key,access,&temporary,FALSE)
#define GR_BRIDGE_END(expression) do{LSTATUS grBridgeStatus=(expression);if(temporary)RegCloseKey(temporary);return grBridgeStatus;}while(0)
static LSTATUS WINAPI GRBackendRegOpenKeyExW(HKEY key,LPCWSTR sub,DWORD options,REGSAM access,PHKEY result)
{GR_BRIDGE_KEY(KEY_READ);WCHAR path[1024];GR_BRIDGE_END(RegOpenKeyExW(mappedKey,GRBackendSubkey(mappedKey,sub,path),options,access,result));}
static LSTATUS WINAPI GRBackendRegCreateKeyW(HKEY key,LPCWSTR sub,PHKEY result)
{GR_BRIDGE_KEY(KEY_CREATE_SUB_KEY);WCHAR path[1024];GR_BRIDGE_END(RegCreateKeyW(mappedKey,GRBackendSubkey(mappedKey,sub,path),result));}
static LSTATUS WINAPI GRBackendRegCreateKeyExW(HKEY key,LPCWSTR sub,DWORD reserved,LPWSTR className,DWORD options,REGSAM access,const LPSECURITY_ATTRIBUTES security,PHKEY result,LPDWORD disposition)
{GR_BRIDGE_KEY(KEY_CREATE_SUB_KEY);WCHAR path[1024];GR_BRIDGE_END(RegCreateKeyExW(mappedKey,GRBackendSubkey(mappedKey,sub,path),reserved,className,options,access,security,result,disposition));}
static LSTATUS WINAPI GRBackendRegGetValueW(HKEY key,LPCWSTR sub,LPCWSTR name,DWORD flags,LPDWORD type,PVOID data,LPDWORD bytes)
{GR_BRIDGE_KEY(KEY_QUERY_VALUE);WCHAR path[1024];GR_BRIDGE_END(RegGetValueW(mappedKey,GRBackendSubkey(mappedKey,sub,path),name,flags,type,data,bytes));}
static LSTATUS WINAPI GRBackendRegQueryValueExW(HKEY key,LPCWSTR name,LPDWORD reserved,LPDWORD type,LPBYTE data,LPDWORD bytes)
{GR_BRIDGE_KEY(KEY_QUERY_VALUE);GR_BRIDGE_END(RegQueryValueExW(mappedKey,name,reserved,type,data,bytes));}
static LSTATUS WINAPI GRBackendRegSetValueExW(HKEY key,LPCWSTR name,DWORD reserved,DWORD type,const BYTE* data,DWORD bytes)
{GR_BRIDGE_KEY(KEY_SET_VALUE);GR_BRIDGE_END(RegSetValueExW(mappedKey,name,reserved,type,data,bytes));}
static LSTATUS WINAPI GRBackendRegDeleteValueW(HKEY key,LPCWSTR name)
{GR_BRIDGE_KEY(KEY_SET_VALUE);GR_BRIDGE_END(RegDeleteValueW(mappedKey,name));}
static LSTATUS WINAPI GRBackendRegQueryInfoKeyW(HKEY key,LPWSTR className,LPDWORD chars,LPDWORD reserved,LPDWORD subkeys,LPDWORD maxSubkey,LPDWORD maxClass,LPDWORD values,LPDWORD maxValueName,LPDWORD maxValue,LPDWORD security,PFILETIME written)
{GR_BRIDGE_KEY(KEY_READ);GR_BRIDGE_END(RegQueryInfoKeyW(mappedKey,className,chars,reserved,subkeys,maxSubkey,maxClass,values,maxValueName,maxValue,security,written));}
static LSTATUS WINAPI GRBackendRegEnumKeyExW(HKEY key,DWORD index,LPWSTR name,LPDWORD chars,LPDWORD reserved,LPWSTR className,LPDWORD classChars,PFILETIME written)
{GR_BRIDGE_KEY(KEY_ENUMERATE_SUB_KEYS);GR_BRIDGE_END(RegEnumKeyExW(mappedKey,index,name,chars,reserved,className,classChars,written));}
static LSTATUS WINAPI GRBackendRegEnumValueW(HKEY key,DWORD index,LPWSTR name,LPDWORD chars,LPDWORD reserved,LPDWORD type,LPBYTE data,LPDWORD bytes)
{GR_BRIDGE_KEY(KEY_QUERY_VALUE);GR_BRIDGE_END(RegEnumValueW(mappedKey,index,name,chars,reserved,type,data,bytes));}
static LSTATUS WINAPI GRBackendRegNotifyChangeKeyValue(HKEY key,BOOL subtree,DWORD filter,HANDLE event,BOOL asynchronous)
{HKEY temporary=NULL;HKEY mappedKey=GRBackendKey(key,KEY_NOTIFY,&temporary,TRUE);return RegNotifyChangeKeyValue(mappedKey,subtree,filter,event,asynchronous);}
static LSTATUS WINAPI GRBackendSHGetValueW(HKEY key,LPCWSTR sub,LPCWSTR name,LPDWORD type,LPVOID data,LPDWORD bytes)
{GR_BRIDGE_KEY(KEY_QUERY_VALUE);WCHAR path[1024];GR_BRIDGE_END(SHGetValueW(mappedKey,GRBackendSubkey(mappedKey,sub,path),name,type,data,bytes));}
static LSTATUS WINAPI GRBackendSHSetValueW(HKEY key,LPCWSTR sub,LPCWSTR name,DWORD type,LPCVOID data,DWORD bytes)
{GR_BRIDGE_KEY(KEY_SET_VALUE|KEY_CREATE_SUB_KEY);WCHAR path[1024];GR_BRIDGE_END(SHSetValueW(mappedKey,GRBackendSubkey(mappedKey,sub,path),name,type,data,bytes));}
static LSTATUS WINAPI GRBackendSHRegGetValueW(HKEY key,LPCWSTR sub,LPCWSTR name,SRRF flags,LPDWORD type,LPVOID data,LPDWORD bytes)
{GR_BRIDGE_KEY(KEY_QUERY_VALUE);WCHAR path[1024];GR_BRIDGE_END(SHRegGetValueW(mappedKey,GRBackendSubkey(mappedKey,sub,path),name,flags,type,data,bytes));}
#undef GR_BRIDGE_KEY
#undef GR_BRIDGE_END

static BOOL GRPatchBackendRegistry(HMODULE module)
{
    grCompareRegistryHandles=(GRCompareHandlesFn)GetProcAddress(GetModuleHandleW(L"kernelbase.dll"),"CompareObjectHandles");
    if(!grCompareRegistryHandles)grCompareRegistryHandles=(GRCompareHandlesFn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"CompareObjectHandles");
    if(!grCompareRegistryHandles)return FALSE;
    for(UINT i=0;i<ARRAYSIZE(grLegacyBackendKeys);++i)
        RegOpenKeyExW(HKEY_CURRENT_USER,grLegacySettingsKeys[i],0,KEY_READ,&grLegacyBackendKeys[i]);
    struct GRRegistryPatch {const char* name;uintptr_t replacement;} patches[]={
        {"RegOpenKeyExW",(uintptr_t)GRBackendRegOpenKeyExW},{"RegCreateKeyW",(uintptr_t)GRBackendRegCreateKeyW},
        {"RegCreateKeyExW",(uintptr_t)GRBackendRegCreateKeyExW},{"RegGetValueW",(uintptr_t)GRBackendRegGetValueW},
        {"RegQueryValueExW",(uintptr_t)GRBackendRegQueryValueExW},{"RegSetValueExW",(uintptr_t)GRBackendRegSetValueExW},
        {"RegDeleteValueW",(uintptr_t)GRBackendRegDeleteValueW},{"RegQueryInfoKeyW",(uintptr_t)GRBackendRegQueryInfoKeyW},
        {"RegEnumKeyExW",(uintptr_t)GRBackendRegEnumKeyExW},{"RegEnumValueW",(uintptr_t)GRBackendRegEnumValueW},
        {"RegNotifyChangeKeyValue",(uintptr_t)GRBackendRegNotifyChangeKeyValue},{"SHGetValueW",(uintptr_t)GRBackendSHGetValueW},
        {"SHSetValueW",(uintptr_t)GRBackendSHSetValueW},{"SHRegGetValueW",(uintptr_t)GRBackendSHRegGetValueW}
    };
    BYTE* base=(BYTE*)module;IMAGE_DOS_HEADER* dos=(IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* nt=(IMAGE_NT_HEADERS*)(base+dos->e_lfanew);
    DWORD rva=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if(!rva)return FALSE;
    BOOL complete=TRUE;UINT count=0;
    for(IMAGE_IMPORT_DESCRIPTOR* library=(IMAGE_IMPORT_DESCRIPTOR*)(base+rva);library->Name;++library){
        if(!library->OriginalFirstThunk){complete=FALSE;continue;}
        for(IMAGE_THUNK_DATA* thunk=(IMAGE_THUNK_DATA*)(base+library->OriginalFirstThunk);thunk->u1.AddressOfData;++thunk){
            if(IMAGE_SNAP_BY_ORDINAL(thunk->u1.Ordinal))continue;
            const char* name=(const char*)((IMAGE_IMPORT_BY_NAME*)(base+thunk->u1.AddressOfData))->Name;
            BOOL found=FALSE;
            for(UINT i=0;i<ARRAYSIZE(patches);++i)if(!strcmp(name,patches[i].name)){
                found=TRUE;++count;
                if(!VnPatchIAT(module,(PSTR)(base+library->Name),(PSTR)name,patches[i].replacement))complete=FALSE;
                break;
            }
            if(!found&&(!strncmp(name,"Reg",3)||!strncmp(name,"SHReg",5))&&strcmp(name,"RegCloseKey"))complete=FALSE;
        }
    }
    return complete&&count>0;
}
