#include "SettingsPaint.h"
#pragma once
#include <commdlg.h>
#include "CursorModel.h"
#pragma comment(lib, "comdlg32.lib")

typedef struct GRCursorChoice { WCHAR name[96], path[MAX_PATH]; BOOL system; } GRCursorChoice;
typedef struct GRCursorPanel {
    HWND schemes, roles, name, status, buttons[7];
    GRCursorChoice choices[256]; UINT count, dpi;
    GRCursorScheme current;
    GRCursorPreview previews[GR_CURSOR_ROLES];
    HFONT font; HBRUSH background;
} GRCursorPanel;

static COLORREF GRCursorBackground(void) { return IsHighContrast() ? GetSysColor(COLOR_WINDOW) : g_darkModeEnabled ? RGB(25,32,45) : RGB(255,255,255); }
static COLORREF GRCursorText(void) { return IsHighContrast() ? GetSysColor(COLOR_WINDOWTEXT) : g_darkModeEnabled ? RGB(234,241,252) : RGB(23,38,58); }

static void GRCursorPanelMessage(GRCursorPanel* panel, PCWSTR message) { SetWindowTextW(panel->status, message); }
static void GRCursorPanelLoadPreviews(HWND hwnd, GRCursorPanel* panel)
{
    static const UINT defaults[GR_CURSOR_ROLES] = {32512,32651,32650,32514,32515,32513,32631,32648,32645,32644,32642,32643,32646,32516,32649,32671,32672};
    SendMessageW(panel->roles, WM_SETREDRAW, FALSE, 0);
    SendMessageW(panel->roles, LB_RESETCONTENT, 0, 0);
    BOOL animated = FALSE;
    GRCursorScheme defaultPreview={0};
    GRCursorSystemScheme(L"Windows Aero",&defaultPreview);
    for (UINT i = 0; i < GR_CURSOR_ROLES; ++i) {
        GRCursorFreePreview(&panel->previews[i]);
        if (panel->current.paths[i][0]) GRCursorLoadPreview(panel->current.paths[i], &panel->previews[i]);
        if(!panel->previews[i].cursor && defaultPreview.paths[i][0])
            GRCursorLoadPreview(defaultPreview.paths[i],&panel->previews[i]);
        if(!panel->previews[i].cursor) {
            static const PCWSTR fallbackFiles[GR_CURSOR_ROLES]={L"aero_arrow.cur",L"aero_helpsel.cur",L"aero_working.ani",L"aero_busy.ani",L"cross_r.cur",L"beam_r.cur",L"aero_pen.cur",L"aero_unavail.cur",L"aero_ns.cur",L"aero_ew.cur",L"aero_nwse.cur",L"aero_nesw.cur",L"aero_move.cur",L"aero_up.cur",L"aero_link.cur",L"aero_pin.cur",L"aero_person.cur"};
            WCHAR nativePath[MAX_PATH];
            if(GetWindowsDirectoryW(nativePath,MAX_PATH)) {
                wcscat_s(nativePath,MAX_PATH,L"\\Cursors\\");wcscat_s(nativePath,MAX_PATH,fallbackFiles[i]);
                GRCursorLoadPreview(nativePath,&panel->previews[i]);
            }
        }
        if(!panel->previews[i].cursor) {
            panel->previews[i].cursor=LoadCursorW(NULL,MAKEINTRESOURCEW(defaults[i]));
            panel->previews[i].shared=TRUE;panel->previews[i].steps=1;
        }
        panel->previews[i].next = GetTickCount64() + panel->previews[i].delay[0];
        animated |= panel->previews[i].steps > 1;
        SendMessageW(panel->roles, LB_ADDSTRING, 0, (LPARAM)grCursorLabels[i]);
    }
    SendMessageW(panel->roles, LB_SETCURSEL, 0, 0);
    SendMessageW(panel->roles, WM_SETREDRAW, TRUE, 0);
    SetWindowTextW(panel->name, panel->current.name[0] ? panel->current.name : L"My cursor scheme");
    KillTimer(hwnd, 1);
    if (animated && IsWindowVisible(hwnd)) SetTimer(hwnd, 1, 16, NULL);
    InvalidateRect(hwnd, NULL, TRUE); InvalidateRect(panel->roles, NULL, TRUE);
}
static void GRCursorPanelAddChoice(GRCursorPanel* panel, PCWSTR name, PCWSTR path, BOOL system)
{
    if (panel->count >= ARRAYSIZE(panel->choices) || wcslen(name) >= 96 || (path && wcslen(path) >= MAX_PATH)) return;
    GRCursorChoice* item = &panel->choices[panel->count++];
    wcscpy_s(item->name, ARRAYSIZE(item->name), name); item->system = system;
    if (path) wcscpy_s(item->path, MAX_PATH, path);
    SendMessageW(panel->schemes, CB_ADDSTRING, 0, (LPARAM)name);
}
static void GRCursorPanelScan(GRCursorPanel* panel, PCWSTR directory)
{
    WCHAR pattern[MAX_PATH]; if (swprintf_s(pattern, MAX_PATH, L"%s\\*.grcursors", directory) < 0) return;
    WIN32_FIND_DATAW entry; HANDLE search = FindFirstFileW(pattern, &entry);
    if (search == INVALID_HANDLE_VALUE) return;
    do {
        if ((entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) || !_wcsicmp(entry.cFileName,L"Previous.grcursors")) continue;
        WCHAR path[MAX_PATH], name[96];
        if (swprintf_s(path, MAX_PATH, L"%s\\%s", directory, entry.cFileName) < 0) continue;
        GetPrivateProfileStringW(L"GR Taskbar Cursor Scheme",L"Name",entry.cFileName,name,ARRAYSIZE(name),path);
        GRCursorPanelAddChoice(panel,name,path,FALSE);
    } while (FindNextFileW(search, &entry));
    FindClose(search);
}
static void GRCursorPanelPopulate(GRCursorPanel* panel)
{
    panel->count = 0; ZeroMemory(panel->choices,sizeof(panel->choices));
    SendMessageW(panel->schemes,CB_RESETCONTENT,0,0);
    GRCursorPanelAddChoice(panel,L"Current Windows cursors",NULL,FALSE);
    HKEY key;
    if (!RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Control Panel\\Cursors\\Schemes",0,KEY_QUERY_VALUE,&key)) {
        for (DWORD index=0;;++index) {
            WCHAR name[96]; DWORD size=ARRAYSIZE(name);
            LSTATUS status=RegEnumValueW(key,index,name,&size,NULL,NULL,NULL,NULL);
            if (status==ERROR_NO_MORE_ITEMS) break;
            if (status==ERROR_SUCCESS) GRCursorPanelAddChoice(panel,name,NULL,TRUE);
        }
        RegCloseKey(key);
    }
    WCHAR path[MAX_PATH]; HMODULE module=NULL;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&GRCursorPanelPopulate,&module) && GetModuleFileNameW(module,path,MAX_PATH) &&
        PathRemoveFileSpecW(path) && PathAppendW(path,L"Cursors")) GRCursorPanelScan(panel,path);
    if (GRCursorUserDirectory(path)) GRCursorPanelScan(panel,path);
    SendMessageW(panel->schemes,CB_SETCURSEL,0,0);
}
static BOOL GRCursorPanelPickFile(HWND hwnd, WCHAR path[MAX_PATH], BOOL scheme)
{
    OPENFILENAMEW pick={sizeof(pick)}; path[0]=0;
    pick.hwndOwner=hwnd;pick.lpstrFile=path;pick.nMaxFile=MAX_PATH;
    pick.lpstrTitle=scheme?L"Import a GR Taskbar cursor scheme":L"Choose a static or animated cursor";
    pick.lpstrFilter=scheme?L"GR Taskbar schemes (*.grcursors)\0*.grcursors\0":L"Mouse cursors (*.cur;*.ani)\0*.cur;*.ani\0";
    pick.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_DONTADDTORECENT;
    return GetOpenFileNameW(&pick);
}
static void GRCursorPanelLayout(HWND hwnd,GRCursorPanel* panel)
{
    RECT r;GetClientRect(hwnd,&r);UINT dpi=GetDpiForWindow(hwnd);if(!dpi)dpi=96;
    if(panel->dpi!=dpi) {
        HFONT old=panel->font;
        panel->font=CreateFontW(-MulDiv(14,dpi,96),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_NATURAL_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        panel->dpi=dpi;
        HWND controls[]={panel->schemes,panel->roles,panel->name,panel->status};
        for(UINT i=0;i<ARRAYSIZE(controls);++i)SendMessageW(controls[i],WM_SETFONT,(WPARAM)panel->font,TRUE);
        for(UINT i=0;i<7;++i)SendMessageW(panel->buttons[i],WM_SETFONT,(WPARAM)panel->font,TRUE);
        if(old)DeleteObject(old);
    }
    int unit=MulDiv(8,dpi,96),row=MulDiv(30,dpi,96),right=MulDiv(156,dpi,96),bottom=MulDiv(178,dpi,96);
    MoveWindow(panel->schemes,0,0,r.right, row*10,TRUE);
    MoveWindow(panel->roles,0,row+unit,max(100,r.right-right-unit),max(80,r.bottom-bottom-row),TRUE);
    SendMessageW(panel->roles,LB_SETITEMHEIGHT,0,MulDiv(46,dpi,96));
    MoveWindow(panel->status,0,r.bottom-bottom+unit,r.right,row,TRUE);
    MoveWindow(panel->name,0,r.bottom-MulDiv(130,dpi,96),r.right,row,TRUE);
    for(UINT i=0;i<6;++i)MoveWindow(panel->buttons[i],(i%3)*(r.right+unit)/3,
        r.bottom-MulDiv(i<3?88:46,dpi,96),max(1,(r.right-2*unit)/3),row,TRUE);
    MoveWindow(panel->buttons[6],r.right-right,row+MulDiv(188,dpi,96),right,row,TRUE);
}
static void GRCursorPanelCommand(HWND hwnd,GRCursorPanel* panel,UINT id)
{
    WCHAR path[MAX_PATH],directory[MAX_PATH];
    if(id==201) {
        if(!GRCursorPanelPickFile(hwnd,path,TRUE))return;
        GRCursorScheme imported;
        if(!GRCursorReadScheme(path,&imported,TRUE)){GRCursorPanelMessage(panel,L"Import failed. Check the scheme and its .cur/.ani files.");return;}
        imported.snapshot=FALSE;panel->current=imported;
        GRCursorPanelLoadPreviews(hwnd,panel);
        GRCursorPanelMessage(panel,L"Imported into your permanent library. Save the scheme, then Apply.");
    } else if(id==202) {
        LRESULT role=SendMessageW(panel->roles,LB_GETCURSEL,0,0);if(role<0||role>=GR_CURSOR_ROLES)return;
        if(!GRCursorPanelPickFile(hwnd,path,FALSE))return;
        if(!GRCursorImportAsset(path,panel->current.paths[role])){GRCursorPanelMessage(panel,L"This cursor could not be decoded or copied.");return;}
        panel->current.snapshot=FALSE;
        GRCursorPanelLoadPreviews(hwnd,panel);SendMessageW(panel->roles,LB_SETCURSEL,role,0);
        GRCursorPanelMessage(panel,L"Cursor added. Save the scheme to keep it in the picker.");
    } else if(id==203) {
        GetWindowTextW(panel->name,panel->current.name,ARRAYSIZE(panel->current.name));
        if(!panel->current.name[0]||wcspbrk(panel->current.name,L"\\/:*?\"<>|\r\n")){GRCursorPanelMessage(panel,L"Enter a scheme name without file-name punctuation.");return;}
        if(!GRCursorUserDirectory(directory)||swprintf_s(path,MAX_PATH,L"%s\\%s.grcursors",directory,panel->current.name)<0)return;
        for(UINT i=0;i<GR_CURSOR_ROLES;++i)if(panel->current.paths[i][0]){
            WCHAR imported[MAX_PATH];if(!GRCursorImportAsset(panel->current.paths[i],imported)){GRCursorPanelMessage(panel,L"A cursor could not be saved to your library.");return;}
            wcscpy_s(panel->current.paths[i],MAX_PATH,imported);
        }
        panel->current.snapshot=FALSE;
        if(!GRCursorSaveScheme(path,&panel->current)){GRCursorPanelMessage(panel,L"Could not save the scheme.");return;}
        wcscpy_s(panel->current.file,MAX_PATH,path);GRCursorPanelPopulate(panel);
        GRCursorPanelMessage(panel,L"Scheme saved permanently in your GR Taskbar cursor library.");
    } else if(id==204) {
        GRCursorPanelMessage(panel,GRCursorApply(&panel->current)?L"Applied immediately. Pointer accessibility settings were preserved.":L"Apply failed; the previous Windows cursor values were restored.");
    } else if(id==205) {
        GRCursorScheme previous;
        if(GRCursorUserDirectory(directory)&&swprintf_s(path,MAX_PATH,L"%s\\Previous.grcursors",directory)>=0&&
            GRCursorReadScheme(path,&previous,FALSE)&&GRCursorApply(&previous)){
            panel->current=previous;GRCursorPanelLoadPreviews(hwnd,panel);GRCursorPanelMessage(panel,L"Previous Windows cursors restored.");
        }else GRCursorPanelMessage(panel,L"No usable previous cursor snapshot is available.");
    } else if(id==206) {
        GRCursorScheme defaults;
        if(GRCursorSystemScheme(L"Windows Aero",&defaults)&&GRCursorApply(&defaults)){
            panel->current=defaults;GRCursorPanelLoadPreviews(hwnd,panel);GRCursorPanelMessage(panel,L"Windows default cursors restored.");
        }else GRCursorPanelMessage(panel,L"Windows default scheme could not be restored.");
    } else if(id==207) {
        LRESULT index=SendMessageW(panel->schemes,CB_GETCURSEL,0,0);
        if(index<0||(UINT)index>=panel->count||!GRCursorUserDirectory(directory)||
            !PathIsPrefixW(directory,panel->choices[index].path)){GRCursorPanelMessage(panel,L"Select a saved user scheme to remove.");return;}
        WCHAR removed[MAX_PATH];swprintf_s(removed,MAX_PATH,L"%s\\Removed",directory);SHCreateDirectoryExW(hwnd,removed,NULL);
        if(swprintf_s(path,MAX_PATH,L"%s\\%llu.grcursors",removed,GetTickCount64())>=0&&MoveFileW(panel->choices[index].path,path)){
            GRCursorPanelPopulate(panel);GRCursorPanelMessage(panel,L"Moved to Removed. Active cursors and shared assets were kept.");
        }else GRCursorPanelMessage(panel,L"The saved scheme could not be moved.");
    }
}

static LRESULT CALLBACK GRCursorPanelProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
    GRCursorPanel* panel=(GRCursorPanel*)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    if(message==WM_CREATE){
        panel=(GRCursorPanel*)((CREATESTRUCTW*)lParam)->lpCreateParams;SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)panel);
        panel->background=CreateSolidBrush(GRCursorBackground());
        panel->schemes=CreateWindowW(L"COMBOBOX",L"Cursor scheme",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,0,0,0,0,hwnd,(HMENU)200,NULL,NULL);
        panel->roles=CreateWindowExW(WS_EX_CLIENTEDGE,L"LISTBOX",L"Cursor preview gallery",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS,0,0,0,0,hwnd,(HMENU)210,NULL,NULL);
        panel->name=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,hwnd,(HMENU)211,NULL,NULL);
        SendMessageW(panel->name,EM_SETCUEBANNER,TRUE,(LPARAM)L"Custom scheme name");SendMessageW(panel->name,EM_SETLIMITTEXT,90,0);
        panel->status=CreateWindowW(L"STATIC",L"Choose a scheme or import your own cursors.",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,hwnd,NULL,NULL,NULL);
        PCWSTR labels[]={L"Import scheme...",L"Change cursor...",L"Save scheme",L"Apply",L"Restore previous",L"Windows default",L"Remove saved"};
        for(UINT i=0;i<7;++i)panel->buttons[i]=CreateWindowW(L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,0,0,0,0,hwnd,(HMENU)(UINT_PTR)(201+i),NULL,NULL);
        GRCursorPanelPopulate(panel);GRCursorReadCurrent(&panel->current);GRCursorPanelLoadPreviews(hwnd,panel);GRCursorPanelLayout(hwnd,panel);return 0;
    }
    if(!panel)return DefWindowProcW(hwnd,message,wParam,lParam);
    if(message==WM_SIZE){GRCursorPanelLayout(hwnd,panel);return 0;}
    if(message==WM_COMMAND){
        if(LOWORD(wParam)==200&&HIWORD(wParam)==CBN_SELCHANGE){
            LRESULT index=SendMessageW(panel->schemes,CB_GETCURSEL,0,0);
            if(index>=0&&(UINT)index<panel->count){GRCursorChoice* choice=&panel->choices[index];BOOL ok;
                if(!index)ok=GRCursorReadCurrent(&panel->current);
                else if(choice->system)ok=GRCursorSystemScheme(choice->name,&panel->current);
                else ok=GRCursorReadScheme(choice->path,&panel->current,FALSE);
                if(ok)GRCursorPanelLoadPreviews(hwnd,panel);else GRCursorPanelMessage(panel,L"This scheme contains a missing or invalid cursor.");
            }
        }else if(LOWORD(wParam)==210)InvalidateRect(hwnd,NULL,FALSE);
        else if(HIWORD(wParam)==BN_CLICKED)GRCursorPanelCommand(hwnd,panel,LOWORD(wParam));
        return 0;
    }
    if(message==WM_DRAWITEM){
        DRAWITEMSTRUCT* draw=(DRAWITEMSTRUCT*)lParam;if(draw->CtlID!=210||draw->itemID>=GR_CURSOR_ROLES)return 0;
        DRAWITEMSTRUCT bufferedItem=*draw;draw=&bufferedItem;
        GRSettingsPaint itemPaint;draw->hDC=GRSettingsPaintBegin(&itemPaint,draw->hDC,draw->rcItem);
        int savedDC=SaveDC(draw->hDC);
        BOOL selected=(draw->itemState&ODS_SELECTED)!=0;
        HBRUSH brush=CreateSolidBrush(selected?(g_darkModeEnabled?RGB(37,61,83):RGB(211,231,247)):GRCursorBackground());
        FillRect(draw->hDC,&draw->rcItem,brush);DeleteObject(brush);SetBkMode(draw->hDC,TRANSPARENT);SetTextColor(draw->hDC,GRCursorText());
        UINT role=draw->itemID;GRCursorPreview* p=&panel->previews[role];int icon=MulDiv(28,panel->dpi,96),pad=MulDiv(6,panel->dpi,96);
        if(p->cursor)DrawIconEx(draw->hDC,draw->rcItem.left+pad,draw->rcItem.top+pad,(HICON)p->cursor,icon,icon,p->frame[p->step],NULL,DI_NORMAL);
        RECT text=draw->rcItem;text.left+=icon+3*pad;SelectObject(draw->hDC,panel->font);
        DrawTextW(draw->hDC,grCursorLabels[role],-1,&text,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
        if(draw->itemState&ODS_FOCUS)DrawFocusRect(draw->hDC,&draw->rcItem);RestoreDC(draw->hDC,savedDC);GRSettingsPaintEnd(&itemPaint);return TRUE;
    }
    if(message==WM_TIMER&&wParam==1){
        ULONGLONG now=GetTickCount64();BOOL changed=FALSE;
        for(UINT i=0;i<GR_CURSOR_ROLES;++i){GRCursorPreview* p=&panel->previews[i];
            if(p->steps>1&&now>=p->next){p->step=(p->step+1)%p->steps;p->next=now+p->delay[p->step];changed=TRUE;
                RECT item; if(SendMessageW(panel->roles,LB_GETITEMRECT,i,(LPARAM)&item)!=LB_ERR) InvalidateRect(panel->roles,&item,FALSE);}}
        if(changed){RECT preview;GetClientRect(hwnd,&preview);preview.left=max(0,preview.right-MulDiv(156,panel->dpi,96));preview.top=MulDiv(42,panel->dpi,96);preview.bottom=preview.top+MulDiv(170,panel->dpi,96);InvalidateRect(hwnd,&preview,FALSE);}return 0;
    }
    if(message==WM_SHOWWINDOW){if(!wParam)KillTimer(hwnd,1);else for(UINT i=0;i<GR_CURSOR_ROLES;++i)if(panel->previews[i].steps>1){SetTimer(hwnd,1,16,NULL);break;}}
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_PAINT){
        PAINTSTRUCT paint;HDC dc=BeginPaint(hwnd,&paint);RECT r;GetClientRect(hwnd,&r);FillRect(dc,&r,panel->background);
        LRESULT role=SendMessageW(panel->roles,LB_GETCURSEL,0,0);if(role>=0&&role<GR_CURSOR_ROLES){
            int right=MulDiv(156,panel->dpi,96),top=MulDiv(42,panel->dpi,96),size=MulDiv(64,panel->dpi,96);
            GRCursorPreview* p=&panel->previews[role];if(p->cursor)DrawIconEx(dc,r.right-right+(right-size)/2,top+MulDiv(20,panel->dpi,96),(HICON)p->cursor,size,size,p->frame[p->step],NULL,DI_NORMAL);
            RECT text={r.right-right,top+size+MulDiv(40,panel->dpi,96),r.right,top+MulDiv(178,panel->dpi,96)};
            SetBkMode(dc,TRANSPARENT);SetTextColor(dc,GRCursorText());SelectObject(dc,panel->font);
            DrawTextW(dc,grCursorLabels[role],-1,&text,DT_CENTER|DT_WORDBREAK);
        }EndPaint(hwnd,&paint);return 0;
    }
    if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLORLISTBOX||message==WM_CTLCOLOREDIT){SetTextColor((HDC)wParam,GRCursorText());SetBkColor((HDC)wParam,GRCursorBackground());return (LRESULT)panel->background;}
    if(message==WM_THEMECHANGED){DeleteObject(panel->background);panel->background=CreateSolidBrush(GRCursorBackground());InvalidateRect(hwnd,NULL,TRUE);}
    if(message==WM_NCDESTROY){KillTimer(hwnd,1);for(UINT i=0;i<GR_CURSOR_ROLES;++i)GRCursorFreePreview(&panel->previews[i]);if(panel->font)DeleteObject(panel->font);DeleteObject(panel->background);free(panel);SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);}
    return DefWindowProcW(hwnd,message,wParam,lParam);
}
static HWND CreateEmbeddedCursorSettings(HWND owner)
{
    WNDCLASSW window={0};window.lpfnWndProc=GRCursorPanelProc;window.hInstance=GetModuleHandleW(NULL);window.lpszClassName=L"GRTaskbar_MouseCursors";window.hCursor=LoadCursorW(NULL,IDC_ARROW);
    if(!RegisterClassW(&window)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return NULL;
    GRCursorPanel* panel=(GRCursorPanel*)calloc(1,sizeof(*panel));if(!panel)return NULL;
    return CreateWindowExW(WS_EX_CONTROLPARENT,window.lpszClassName,L"Mouse cursors",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,600,480,owner,NULL,window.hInstance,panel);
}
