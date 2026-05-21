#include "win32_runtime.h"

#include <objidl.h>
#include <string.h>

typedef void GpBitmap;
typedef void GpImage;

typedef struct RtGdiplusStartupInput {
    UINT32 GdiplusVersion;
    void *DebugEventCallback;
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
} RtGdiplusStartupInput;

typedef int (WINAPI *RtGdiplusStartupFn)(ULONG_PTR *token, const RtGdiplusStartupInput *input, void *output);
typedef void (WINAPI *RtGdiplusShutdownFn)(ULONG_PTR token);
typedef int (WINAPI *RtGdipCreateBitmapFromFileFn)(const WCHAR *filename, GpBitmap **bitmap);
typedef int (WINAPI *RtGdipCreateHBITMAPFromBitmapFn)(GpBitmap *bitmap, HBITMAP *hbitmap, COLORREF background);
typedef int (WINAPI *RtGdipDisposeImageFn)(GpImage *image);

typedef struct RtGdiplusApi {
    HMODULE module;
    ULONG_PTR token;
    int attempted;
    int ready;
    RtGdiplusStartupFn startup;
    RtGdiplusShutdownFn shutdown;
    RtGdipCreateBitmapFromFileFn create_from_file;
    RtGdipCreateHBITMAPFromBitmapFn create_hbitmap;
    RtGdipDisposeImageFn dispose_image;
} RtGdiplusApi;

static RtGdiplusApi g_rt_gdiplus;

static int rt_gdiplus_init(void)
{
    RtGdiplusStartupInput input;

    if (g_rt_gdiplus.attempted) {
        return g_rt_gdiplus.ready;
    }
    g_rt_gdiplus.attempted = 1;
    g_rt_gdiplus.module = LoadLibraryA("gdiplus.dll");
    if (g_rt_gdiplus.module == NULL) {
        return 0;
    }

    g_rt_gdiplus.startup = (RtGdiplusStartupFn)GetProcAddress(g_rt_gdiplus.module, "GdiplusStartup");
    g_rt_gdiplus.shutdown = (RtGdiplusShutdownFn)GetProcAddress(g_rt_gdiplus.module, "GdiplusShutdown");
    g_rt_gdiplus.create_from_file =
        (RtGdipCreateBitmapFromFileFn)GetProcAddress(g_rt_gdiplus.module, "GdipCreateBitmapFromFile");
    g_rt_gdiplus.create_hbitmap =
        (RtGdipCreateHBITMAPFromBitmapFn)GetProcAddress(g_rt_gdiplus.module, "GdipCreateHBITMAPFromBitmap");
    g_rt_gdiplus.dispose_image =
        (RtGdipDisposeImageFn)GetProcAddress(g_rt_gdiplus.module, "GdipDisposeImage");

    if (g_rt_gdiplus.startup == NULL ||
        g_rt_gdiplus.shutdown == NULL ||
        g_rt_gdiplus.create_from_file == NULL ||
        g_rt_gdiplus.create_hbitmap == NULL ||
        g_rt_gdiplus.dispose_image == NULL) {
        FreeLibrary(g_rt_gdiplus.module);
        memset(&g_rt_gdiplus, 0, sizeof(g_rt_gdiplus));
        g_rt_gdiplus.attempted = 1;
        return 0;
    }

    memset(&input, 0, sizeof(input));
    input.GdiplusVersion = 1;
    if (g_rt_gdiplus.startup(&g_rt_gdiplus.token, &input, NULL) != 0) {
        FreeLibrary(g_rt_gdiplus.module);
        memset(&g_rt_gdiplus, 0, sizeof(g_rt_gdiplus));
        g_rt_gdiplus.attempted = 1;
        return 0;
    }
    g_rt_gdiplus.ready = 1;
    return 1;
}

HBITMAP rt_load_image_file(const char *path)
{
    HBITMAP bitmap;
    WCHAR wide_path[MAX_PATH];
    GpBitmap *gp_bitmap = NULL;

    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    bitmap = (HBITMAP)LoadImageA(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (bitmap != NULL) {
        return bitmap;
    }

    if (!rt_gdiplus_init()) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_ACP, 0, path, -1, wide_path, (int)(sizeof(wide_path) / sizeof(wide_path[0]))) == 0) {
        return NULL;
    }
    if (g_rt_gdiplus.create_from_file(wide_path, &gp_bitmap) != 0 || gp_bitmap == NULL) {
        return NULL;
    }
    if (g_rt_gdiplus.create_hbitmap(gp_bitmap, &bitmap, RGB(255, 255, 255)) != 0) {
        bitmap = NULL;
    }
    g_rt_gdiplus.dispose_image((GpImage *)gp_bitmap);
    return bitmap;
}

void rt_dispose_image_loader(void)
{
    if (g_rt_gdiplus.ready && g_rt_gdiplus.shutdown != NULL) {
        g_rt_gdiplus.shutdown(g_rt_gdiplus.token);
    }
    if (g_rt_gdiplus.module != NULL) {
        FreeLibrary(g_rt_gdiplus.module);
    }
    memset(&g_rt_gdiplus, 0, sizeof(g_rt_gdiplus));
}
