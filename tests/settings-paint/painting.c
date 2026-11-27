#include <windows.h>
#include <stdio.h>
#include "../../gr_gui/SettingsPaint.h"
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"user32.lib")
int main(void){
 int invisible=0;
 HDC screen=GetDC(NULL),target=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,200,100);HGDIOBJ old=SelectObject(target,bitmap);ReleaseDC(NULL,screen);
 RECT whole={0,0,200,100},item={20,20,180,80};HBRUSH outside=CreateSolidBrush(RGB(255,0,255)),inside=CreateSolidBrush(RGB(25,32,45));FillRect(target,&whole,outside);
 DWORD baseline=0;HGDIOBJ font=GetCurrentObject(target,OBJ_FONT);COLORREF color=GetTextColor(target);
 for(int i=0;i<1001;++i){GRSettingsPaint paint;HDC dc=GRSettingsPaintBegin(&paint,target,item);FillRect(dc,&item,inside);SetTextColor(dc,RGB(255,255,255));SetBkMode(dc,TRANSPARENT);DrawTextW(dc,L"Mouse cursors and Start Icons",-1,&item,DT_CENTER|DT_SINGLELINE|DT_VCENTER);GRSettingsPaintEnd(&paint);if(!i)baseline=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);}
 for(int id=32513;id<=32515;++id){
 GRSettingsPaint paint;HDC dc=GRSettingsPaintBegin(&paint,target,item);FillRect(dc,&item,inside);
 HCURSOR cursor=(HCURSOR)LoadImageW(NULL,L"C:\\Windows\\Cursors\\cross_r.cur",IMAGE_CURSOR,32,32,LR_LOADFROMFILE);DrawIconEx(dc,30,30,(HICON)cursor,32,32,0,NULL,DI_NORMAL);GRSettingsPaintEnd(&paint);
 int visible=0;for(int y=30;y<62;++y)for(int x=30;x<62;++x)if(GetPixel(target,x,y)!=RGB(25,32,45))++visible;
 if(!visible)++invisible;printf("Cursor preview %d: %d visible pixels\n",id,visible);if(cursor)DestroyCursor(cursor);
 }
 DWORD after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);int failed=invisible||after!=baseline||GetCurrentObject(target,OBJ_FONT)!=font||GetTextColor(target)!=color||GetPixel(target,0,0)!=RGB(255,0,255)||GetPixel(target,199,99)!=RGB(255,0,255)||GetPixel(target,21,21)!=RGB(25,32,45);
 printf("1000 buffered settings row paints: %s; GDI handles %lu -> %lu; drawing bounds and target state preserved\n",failed?"FAIL":"PASS",baseline,after);
 SelectObject(target,old);DeleteObject(bitmap);DeleteObject(outside);DeleteObject(inside);DeleteDC(target);return failed;
}
