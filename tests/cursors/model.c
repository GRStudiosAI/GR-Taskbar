#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <assert.h>
#include <stdio.h>
#include <wchar.h>
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"advapi32.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"shlwapi.lib")
static WCHAR testRoot[MAX_PATH];static int reloads,failWrite;
static BOOL WINAPI TestSPI(UINT action,UINT value,PVOID data,UINT flags){assert(action==SPI_SETCURSORS);assert(!value&&!data);assert(flags==(SPIF_SENDCHANGE|SPIF_UPDATEINIFILE));++reloads;return TRUE;}
static HRESULT WINAPI TestFolder(HWND hwnd,int folder,HANDLE token,DWORD flags,LPWSTR path){(void)hwnd;(void)folder;(void)token;(void)flags;wcscpy_s(path,MAX_PATH,testRoot);return S_OK;}
static LSTATUS WINAPI TestWrite(HKEY key,LPCWSTR name,DWORD reserved,DWORD type,const BYTE* data,DWORD size){if(failWrite>0&&!--failWrite)return ERROR_ACCESS_DENIED;return RegSetValueExW(key,name,reserved,type,data,size);}
#define SystemParametersInfoW TestSPI
#define SHGetFolderPathW TestFolder
#define RegSetValueExW TestWrite
#include "../../gr_gui/CursorModel.h"
#undef RegSetValueExW

int wmain(void){
 WCHAR temp[MAX_PATH],keyName[160];assert(GetTempPathW(MAX_PATH,temp));
 swprintf_s(testRoot,MAX_PATH,L"%sGR-Cursors-Test-%lu-%llu",temp,GetCurrentProcessId(),GetTickCount64());assert(CreateDirectoryW(testRoot,NULL));
 swprintf_s(keyName,ARRAYSIZE(keyName),L"Software\\GR-Taskbar-Tests\\Cursor-%lu-%llu",GetCurrentProcessId(),GetTickCount64());
 HKEY isolated;assert(!RegCreateKeyExW(HKEY_CURRENT_USER,keyName,0,NULL,0,KEY_ALL_ACCESS,NULL,&isolated,NULL));
 assert(!RegOverridePredefKey(HKEY_CURRENT_USER,isolated));
 HKEY windows;assert(!RegCreateKeyExW(HKEY_CURRENT_USER,L"Control Panel\\Cursors",0,NULL,0,KEY_ALL_ACCESS,NULL,&windows,NULL));
 const WCHAR original[]=L"%SystemRoot%\\Cursors\\aero_arrow.cur",originalName[]=L"Original scheme";
 assert(!RegSetValueExW(windows,L"Arrow",0,REG_EXPAND_SZ,(BYTE*)original,sizeof(original)));
 assert(!RegSetValueExW(windows,NULL,0,REG_SZ,(BYTE*)originalName,sizeof(originalName)));
 DWORD accessibility=64;assert(!RegSetValueExW(windows,L"CursorBaseSize",0,REG_DWORD,(BYTE*)&accessibility,4));RegCloseKey(windows);
 WCHAR arrow[MAX_PATH],busy[MAX_PATH];GetWindowsDirectoryW(arrow,MAX_PATH);wcscat_s(arrow,MAX_PATH,L"\\Cursors\\aero_arrow.cur");
 GetWindowsDirectoryW(busy,MAX_PATH);wcscat_s(busy,MAX_PATH,L"\\Cursors\\aero_busy.ani");
 GRCursorPreview preview;assert(GRCursorLoadPreview(arrow,&preview));assert(preview.steps==1);GRCursorFreePreview(&preview);
 assert(GRCursorLoadPreview(busy,&preview));assert(preview.steps>1);UINT steps=preview.steps;GRCursorFreePreview(&preview);printf("PASS static cursor and animated cursor (%u steps) decode\n",steps);
 DWORD handles=GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
 for(int i=0;i<100;++i){assert(GRCursorLoadPreview(busy,&preview));GRCursorFreePreview(&preview);}assert(GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS)==handles);puts("PASS repeated preview handles released");
 WCHAR stored[MAX_PATH],again[MAX_PATH];assert(GRCursorImportAsset(arrow,stored));assert(GRCursorImportAsset(arrow,again));assert(!wcscmp(stored,again));assert(GetFileAttributesW(arrow)!=INVALID_FILE_ATTRIBUTES);puts("PASS permanent imports deduplicate without moving the original");
 GRCursorScheme scheme={0};wcscpy_s(scheme.name,ARRAYSIZE(scheme.name),L"GR \x732b scheme");wcscpy_s(scheme.paths[0],MAX_PATH,stored);
 assert(GRCursorImportAsset(busy,scheme.paths[3]));WCHAR directory[MAX_PATH],saved[MAX_PATH];assert(GRCursorUserDirectory(directory));swprintf_s(saved,MAX_PATH,L"%s\\test.grcursors",directory);
 assert(GRCursorSaveScheme(saved,&scheme));GRCursorScheme loaded;assert(GRCursorReadScheme(saved,&loaded,FALSE));assert(!wcscmp(loaded.name,scheme.name));assert(!wcscmp(loaded.paths[3],scheme.paths[3]));puts("PASS Unicode saved scheme round trip");
 failWrite=3;assert(!GRCursorApply(&scheme));GRCursorScheme current;assert(GRCursorReadCurrent(&current));assert(current.types[0]==REG_EXPAND_SZ);assert(!wcscmp(current.paths[0],original));assert(!current.present[14]);puts("PASS injected write failure restores exact role type and missing values");
 failWrite=0;assert(GRCursorApply(&scheme));assert(reloads>=2);DWORD bytes=4,actual=0;assert(!RegGetValueW(HKEY_CURRENT_USER,L"Control Panel\\Cursors",L"CursorBaseSize",RRF_RT_REG_DWORD,NULL,&actual,&bytes));assert(actual==64);puts("PASS immediate reload requested; accessibility size preserved (system reload mocked)");
 swprintf_s(saved,MAX_PATH,L"%s\\Previous.grcursors",directory);GRCursorScheme previous;assert(GRCursorReadScheme(saved,&previous,FALSE));assert(previous.snapshot);assert(GRCursorApply(&previous));assert(GRCursorReadCurrent(&current));assert(!current.present[14]&&current.types[0]==REG_EXPAND_SZ&&!wcscmp(current.paths[0],original));puts("PASS persistent restore snapshot preserves missing and expandable role values");
 wcscpy_s(scheme.paths[4],MAX_PATH,L"Z:\\missing\\invalid.cur");int before=reloads;assert(!GRCursorApply(&scheme));assert(before==reloads);puts("PASS missing cursor rejected before changing Windows roles");
 WCHAR malformed[MAX_PATH];swprintf_s(malformed,MAX_PATH,L"%s\\bad.ani",testRoot);HANDLE file=CreateFileW(malformed,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);assert(file!=INVALID_HANDLE_VALUE);DWORD written;WriteFile(file,"RIFFxxxxACON",12,&written,NULL);CloseHandle(file);assert(!GRCursorLoadPreview(malformed,&preview));puts("PASS truncated animation rejected");
 assert(!RegOverridePredefKey(HKEY_CURRENT_USER,NULL));RegCloseKey(isolated);assert(!RegDeleteTreeW(HKEY_CURRENT_USER,keyName));
 printf("All cursor model checks passed. No live Windows cursor changes. Fixtures: %ls\n",testRoot);return 0;
}
