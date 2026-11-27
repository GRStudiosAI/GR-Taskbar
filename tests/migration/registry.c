#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlwapi.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#pragma comment(lib,"advapi32.lib")
#pragma comment(lib,"shlwapi.lib")
static BOOL VnPatchIAT(HMODULE module,PSTR library,PSTR name,uintptr_t replacement){(void)module;(void)library;(void)name;(void)replacement;return FALSE;}
#include "../../GR-Taskbar/LegacyRegistryBridge.h"
static void SetNumber(HKEY root,PCWSTR sub,PCWSTR name,DWORD value){assert(!RegSetKeyValueW(root,sub,name,REG_DWORD,&value,4));}
static DWORD Number(PCWSTR sub,PCWSTR name){DWORD value=0,bytes=4;assert(!RegGetValueW(HKEY_CURRENT_USER,sub,name,RRF_RT_REG_DWORD,NULL,&value,&bytes));return value;}
int wmain(void){
 WCHAR testKey[160];swprintf_s(testKey,ARRAYSIZE(testKey),L"Software\\GR-Taskbar-Tests\\Migration-%lu-%llu",GetCurrentProcessId(),GetTickCount64());
 HKEY isolated;assert(!RegCreateKeyExW(HKEY_CURRENT_USER,testKey,0,NULL,0,KEY_ALL_ACCESS,NULL,&isolated,NULL));
 assert(!RegOverridePredefKey(HKEY_CURRENT_USER,isolated));assert(!RegOverridePredefKey(HKEY_LOCAL_MACHINE,isolated));
 HKEY uninstall;assert(!RegCreateKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",0,NULL,0,KEY_ALL_ACCESS,NULL,&uninstall,NULL));RegCloseKey(uninstall);
 SetNumber(HKEY_CURRENT_USER,grLegacySettingsKeys[0],L"OrbStyle",3);SetNumber(HKEY_CURRENT_USER,grLegacySettingsKeys[0],L"UsefulSetting",72);
 SetNumber(HKEY_CURRENT_USER,L"Software\\ExplorerPatcher\\sws",L"Theme",2);
 SetNumber(HKEY_CURRENT_USER,grMigratedSettingsKeys[0],L"OrbStyle",1);
 const WCHAR website[]=L"https://untrusted.invalid/installer.exe",unicode[]=L"GR \x732b theme";
 assert(!RegSetKeyValueW(HKEY_CURRENT_USER,grLegacySettingsKeys[0],L"UpdateURL",REG_SZ,website,sizeof(website)));
 assert(!RegSetKeyValueW(HKEY_CURRENT_USER,grLegacySettingsKeys[0],L"ThemeName",REG_SZ,unicode,sizeof(unicode)));
 SetNumber(HKEY_CURRENT_USER,grLegacySettingsKeys[1],L"XamlSounds",1);
 SetNumber(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",L"NativeSetting",123);
 assert(GRMigrateProductSettings(FALSE));assert(Number(grMigratedSettingsKeys[0],L"OrbStyle")==1);assert(Number(grMigratedSettingsKeys[0],L"UsefulSetting")==72);
 DWORD bytes=0;assert(RegGetValueW(HKEY_CURRENT_USER,grMigratedSettingsKeys[0],L"UpdateURL",RRF_RT_ANY,NULL,NULL,&bytes)==ERROR_FILE_NOT_FOUND);
 assert(Number(L"Software\\GR-Taskbar\\StartUI",L"XamlSounds")==1);puts("PASS copy missing preferences, preserve current choices and reject old update routes");
 HKEY source,backup;assert(!RegOpenKeyExW(HKEY_CURRENT_USER,grLegacySettingsKeys[0],0,KEY_READ,&source));
 assert(!RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\GR-Taskbar\\MigrationBackup\\Source0",0,KEY_READ,&backup));
 assert(!GRMigrationVerifyBackup(source,backup,0));RegCloseKey(backup);grLegacyBackendKeys[0]=source;
 grCompareRegistryHandles=(GRCompareHandlesFn)GetProcAddress(GetModuleHandleW(L"kernelbase.dll"),"CompareObjectHandles");assert(grCompareRegistryHandles);
 assert(GRMigrateProductSettings(TRUE));HKEY missing;assert(RegOpenKeyExW(HKEY_CURRENT_USER,grLegacySettingsKeys[0],0,KEY_READ,&missing)==ERROR_FILE_NOT_FOUND);
 DWORD value=0;bytes=4;assert(!GRBackendRegQueryValueExW(source,L"OrbStyle",NULL,NULL,(BYTE*)&value,&bytes));assert(value==1);
 value=99;assert(!GRBackendRegSetValueExW(source,L"CachedHandleWrite",0,REG_DWORD,(BYTE*)&value,4));assert(Number(grMigratedSettingsKeys[0],L"CachedHandleWrite")==99);
 assert(!GRBackendRegOpenKeyExW(source,L"sws",0,KEY_READ,&missing));value=0;bytes=4;assert(!RegQueryValueExW(missing,L"Theme",NULL,NULL,(BYTE*)&value,&bytes));assert(value==2);RegCloseKey(missing);
 puts("PASS verified old namespace cleanup; cached deleted-key handles read and write the GR namespace");
 WCHAR translated[1024];PCWSTR native=L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";
 assert(GRMapLegacyProductKey(native,translated)==native);PCWSTR similar=L"Software\\ExplorerPatcherOther";assert(GRMapLegacyProductKey(similar,translated)==similar);
 assert(!wcscmp(GRMapLegacyProductKey(L"SOFTWARE\\EXPLORERPATCHER\\sws",translated),L"Software\\GR-Taskbar\\sws"));
 assert(Number(native,L"NativeSetting")==123);puts("PASS exact product boundaries and native Windows settings preserved");
 SetNumber(HKEY_CURRENT_USER,grLegacySettingsKeys[0],L"NewLegacyValue",4);
 const WCHAR oldName[]=L"ExplorerPatcher",oldLocation[]=L"C:\\Program Files\\SeparateLegacyInstall";
 assert(!RegSetKeyValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Separate",L"DisplayName",REG_SZ,oldName,sizeof(oldName)));
 assert(!RegSetKeyValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Separate",L"InstallLocation",REG_SZ,oldLocation,sizeof(oldLocation)));
 assert(GRMigrateProductSettings(TRUE));assert(!RegOpenKeyExW(HKEY_CURRENT_USER,grLegacySettingsKeys[0],0,KEY_READ,&missing));RegCloseKey(missing);
 assert(Number(L"Software\\GR-Taskbar\\Migration",L"CleanupPending")==1);puts("PASS separate legacy installation prevents destructive cleanup");
 RegCloseKey(source);grLegacyBackendKeys[0]=NULL;
 assert(!RegOverridePredefKey(HKEY_LOCAL_MACHINE,NULL));assert(!RegOverridePredefKey(HKEY_CURRENT_USER,NULL));RegCloseKey(isolated);assert(!RegDeleteTreeW(HKEY_CURRENT_USER,testKey));
 puts("All migration checks passed in an isolated registry view. No live settings migrated.");return 0;
}
