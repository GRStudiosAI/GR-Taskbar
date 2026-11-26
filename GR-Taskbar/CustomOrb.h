#pragma once

#include <wincodec.h>
#include "StartIconSizing.h"
#pragma comment(lib, "windowscodecs.lib")

#define ORB_STYLE_CUSTOM 3
#define ORB_STYLE_WINDOWS7 4
#define ORB_STYLE_WINDOWSXP 5
#define CUSTOM_ORB_IMAGE_VALUE L"CustomOrbImage"

typedef struct CustomOrbImage
{
    HBITMAP bitmap;
    UINT width;
    UINT frameHeight;
    RECT contentBounds; // One shared alpha bound for all states; prevents jitter.
} CustomOrbImage;

typedef struct GRStartIconRenderCache
{
    HBITMAP bitmap;
    int width;
    int height;
} GRStartIconRenderCache;

static void GRClearStartIconCache(GRStartIconRenderCache* cache)
{
    if (cache->bitmap) DeleteObject(cache->bitmap);
    ZeroMemory(cache, sizeof(*cache));
}

static BOOL GRMeasureOrbContent(CustomOrbImage* image)
{
    BITMAP bitmap = { 0 };
    if (!image->bitmap || GetObjectW(image->bitmap, (int)sizeof(bitmap), &bitmap) != (int)sizeof(bitmap) || !bitmap.bmBits) return FALSE;
    RECT bounds = { (LONG)image->width, (LONG)image->frameHeight, 0, 0 };
    for (UINT frame = 0; frame < 3; ++frame)
        for (UINT y = 0; y < image->frameHeight; ++y)
        {
            const DWORD* row = (const DWORD*)((const BYTE*)bitmap.bmBits + (frame * image->frameHeight + y) * bitmap.bmWidthBytes);
            for (UINT x = 0; x < image->width; ++x)
                if (row[x] >> 24)
                {
                    bounds.left = min(bounds.left, (LONG)x);
                    bounds.top = min(bounds.top, (LONG)y);
                    bounds.right = max(bounds.right, (LONG)x + 1);
                    bounds.bottom = max(bounds.bottom, (LONG)y + 1);
                }
        }
    image->contentBounds = bounds;
    return bounds.right > bounds.left && bounds.bottom > bounds.top;
}

// Built-in button artwork is independent of the taskbar and Start menu styles.
// Render directly into premultiplied pixels so both previews and the taskbar
// use the same transparent artwork without depending on the current theme.
static BOOL CreateBuiltinOrbImage(DWORD style, CustomOrbImage* image)
{
    BITMAPINFO info = { 0 };
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 256;
    info.bmiHeader.biHeight = -768;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    DWORD* pixels = NULL;
    HBITMAP bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);
    if (!bitmap) return FALSE;
    ZeroMemory(pixels, 256 * 768 * sizeof(DWORD));
    for (int frame = 0; frame < 3; ++frame)
    {
        for (int y = 0; y < 256; ++y)
        {
            for (int x = 0; x < 256; ++x)
            {
                DWORD color = 0;
                double px = x / 4.0 - (frame == 2), py = y / 4.0 - (frame == 2);
                if (style == ORB_STYLE_WINDOWS7 || style == ORB_STYLE_WINDOWSXP)
                {
                    double radius = (px - 32) * (px - 32) + (py - 32) * (py - 32);
                    if (style == ORB_STYLE_WINDOWS7 && radius < 29 * 29)
                    {
                        BYTE glow = (BYTE)(45 + (64 - py) * (frame == 1 ? 2 : 1));
                        color = 0xff000000 | ((DWORD)(glow / 5) << 16) | ((DWORD)(glow / 2) << 8) | glow;
                        if (radius > 27 * 27) color = frame == 1 ? 0xff8cddff : 0xff5597bf;
                    }
                    double wave = px < 32 ? (px - 14) / 6 : (50 - px) / 6;
                    double wy = py - wave;
                    if (((px >= 15 && px < 30) || (px >= 33 && px < 49)) &&
                        ((wy >= 17 && wy < 31) || (wy >= 34 && wy < 48)))
                        color = wy < 31 ? (px < 32 ? 0xffff633e : 0xff78c541) :
                            (px < 32 ? 0xff35a7ed : 0xffffd54b);
                }
                else
                {
                    double top = style == 0 ? 18 - (px - 14) / 6 : 14;
                    double bottom = style == 0 ? 46 + (px - 14) / 6 : 50;
                    if (((px >= 14 && px < 30) || (px >= 34 && px < 50)) &&
                        py >= top && py < bottom && (py < 30 || py >= 34))
                        color = frame == 1 ? 0xff64d8ff : frame == 2 ? 0xff158ccc : style == 1 ? 0xff149de0 : 0xffe6f6ff;
                }
                if (style == ORB_STYLE_WINDOWSXP && frame == 1 && color)
                    color = 0xff000000 | (min(255, ((color >> 16) & 255) + 24) << 16) |
                        (min(255, ((color >> 8) & 255) + 24) << 8) | min(255, (color & 255) + 24);
                pixels[(frame * 256 + y) * 256 + x] = color;
            }
        }
    }
    image->bitmap = bitmap;
    image->width = 256;
    image->frameHeight = 256;
    return GRMeasureOrbContent(image);
}

// Select the highest-resolution usable ICO frame, not the first (often 16px).
static BOOL GRBestWicFrame(IWICBitmapDecoder* decoder, IWICBitmapFrameDecode** selected)
{
    UINT count = 0; ULONGLONG largest = 0;
    *selected = NULL;
    if (FAILED(decoder->lpVtbl->GetFrameCount(decoder, &count))) return FALSE;
    for (UINT i = 0; i < min(count, 256); ++i)
    {
        IWICBitmapFrameDecode* candidate = NULL;
        UINT width = 0, height = 0;
        if (SUCCEEDED(decoder->lpVtbl->GetFrame(decoder, i, &candidate)))
        {
            if (SUCCEEDED(candidate->lpVtbl->GetSize(candidate, &width, &height)) &&
                width && width <= 4096 && height && height <= 12288 &&
                (height == width || (height >= width * 2 && height >= 3)) && (ULONGLONG)width * height > largest)
            {
                if (*selected) (*selected)->lpVtbl->Release(*selected);
                *selected = candidate; candidate = NULL;
                largest = (ULONGLONG)width * height;
            }
            if (candidate) candidate->lpVtbl->Release(candidate);
        }
    }
    return *selected != NULL;
}

// Many existing orb BMPs store real alpha in the reserved fourth byte of a
// 32-bit BI_RGB DIB. WIC's BMP decoder treats that format as opaque BGRX.
// Preserve that artwork through a native DIB and an explicitly alpha-aware
// WIC source. Ordinary 24-bit/BGRX bitmaps continue through the normal decoder.
static IWICBitmap* GRLegacyBmpAlphaSource(IWICImagingFactory* factory, PCWSTR path, UINT width, UINT height)
{
    if ((ULONGLONG)width * height > 16 * 1024 * 1024) return NULL;
    HBITMAP native = (HBITMAP)LoadImageW(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (!native) return NULL;
    DIBSECTION dib = { 0 }; IWICBitmap* source = NULL;
    if (GetObjectW(native, sizeof(dib), &dib) == sizeof(dib) && dib.dsBm.bmBits &&
        dib.dsBm.bmBitsPixel == 32 && dib.dsBmih.biCompression == BI_RGB &&
        dib.dsBm.bmWidth == (LONG)width && dib.dsBm.bmHeight == (LONG)height) {
        BOOL hasAlpha = FALSE, hasTransparency = FALSE;
        for (UINT y = 0; y < height; ++y) {
            const DWORD* row = (const DWORD*)((const BYTE*)dib.dsBm.bmBits + y * dib.dsBm.bmWidthBytes);
            for (UINT x = 0; x < width; ++x) {
                BYTE alpha = (BYTE)(row[x] >> 24);
                hasAlpha |= alpha != 0; hasTransparency |= alpha != 255;
            }
        }
        if (hasAlpha && hasTransparency)
            factory->lpVtbl->CreateBitmapFromHBITMAP(factory, native, NULL, WICBitmapUseAlpha, &source);
    }
    DeleteObject(native);
    return source;
}

// Decode high-resolution artwork to premultiplied BGRA, retaining all states.
// Bound source storage to 1024px per frame dimension; this is comfortably above
// the maximum 48-logical-pixel icon at supported desktop scaling factors.
static BOOL LoadCustomOrbImage(PCWSTR path, CustomOrbImage* image)
{
    ZeroMemory(image, sizeof(*image));
    HRESULT apartment = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(apartment) && apartment != RPC_E_CHANGED_MODE) return FALSE;
    IWICImagingFactory* factory = NULL;
    IWICBitmapDecoder* decoder = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICBitmap* legacySource = NULL;
    IWICFormatConverter* converter = NULL;
    HBITMAP bitmap = NULL;
    GUID format;
    UINT width = 0, height = 0;
    BOOL ok = FALSE;
    if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
            &IID_IWICImagingFactory, (void**)&factory)) ||
        FAILED(factory->lpVtbl->CreateDecoderFromFilename(factory, path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)) ||
        FAILED(decoder->lpVtbl->GetContainerFormat(decoder, &format)) ||
        !(IsEqualGUID(&format, &GUID_ContainerFormatPng) || IsEqualGUID(&format, &GUID_ContainerFormatBmp) ||
          IsEqualGUID(&format, &GUID_ContainerFormatIco) || IsEqualGUID(&format, &GUID_ContainerFormatJpeg)) ||
        !GRBestWicFrame(decoder, &frame) || FAILED(frame->lpVtbl->GetSize(frame, &width, &height))) goto cleanup;
    if (IsEqualGUID(&format, &GUID_ContainerFormatBmp)) legacySource = GRLegacyBmpAlphaSource(factory, path, width, height);
    if (FAILED(factory->lpVtbl->CreateFormatConverter(factory, &converter)) ||
        FAILED(converter->lpVtbl->Initialize(converter, legacySource ? (IWICBitmapSource*)legacySource : (IWICBitmapSource*)frame,
            &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom))) goto cleanup;
    BOOL single = width == height;
    UINT rawFrameHeight = single ? height : height / 3;
    UINT longest = max(width, rawFrameHeight);
    UINT scaledWidth = longest > 1024 ? (UINT)max(1, MulDiv((int)width, 1024, (int)longest)) : width;
    UINT scaledHeight = longest > 1024 ? (UINT)max(1, MulDiv((int)rawFrameHeight, 1024, (int)longest)) : rawFrameHeight;
    UINT outWidth = scaledWidth + (single ? 2 : 0);
    UINT outFrameHeight = scaledHeight + (single ? 2 : 0);
    UINT stride = outWidth * 4;
    BITMAPINFO info = { 0 };
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = (LONG)outWidth;
    info.bmiHeader.biHeight = -(LONG)(outFrameHeight * 3);
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    BYTE* pixels = NULL;
    bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);
    if (!bitmap) goto cleanup;
    ZeroMemory(pixels, stride * outFrameHeight * 3);
    for (UINT state = 0; state < (single ? 1u : 3u); ++state)
    {
        IWICBitmapClipper* clipper = NULL;
        IWICBitmapScaler* scaler = NULL;
        UINT top = single ? 0 : (state * height) / 3;
        UINT bottom = single ? height : ((state + 1) * height) / 3;
        WICRect crop = { 0, (INT)top, (INT)width, (INT)(bottom - top) };
        BOOL decoded = SUCCEEDED(factory->lpVtbl->CreateBitmapClipper(factory, &clipper)) &&
            SUCCEEDED(clipper->lpVtbl->Initialize(clipper, (IWICBitmapSource*)converter, &crop)) &&
            SUCCEEDED(factory->lpVtbl->CreateBitmapScaler(factory, &scaler)) &&
            SUCCEEDED(scaler->lpVtbl->Initialize(scaler, (IWICBitmapSource*)clipper, scaledWidth, scaledHeight, WICBitmapInterpolationModeFant)) &&
            SUCCEEDED(scaler->lpVtbl->CopyPixels(scaler, NULL, stride, stride * scaledHeight,
                pixels + state * outFrameHeight * stride + (single ? stride + 4 : 0)));
        if (scaler) scaler->lpVtbl->Release(scaler);
        if (clipper) clipper->lpVtbl->Release(clipper);
        if (!decoded) goto cleanup;
    }
    if (single)
    {
        DWORD* colors = (DWORD*)pixels;
        UINT count = outWidth * outFrameHeight;
        for (UINT y = 0; y < scaledHeight; ++y)
            for (UINT x = 0; x < scaledWidth; ++x)
            {
                DWORD pixel = colors[(y + 1) * outWidth + x + 1];
                UINT alpha = pixel >> 24, red = (pixel >> 16) & 255, green = (pixel >> 8) & 255, blue = pixel & 255;
                colors[count + (y + 1) * outWidth + x + 1] = (alpha << 24) | ((red + (alpha - red) / 5) << 16) |
                    ((green + (alpha - green) / 5) << 8) | (blue + (alpha - blue) / 5);
                // Extra canvas preserves edge pixels when the pressed state moves.
                colors[2 * count + (y + 2) * outWidth + x + 2] = (alpha << 24) |
                    ((red * 3 / 4) << 16) | ((green * 3 / 4) << 8) | (blue * 3 / 4);
            }
    }
    image->bitmap = bitmap; image->width = outWidth; image->frameHeight = outFrameHeight;
    ok = GRMeasureOrbContent(image);
cleanup:
    if (!ok)
    {
        if (bitmap) DeleteObject(bitmap);
        ZeroMemory(image, sizeof(*image));
    }
    if (converter) converter->lpVtbl->Release(converter);
    if (legacySource) legacySource->lpVtbl->Release(legacySource);
    if (frame) frame->lpVtbl->Release(frame);
    if (decoder) decoder->lpVtbl->Release(decoder);
    if (factory) factory->lpVtbl->Release(factory);
    if (SUCCEEDED(apartment)) CoUninitialize();
    return ok;
}

// Pre-resample cropped, premultiplied frames with WIC Fant filtering. The final
// AlphaBlend is 1:1; GDI's nearest-neighbor AlphaBlend stretching is not used.
static HBITMAP GRScaleOrbFrames(const CustomOrbImage* image, int width, int height)
{
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) return NULL;
    BITMAP sourceBitmap = { 0 };
    if (GetObjectW(image->bitmap, (int)sizeof(sourceBitmap), &sourceBitmap) != (int)sizeof(sourceBitmap) || !sourceBitmap.bmBits) return NULL;
    HRESULT apartment = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(apartment) && apartment != RPC_E_CHANGED_MODE) return NULL;
    IWICImagingFactory* factory = NULL; IWICBitmap* source = NULL;
    HBITMAP result = NULL;
    BOOL ok = FALSE;
    if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
            &IID_IWICImagingFactory, (void**)&factory)) ||
        FAILED(factory->lpVtbl->CreateBitmapFromMemory(factory, image->width, image->frameHeight * 3,
            &GUID_WICPixelFormat32bppPBGRA, (UINT)sourceBitmap.bmWidthBytes,
            (UINT)sourceBitmap.bmWidthBytes * image->frameHeight * 3, sourceBitmap.bmBits, &source))) goto cleanup;
    BITMAPINFO info = { 0 };
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height * 3; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    BYTE* pixels = NULL;
    result = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);
    if (!result) goto cleanup;
    RECT bounds = image->contentBounds;
    for (UINT state = 0; state < 3; ++state)
    {
        IWICBitmapClipper* clipper = NULL; IWICBitmapScaler* scaler = NULL;
        WICRect crop = { bounds.left, bounds.top + (INT)(state * image->frameHeight),
            bounds.right - bounds.left, bounds.bottom - bounds.top };
        BOOL copied = SUCCEEDED(factory->lpVtbl->CreateBitmapClipper(factory, &clipper)) &&
            SUCCEEDED(clipper->lpVtbl->Initialize(clipper, (IWICBitmapSource*)source, &crop)) &&
            SUCCEEDED(factory->lpVtbl->CreateBitmapScaler(factory, &scaler)) &&
            SUCCEEDED(scaler->lpVtbl->Initialize(scaler, (IWICBitmapSource*)clipper, (UINT)width, (UINT)height, WICBitmapInterpolationModeFant)) &&
            SUCCEEDED(scaler->lpVtbl->CopyPixels(scaler, NULL, (UINT)width * 4, (UINT)(width * height * 4), pixels + state * width * height * 4));
        if (scaler) scaler->lpVtbl->Release(scaler);
        if (clipper) clipper->lpVtbl->Release(clipper);
        if (!copied) goto cleanup;
    }
    ok = TRUE;
cleanup:
    if (source) source->lpVtbl->Release(source);
    if (factory) factory->lpVtbl->Release(factory);
    if (SUCCEEDED(apartment)) CoUninitialize();
    if (!ok && result) { DeleteObject(result); result = NULL; }
    return result;
}

static BOOL GRDrawOrbFrame(HDC dc, const CustomOrbImage* image, GRStartIconRenderCache* cache, RECT target, int state)
{
    int width = target.right - target.left, height = target.bottom - target.top;
    if (!image->bitmap || width <= 0 || height <= 0) return FALSE;
    if (!cache->bitmap || cache->width != width || cache->height != height)
    {
        GRClearStartIconCache(cache);
        cache->bitmap = GRScaleOrbFrames(image, width, height);
        cache->width = width; cache->height = height;
    }
    if (!cache->bitmap) return FALSE;
    HDC source = CreateCompatibleDC(dc);
    if (!source) return FALSE;
    HGDIOBJ old = SelectObject(source, cache->bitmap);
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    BOOL drawn = old && old != HGDI_ERROR && GdiAlphaBlend(dc, target.left, target.top, width, height,
        source, 0, max(0, min(2, state)) * height, width, height, blend);
    if (old && old != HGDI_ERROR) SelectObject(source, old);
    DeleteDC(source);
    return drawn;
}
