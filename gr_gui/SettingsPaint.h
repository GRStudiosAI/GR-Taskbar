#pragma once
#include <windows.h>

// Paint an owner-drawn control atomically. Keep fonts, clipping and background
// state private to the item instead of leaking them into another control's DC.
typedef struct GRSettingsPaint {
    HDC target, dc;
    HBITMAP bitmap;
    HGDIOBJ previous;
    RECT rect;
} GRSettingsPaint;

static HDC GRSettingsPaintBegin(GRSettingsPaint* paint, HDC target, RECT rect)
{
    ZeroMemory(paint,sizeof(*paint)); paint->target=target; paint->rect=rect;
    int width=rect.right-rect.left, height=rect.bottom-rect.top;
    if(width<=0||height<=0) return target;
    paint->dc=CreateCompatibleDC(target);
    BITMAPINFO info={0}; info.bmiHeader.biSize=sizeof(info.bmiHeader);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* pixels=NULL;paint->bitmap=CreateDIBSection(target,&info,DIB_RGB_COLORS,&pixels,NULL,0);
    if(!paint->dc||!paint->bitmap) {
        if(paint->dc)DeleteDC(paint->dc);
        if(paint->bitmap)DeleteObject(paint->bitmap);
        paint->dc=NULL;paint->bitmap=NULL;return target;
    }
    paint->previous=SelectObject(paint->dc,paint->bitmap);
    SetViewportOrgEx(paint->dc,-rect.left,-rect.top,NULL);
    SelectObject(paint->dc,GetCurrentObject(target,OBJ_FONT));
    return paint->dc;
}

static void GRSettingsPaintEnd(GRSettingsPaint* paint)
{
    if(!paint->dc) return;
    BitBlt(paint->target,paint->rect.left,paint->rect.top,
        paint->rect.right-paint->rect.left,paint->rect.bottom-paint->rect.top,
        paint->dc,paint->rect.left,paint->rect.top,SRCCOPY);
    SelectObject(paint->dc,paint->previous); DeleteObject(paint->bitmap);DeleteDC(paint->dc);
}
