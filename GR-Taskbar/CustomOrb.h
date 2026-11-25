#pragma once

#include <wincodec.h>
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
} CustomOrbImage;

// Built-in button artwork is independent of the taskbar and Start menu styles.
// Render directly into premultiplied pixels so both previews and the taskbar
// use the same transparent artwork without depending on the current theme.
static BOOL CreateBuiltinOrbImage(DWORD style, CustomOrbImage* image)
{
    BITMAPINFO info = { 0 };
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 64;
    info.bmiHeader.biHeight = -192;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    DWORD* pixels = NULL;
    HBITMAP bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);
    if (!bitmap) return FALSE;
    ZeroMemory(pixels, 64 * 192 * sizeof(DWORD));
    for (int frame = 0; frame < 3; ++frame)
    {
        for (int y = 0; y < 64; ++y)
        {
            for (int x = 0; x < 64; ++x)
            {
                DWORD color = 0;
                int px = x - (frame == 2), py = y - (frame == 2);
                if (style == ORB_STYLE_WINDOWS7 || style == ORB_STYLE_WINDOWSXP)
                {
                    int radius = (px - 32) * (px - 32) + (py - 32) * (py - 32);
                    if (style == ORB_STYLE_WINDOWS7 && radius < 29 * 29)
                    {
                        BYTE glow = (BYTE)(45 + (64 - py) * (frame == 1 ? 2 : 1));
                        color = 0xff000000 | ((DWORD)(glow / 5) << 16) | ((DWORD)(glow / 2) << 8) | glow;
                        if (radius > 27 * 27) color = frame == 1 ? 0xff8cddff : 0xff5597bf;
                    }
                    int wave = px < 32 ? (px - 14) / 6 : (50 - px) / 6;
                    int wy = py - wave;
                    if (((px >= 15 && px < 30) || (px >= 33 && px < 49)) &&
                        ((wy >= 17 && wy < 31) || (wy >= 34 && wy < 48)))
                        color = wy < 31 ? (px < 32 ? 0xffff633e : 0xff78c541) :
                            (px < 32 ? 0xff35a7ed : 0xffffd54b);
                }
                else
                {
                    int top = style == 0 ? 18 - (px - 14) / 6 : 14;
                    int bottom = style == 0 ? 46 + (px - 14) / 6 : 50;
                    if (((px >= 14 && px < 30) || (px >= 34 && px < 50)) &&
                        py >= top && py < bottom && (py < 30 || py >= 34))
                        color = frame == 1 ? 0xff64d8ff : frame == 2 ? 0xff158ccc : style == 1 ? 0xff149de0 : 0xffe6f6ff;
                }
                if (style == ORB_STYLE_WINDOWSXP && frame == 1 && color)
                    color = 0xff000000 | (min(255, ((color >> 16) & 255) + 24) << 16) |
                        (min(255, ((color >> 8) & 255) + 24) << 8) | min(255, (color & 255) + 24);
                pixels[(frame * 64 + y) * 64 + x] = color;
            }
        }
    }
    *image = (CustomOrbImage){ bitmap, 64, 64 };
    return TRUE;
}

// Accept a square icon or three equal-height normal/hover/pressed frames.
static BOOL LoadCustomOrbImage(PCWSTR path, CustomOrbImage* image)
{
    HRESULT apartment = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(apartment) && apartment != RPC_E_CHANGED_MODE)
        return FALSE;

    IWICImagingFactory* factory = NULL;
    IWICBitmapDecoder* decoder = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICFormatConverter* converter = NULL;
    GUID format;
    UINT width = 0;
    UINT height = 0;
    HBITMAP bitmap = NULL;

    if (SUCCEEDED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
            &IID_IWICImagingFactory, (void**)&factory)) &&
        SUCCEEDED(factory->lpVtbl->CreateDecoderFromFilename(factory, path, NULL, GENERIC_READ,
            WICDecodeMetadataCacheOnDemand, &decoder)) &&
        SUCCEEDED(decoder->lpVtbl->GetContainerFormat(decoder, &format)) &&
        (IsEqualGUID(&format, &GUID_ContainerFormatPng) || IsEqualGUID(&format, &GUID_ContainerFormatBmp) ||
            IsEqualGUID(&format, &GUID_ContainerFormatIco) || IsEqualGUID(&format, &GUID_ContainerFormatJpeg)) &&
        SUCCEEDED(decoder->lpVtbl->GetFrame(decoder, 0, &frame)) &&
        SUCCEEDED(frame->lpVtbl->GetSize(frame, &width, &height)) &&
        width > 0 && width <= 1024 && height > 0 && height <= 3072 && (height == width || height % 3 == 0) &&
        SUCCEEDED(factory->lpVtbl->CreateFormatConverter(factory, &converter)) &&
        SUCCEEDED(converter->lpVtbl->Initialize(converter, (IWICBitmapSource*)frame,
            &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom)))
    {
        BITMAPINFO info = { 0 };
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = (LONG)width;
        UINT outputHeight = height == width ? height * 3 : height;
        info.bmiHeader.biHeight = -(LONG)outputHeight;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        void* pixels = NULL;
        bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &pixels, NULL, 0);
        if (bitmap && FAILED(converter->lpVtbl->CopyPixels(converter, NULL, width * 4,
                width * height * 4, (BYTE*)pixels)))
        {
            DeleteObject(bitmap);
            bitmap = NULL;
        }
        if (bitmap && height == width)
        {
            DWORD* colors = pixels;
            UINT count = width * height;
            ZeroMemory(colors + count, count * 2 * sizeof(DWORD));
            for (UINT y = 0; y < height; ++y)
            {
                for (UINT x = 0; x < width; ++x)
                {
                    DWORD pixel = colors[y * width + x];
                    UINT alpha = pixel >> 24;
                    UINT red = (pixel >> 16) & 255, green = (pixel >> 8) & 255, blue = pixel & 255;
                    colors[count + y * width + x] = (alpha << 24) | ((red + (alpha - red) / 5) << 16) |
                        ((green + (alpha - green) / 5) << 8) | (blue + (alpha - blue) / 5);
                    if (x + 1 < width && y + 1 < height)
                        colors[2 * count + (y + 1) * width + x + 1] = (alpha << 24) |
                            ((red * 3 / 4) << 16) | ((green * 3 / 4) << 8) | (blue * 3 / 4);
                }
            }
            height = outputHeight;
        }
    }

    if (converter) converter->lpVtbl->Release(converter);
    if (frame) frame->lpVtbl->Release(frame);
    if (decoder) decoder->lpVtbl->Release(decoder);
    if (factory) factory->lpVtbl->Release(factory);
    if (SUCCEEDED(apartment)) CoUninitialize();

    *image = (CustomOrbImage){ bitmap, width, height / 3 };
    return bitmap != NULL;
}
