#include "win32_builder.h"

#include <objidl.h>
#include <stdio.h>
#include <string.h>

typedef void GpBitmap;
typedef void GpImage;

typedef struct BwGdiplusStartupInput {
    UINT32 GdiplusVersion;
    void *DebugEventCallback;
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
} BwGdiplusStartupInput;

typedef int (WINAPI *BwGdiplusStartupFn)(ULONG_PTR *token, const BwGdiplusStartupInput *input, void *output);
typedef void (WINAPI *BwGdiplusShutdownFn)(ULONG_PTR token);
typedef int (WINAPI *BwGdipCreateBitmapFromFileFn)(const WCHAR *filename, GpBitmap **bitmap);
typedef int (WINAPI *BwGdipCreateHBITMAPFromBitmapFn)(GpBitmap *bitmap, HBITMAP *hbitmap, COLORREF background);
typedef int (WINAPI *BwGdipDisposeImageFn)(GpImage *image);

typedef struct BwGdiplusApi {
    HMODULE module;
    ULONG_PTR token;
    int attempted;
    int ready;
    BwGdiplusStartupFn startup;
    BwGdiplusShutdownFn shutdown;
    BwGdipCreateBitmapFromFileFn create_from_file;
    BwGdipCreateHBITMAPFromBitmapFn create_hbitmap;
    BwGdipDisposeImageFn dispose_image;
} BwGdiplusApi;

static BwGdiplusApi g_preview_gdiplus;
static HBITMAP g_preview_images[3];
static int g_preview_images_loaded;

static int bw_hex_digit(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

static int bw_dirname_local(const char *path, char *out, size_t out_size)
{
    size_t len = strlen(path);

    while (len > 0 && path[len - 1] != '\\' && path[len - 1] != '/') {
        --len;
    }
    while (len > 1 && (path[len - 1] == '\\' || path[len - 1] == '/')) {
        --len;
    }
    if (len == 0 || len >= out_size) {
        return -1;
    }
    memcpy(out, path, len);
    out[len] = '\0';
    return 0;
}

static int bw_join_local(char *out, size_t out_size, const char *left, const char *right)
{
    size_t len;

    if (strlen(left) >= out_size) {
        return -1;
    }
    strcpy(out, left);
    len = strlen(out);
    if (len > 0 && out[len - 1] != '\\' && out[len - 1] != '/') {
        if (len + 2 >= out_size) {
            return -1;
        }
        out[len++] = '\\';
        out[len] = '\0';
    }
    if (len + strlen(right) >= out_size) {
        return -1;
    }
    strcat(out, right);
    return 0;
}

static int bw_file_exists(const char *path)
{
    DWORD attrs = GetFileAttributesA(path);

    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static int bw_gdiplus_init(void)
{
    BwGdiplusStartupInput input;

    if (g_preview_gdiplus.attempted) {
        return g_preview_gdiplus.ready;
    }
    g_preview_gdiplus.attempted = 1;
    g_preview_gdiplus.module = LoadLibraryA("gdiplus.dll");
    if (g_preview_gdiplus.module == NULL) {
        return 0;
    }
    g_preview_gdiplus.startup = (BwGdiplusStartupFn)GetProcAddress(g_preview_gdiplus.module, "GdiplusStartup");
    g_preview_gdiplus.shutdown = (BwGdiplusShutdownFn)GetProcAddress(g_preview_gdiplus.module, "GdiplusShutdown");
    g_preview_gdiplus.create_from_file =
        (BwGdipCreateBitmapFromFileFn)GetProcAddress(g_preview_gdiplus.module, "GdipCreateBitmapFromFile");
    g_preview_gdiplus.create_hbitmap =
        (BwGdipCreateHBITMAPFromBitmapFn)GetProcAddress(g_preview_gdiplus.module, "GdipCreateHBITMAPFromBitmap");
    g_preview_gdiplus.dispose_image =
        (BwGdipDisposeImageFn)GetProcAddress(g_preview_gdiplus.module, "GdipDisposeImage");
    if (g_preview_gdiplus.startup == NULL ||
        g_preview_gdiplus.shutdown == NULL ||
        g_preview_gdiplus.create_from_file == NULL ||
        g_preview_gdiplus.create_hbitmap == NULL ||
        g_preview_gdiplus.dispose_image == NULL) {
        FreeLibrary(g_preview_gdiplus.module);
        memset(&g_preview_gdiplus, 0, sizeof(g_preview_gdiplus));
        g_preview_gdiplus.attempted = 1;
        return 0;
    }

    memset(&input, 0, sizeof(input));
    input.GdiplusVersion = 1;
    if (g_preview_gdiplus.startup(&g_preview_gdiplus.token, &input, NULL) != 0) {
        FreeLibrary(g_preview_gdiplus.module);
        memset(&g_preview_gdiplus, 0, sizeof(g_preview_gdiplus));
        g_preview_gdiplus.attempted = 1;
        return 0;
    }
    g_preview_gdiplus.ready = 1;
    return 1;
}

static HBITMAP bw_load_png_file(const char *path)
{
    WCHAR wide_path[MAX_PATH];
    GpBitmap *bitmap = NULL;
    HBITMAP hbitmap = NULL;

    if (!bw_gdiplus_init() || !bw_file_exists(path)) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_ACP, 0, path, -1, wide_path, (int)(sizeof(wide_path) / sizeof(wide_path[0]))) == 0) {
        return NULL;
    }
    if (g_preview_gdiplus.create_from_file(wide_path, &bitmap) != 0 || bitmap == NULL) {
        return NULL;
    }
    if (g_preview_gdiplus.create_hbitmap(bitmap, &hbitmap, RGB(255, 255, 255)) != 0) {
        hbitmap = NULL;
    }
    g_preview_gdiplus.dispose_image((GpImage *)bitmap);
    return hbitmap;
}

static HBITMAP bw_load_preview_asset(const char *file_name)
{
    const char *candidates[] = {
        "docs\\assets",
        ".",
        "..\\docs\\assets",
        "..\\..\\docs\\assets"
    };
    char self[MAX_PATH];
    char self_dir[MAX_PATH];
    char path[MAX_PATH];
    size_t i;
    HBITMAP bitmap;

    for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (bw_join_local(path, sizeof(path), candidates[i], file_name) == 0) {
            bitmap = bw_load_png_file(path);
            if (bitmap != NULL) {
                return bitmap;
            }
        }
    }

    if (GetModuleFileNameA(NULL, self, sizeof(self)) != 0 &&
        bw_dirname_local(self, self_dir, sizeof(self_dir)) == 0) {
        const char *exe_candidates[] = {
            ".",
            "docs\\assets",
            "..\\docs\\assets",
            "..\\..\\docs\\assets"
        };

        for (i = 0; i < sizeof(exe_candidates) / sizeof(exe_candidates[0]); ++i) {
            char dir[MAX_PATH];

            if (bw_join_local(dir, sizeof(dir), self_dir, exe_candidates[i]) == 0 &&
                bw_join_local(path, sizeof(path), dir, file_name) == 0) {
                bitmap = bw_load_png_file(path);
                if (bitmap != NULL) {
                    return bitmap;
                }
            }
        }
    }

    return NULL;
}

static void bw_load_preview_images(void)
{
    if (g_preview_images_loaded) {
        return;
    }
    g_preview_images_loaded = 1;
    g_preview_images[OS_INSTALLER_STYLE_CLASSIC] = bw_load_preview_asset("screenshot-installer-classic.png");
    g_preview_images[OS_INSTALLER_STYLE_MODERN] = bw_load_preview_asset("screenshot-installer-modern.png");
    g_preview_images[OS_INSTALLER_STYLE_LEGACY] = bw_load_preview_asset("screenshot-installer-legacy.png");
}

static void bw_draw_bitmap_fit(HDC dc, HBITMAP bitmap, const RECT *rect)
{
    HDC mem;
    BITMAP bm;
    HGDIOBJ old_bitmap;
    int dst_w = rect->right - rect->left;
    int dst_h = rect->bottom - rect->top;
    int draw_w;
    int draw_h;
    int draw_x;
    int draw_y;

    if (bitmap == NULL || dst_w <= 0 || dst_h <= 0) {
        return;
    }

    GetObject(bitmap, sizeof(bm), &bm);
    if (bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        return;
    }

    draw_w = dst_w;
    draw_h = (bm.bmHeight * draw_w) / bm.bmWidth;
    if (draw_h > dst_h) {
        draw_h = dst_h;
        draw_w = (bm.bmWidth * draw_h) / bm.bmHeight;
    }
    draw_x = rect->left + (dst_w - draw_w) / 2;
    draw_y = rect->top + (dst_h - draw_h) / 2;

    mem = CreateCompatibleDC(dc);
    old_bitmap = SelectObject(mem, bitmap);
    SetStretchBltMode(dc, HALFTONE);
    SetBrushOrgEx(dc, 0, 0, NULL);
    StretchBlt(dc, draw_x, draw_y, draw_w, draw_h, mem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
    SelectObject(mem, old_bitmap);
    DeleteDC(mem);
}

static COLORREF bw_theme_color(const char *value, COLORREF fallback)
{
    int r1;
    int r2;
    int g1;
    int g2;
    int b1;
    int b2;

    if (value == NULL || value[0] == '\0') {
        return fallback;
    }
    if (value[0] == '#') {
        ++value;
    }
    if (strlen(value) != 6) {
        return fallback;
    }
    r1 = bw_hex_digit(value[0]);
    r2 = bw_hex_digit(value[1]);
    g1 = bw_hex_digit(value[2]);
    g2 = bw_hex_digit(value[3]);
    b1 = bw_hex_digit(value[4]);
    b2 = bw_hex_digit(value[5]);
    if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0) {
        return fallback;
    }
    return RGB((r1 << 4) | r2, (g1 << 4) | g2, (b1 << 4) | b2);
}

static void bw_fill(HDC dc, const RECT *rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);

    FillRect(dc, rect, brush);
    DeleteObject(brush);
}

static void bw_preview_frame(HDC dc, const RECT *rect, int active)
{
    HPEN pen = CreatePen(PS_SOLID, active ? 2 : 1, active ? RGB(28, 122, 242) : RGB(190, 197, 208));
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    HGDIOBJ old_pen = SelectObject(dc, pen);

    Rectangle(dc, rect->left, rect->top, rect->right, rect->bottom);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
}

static void bw_template_card_rects(const RECT *bounds, RECT cards[3])
{
    int gap = 8;
    int card_w = (bounds->right - bounds->left - gap * 2) / 3;
    int top = bounds->top + 30;
    int bottom = bounds->top + 116;
    int i;

    for (i = 0; i < 3; ++i) {
        cards[i].left = bounds->left + i * (card_w + gap);
        cards[i].top = top;
        cards[i].right = cards[i].left + card_w;
        cards[i].bottom = bottom;
    }
    cards[2].right = bounds->right;
}

static void bw_template_card_image_rect(const RECT *card, RECT *image)
{
    *image = *card;
    image->left += 5;
    image->top += 5;
    image->right -= 5;
    image->bottom -= 24;
}

int bw_template_preview_style_from_point(const RECT *bounds, int x, int y)
{
    RECT cards[3];
    POINT point;
    int i;

    point.x = x;
    point.y = y;
    bw_template_card_rects(bounds, cards);
    for (i = 0; i < 3; ++i) {
        if (PtInRect(&cards[i], point)) {
            return i;
        }
    }

    return -1;
}

static void bw_draw_classic_preview(HDC dc, RECT rect)
{
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    int side_w = w > 140 ? 52 : w / 4;
    int header_h = h > 100 ? 30 : h / 4;
    int footer_h = h > 100 ? 26 : h / 5;
    RECT side = {rect.left, rect.top, rect.left + side_w, rect.bottom};
    RECT header = {side.right, rect.top, rect.right, rect.top + header_h};
    RECT body = {side.right, header.bottom, rect.right, rect.bottom - footer_h};
    RECT footer = {side.right, body.bottom, rect.right, rect.bottom};
    RECT title = {body.left + 12, body.top + 16, body.right - 12, body.top + 22};
    RECT line = {body.left + 12, title.bottom + 12, body.right - 36, title.bottom + 17};
    RECT bar = {body.left + 12, body.bottom - 24, body.right - 12, body.bottom - 16};
    RECT dot = {side.left + side_w / 2 - 10, side.top + 30, side.left + side_w / 2 + 10, side.top + 50};

    bw_fill(dc, &rect, bw_theme_color(g_bw.config.theme.background, RGB(246, 248, 251)));
    bw_fill(dc, &side, bw_theme_color(g_bw.config.theme.sidebar, RGB(21, 78, 158)));
    bw_fill(dc, &header, bw_theme_color(g_bw.config.theme.panel, RGB(255, 255, 255)));
    bw_fill(dc, &body, bw_theme_color(g_bw.config.theme.background, RGB(246, 248, 251)));
    bw_fill(dc, &footer, bw_theme_color(g_bw.config.theme.panel, RGB(255, 255, 255)));
    bw_fill(dc, &title, RGB(18, 24, 38));
    bw_fill(dc, &line, RGB(154, 164, 179));
    bw_fill(dc, &dot, RGB(255, 255, 255));
    bw_fill(dc, &bar, RGB(224, 230, 238));
    bar.right = bar.left + ((bar.right - bar.left) * 62) / 100;
    bw_fill(dc, &bar, bw_theme_color(g_bw.config.theme.progress, RGB(22, 163, 74)));
}

static void bw_draw_modern_preview(HDC dc, RECT rect)
{
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    int rail_w = w > 140 ? 56 : w / 4;
    int margin = w > 140 ? 16 : 4;
    int card_w;
    int card_h;
    int radius;
    int cx;
    int cy;
    RECT rail = {rect.left, rect.top, rect.left + rail_w, rect.bottom};
    RECT card = {rail.right + margin, rect.top + h / 4, rect.right - margin, rect.bottom - h / 5};
    RECT rail_line = {rail.left + rail_w / 2 - 1, rail.top + h / 5, rail.left + rail_w / 2 + 1, rail.bottom - h / 5};
    RECT dot = {rail.left + rail_w / 2 - 6, rail.top + h / 4, rail.left + rail_w / 2 + 6, rail.top + h / 4 + 12};
    RECT title = {card.left + 12, card.top + 12, card.right - 12, card.top + 18};
    HPEN ring;
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;

    if (rail_w < 10) {
        rail_w = 10;
        rail.right = rail.left + rail_w;
    }
    if (card.right <= card.left + 8) {
        card.left = rail.right + 3;
        card.right = rect.right - 3;
    }
    card_w = card.right - card.left;
    card_h = card.bottom - card.top;
    radius = (card_w < card_h ? card_w : card_h) / 4;
    if (radius < 5) {
        radius = 5;
    }
    cx = (card.left + card.right) / 2;
    cy = (card.top + card.bottom) / 2 + 6;

    bw_fill(dc, &rect, bw_theme_color(g_bw.config.theme.background, RGB(255, 255, 255)));
    bw_fill(dc, &rail, bw_theme_color(g_bw.config.theme.sidebar, RGB(243, 244, 246)));
    bw_fill(dc, &rail_line, RGB(203, 213, 225));
    bw_fill(dc, &card, bw_theme_color(g_bw.config.theme.panel, RGB(255, 255, 255)));
    bw_preview_frame(dc, &card, 0);
    bw_fill(dc, &dot, bw_theme_color(g_bw.config.theme.accent, RGB(0, 120, 212)));
    bw_fill(dc, &title, RGB(30, 41, 59));

    ring = CreatePen(PS_SOLID, 5, bw_theme_color(g_bw.config.theme.progress, RGB(0, 120, 212)));
    old_pen = SelectObject(dc, ring);
    old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, cx - radius, cy - radius, cx + radius, cy + radius);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(ring);
}

static void bw_draw_legacy_gradient(HDC dc, const RECT *rect)
{
    COLORREF top = bw_theme_color(g_bw.config.theme.legacy_top, RGB(0, 18, 232));
    COLORREF bottom = bw_theme_color(g_bw.config.theme.legacy_bottom, RGB(0, 0, 18));
    int y;
    int height = rect->bottom - rect->top;

    for (y = rect->top; y < rect->bottom; y += 3) {
        int t = height > 0 ? ((y - rect->top) * 255) / height : 0;
        RECT band = {rect->left, y, rect->right, y + 3};
        COLORREF color = RGB(GetRValue(top) + ((GetRValue(bottom) - GetRValue(top)) * t) / 255,
                             GetGValue(top) + ((GetGValue(bottom) - GetGValue(top)) * t) / 255,
                             GetBValue(top) + ((GetBValue(bottom) - GetBValue(top)) * t) / 255);

        if (band.bottom > rect->bottom) {
            band.bottom = rect->bottom;
        }
        bw_fill(dc, &band, color);
    }
}

static void bw_draw_legacy_preview(HDC dc, RECT rect)
{
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    RECT title = {rect.left + 8, rect.top + 8, rect.left + w / 2, rect.top + 16};
    RECT meters = {rect.left + 8, rect.bottom - h / 2, rect.left + 8 + w / 4, rect.bottom - 8};
    RECT panel = {rect.right - w / 2 - 8, rect.bottom - h / 2, rect.right - 8, rect.bottom - 8};
    RECT bar = {panel.left + 8, panel.bottom - h / 6, panel.right - 8, panel.bottom - h / 9};
    RECT meter_a = {meters.left + 8, meters.top + 18, meters.left + 16, meters.bottom - 20};
    RECT meter_b = {meters.left + 26, meters.top + 18, meters.left + 34, meters.bottom - 20};
    RECT meter_c = {meters.left + 44, meters.top + 18, meters.left + 52, meters.bottom - 20};

    bw_draw_legacy_gradient(dc, &rect);
    bw_fill(dc, &title, RGB(255, 255, 255));
    bw_fill(dc, &meters, bw_theme_color(g_bw.config.theme.panel, RGB(238, 238, 224)));
    DrawEdge(dc, &meters, EDGE_RAISED, BF_RECT);
    bw_fill(dc, &panel, bw_theme_color(g_bw.config.theme.panel, RGB(238, 238, 224)));
    DrawEdge(dc, &panel, EDGE_RAISED, BF_RECT);
    bw_fill(dc, &meter_a, RGB(255, 255, 248));
    bw_fill(dc, &meter_b, RGB(255, 255, 248));
    bw_fill(dc, &meter_c, RGB(255, 255, 248));
    meter_a.top = meter_a.bottom - (meter_a.bottom - meter_a.top) / 4;
    meter_b.top = meter_b.bottom - (meter_b.bottom - meter_b.top) / 3;
    meter_c.top = meter_c.bottom - ((meter_c.bottom - meter_c.top) * 4) / 5;
    bw_fill(dc, &meter_a, RGB(0, 22, 190));
    bw_fill(dc, &meter_b, RGB(0, 22, 190));
    bw_fill(dc, &meter_c, bw_theme_color(g_bw.config.theme.progress, RGB(174, 0, 134)));
    bw_fill(dc, &bar, RGB(255, 255, 248));
    bar.right = bar.left + ((bar.right - bar.left) * 54) / 100;
    bw_fill(dc, &bar, bw_theme_color(g_bw.config.theme.progress, RGB(174, 0, 134)));
}

void bw_draw_template_preview(HDC dc, const RECT *bounds)
{
    RECT title = {bounds->left, bounds->top, bounds->right, bounds->top + 18};
    RECT cards[3];
    RECT classic;
    RECT modern;
    RECT legacy;
    RECT label = {bounds->left, bounds->top + 124, bounds->right, bounds->top + 144};
    RECT selected = {bounds->left, bounds->top + 154, bounds->right, bounds->bottom};
    const char *names[] = {"Classic wizard", "Modern suite", "Legacy full-screen"};
    const char *short_names[] = {"Classic", "Modern", "Legacy"};
    int style = g_bw.config.installer_style;
    HFONT old_font = (HFONT)SelectObject(dc, g_bw.font_body);
    int have_images;
    int i;

    if (style < OS_INSTALLER_STYLE_CLASSIC || style > OS_INSTALLER_STYLE_LEGACY) {
        style = OS_INSTALLER_STYLE_CLASSIC;
    }
    bw_template_card_rects(bounds, cards);
    bw_template_card_image_rect(&cards[OS_INSTALLER_STYLE_CLASSIC], &classic);
    bw_template_card_image_rect(&cards[OS_INSTALLER_STYLE_MODERN], &modern);
    bw_template_card_image_rect(&cards[OS_INSTALLER_STYLE_LEGACY], &legacy);
    bw_load_preview_images();
    have_images = g_preview_images[OS_INSTALLER_STYLE_CLASSIC] != NULL &&
                  g_preview_images[OS_INSTALLER_STYLE_MODERN] != NULL &&
                  g_preview_images[OS_INSTALLER_STYLE_LEGACY] != NULL;

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(87, 96, 106));
    DrawTextA(dc, "Template preview", -1, &title, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

    for (i = 0; i < 3; ++i) {
        RECT text = cards[i];
        COLORREF fill = i == style ? RGB(239, 246, 255) : RGB(255, 255, 255);

        bw_fill(dc, &cards[i], fill);
        bw_preview_frame(dc, &cards[i], i == style);
        text.top = cards[i].bottom - 19;
        text.bottom = cards[i].bottom - 2;
        SetTextColor(dc, i == style ? RGB(3, 78, 162) : RGB(31, 41, 55));
        DrawTextA(dc, short_names[i], -1, &text, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }

    if (g_preview_images[OS_INSTALLER_STYLE_CLASSIC] != NULL) {
        bw_fill(dc, &classic, RGB(255, 255, 255));
        bw_draw_bitmap_fit(dc, g_preview_images[OS_INSTALLER_STYLE_CLASSIC], &classic);
    } else {
        bw_draw_classic_preview(dc, classic);
    }
    if (g_preview_images[OS_INSTALLER_STYLE_MODERN] != NULL) {
        bw_fill(dc, &modern, RGB(255, 255, 255));
        bw_draw_bitmap_fit(dc, g_preview_images[OS_INSTALLER_STYLE_MODERN], &modern);
    } else {
        bw_draw_modern_preview(dc, modern);
    }
    if (g_preview_images[OS_INSTALLER_STYLE_LEGACY] != NULL) {
        bw_fill(dc, &legacy, RGB(255, 255, 255));
        bw_draw_bitmap_fit(dc, g_preview_images[OS_INSTALLER_STYLE_LEGACY], &legacy);
    } else {
        bw_draw_legacy_preview(dc, legacy);
    }

    SetTextColor(dc, RGB(17, 24, 39));
    DrawTextA(dc, names[style], -1, &label, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    if (have_images || g_preview_images[style] != NULL) {
        bw_fill(dc, &selected, RGB(255, 255, 255));
        bw_draw_bitmap_fit(dc, g_preview_images[style], &selected);
    } else if (style == OS_INSTALLER_STYLE_MODERN) {
        bw_draw_modern_preview(dc, selected);
    } else if (style == OS_INSTALLER_STYLE_LEGACY) {
        bw_draw_legacy_preview(dc, selected);
    } else {
        bw_draw_classic_preview(dc, selected);
    }
    bw_preview_frame(dc, &selected, 0);
    SelectObject(dc, old_font);
}

void bw_dispose_template_preview(void)
{
    int i;

    for (i = 0; i < 3; ++i) {
        if (g_preview_images[i] != NULL) {
            DeleteObject(g_preview_images[i]);
            g_preview_images[i] = NULL;
        }
    }
    if (g_preview_gdiplus.ready && g_preview_gdiplus.shutdown != NULL) {
        g_preview_gdiplus.shutdown(g_preview_gdiplus.token);
    }
    if (g_preview_gdiplus.module != NULL) {
        FreeLibrary(g_preview_gdiplus.module);
    }
    memset(&g_preview_gdiplus, 0, sizeof(g_preview_gdiplus));
    g_preview_images_loaded = 0;
}
