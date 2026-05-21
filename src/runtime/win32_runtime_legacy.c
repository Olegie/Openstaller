#include "win32_runtime.h"

#include <stdio.h>
#include <string.h>

int rt_legacy_style_enabled(void)
{
    return g_rt.info.installer_style == OS_INSTALLER_STYLE_LEGACY;
}

void rt_legacy_apply_window_show(int *show_cmd)
{
    if (show_cmd != NULL && rt_legacy_style_enabled()) {
        *show_cmd = SW_SHOWMAXIMIZED;
    }
}

static int legacy_progress_page(void)
{
    int page = rt_find_page_kind(g_rt.mode == RT_MODE_UNINSTALL ? RT_PAGE_UNINSTALL_PROGRESS : RT_PAGE_PROGRESS);

    return page >= 0 ? page : 0;
}

static int legacy_is_progress_page(void)
{
    return g_rt.page == legacy_progress_page();
}

static void legacy_panel_rect(HWND hwnd, RECT *out)
{
    RECT client;
    int progress = legacy_is_progress_page();
    int width = progress ? 318 : 464;
    int height = progress ? 124 : 218;
    int margin = 10;

    GetClientRect(hwnd, &client);
    out->right = client.right - margin;
    out->bottom = client.bottom - margin;
    out->left = out->right - width;
    out->top = out->bottom - height;

    if (out->left < 18) {
        out->left = 18;
    }
    if (out->top < 96) {
        out->top = 96;
    }
}

static void legacy_draw_gradient(HDC dc, const RECT *rect)
{
    int y;
    int height = rect->bottom - rect->top;
    COLORREF top = rt_theme_color(g_rt.info.theme.legacy_top, RGB(0, 18, 232));
    COLORREF bottom = rt_theme_color(g_rt.info.theme.legacy_bottom, RGB(0, 0, 18));

    if (height <= 0) {
        return;
    }

    for (y = rect->top; y < rect->bottom; y += 4) {
        int t = ((y - rect->top) * 255) / height;
        int red = GetRValue(top) + ((GetRValue(bottom) - GetRValue(top)) * t) / 255;
        int green = GetGValue(top) + ((GetGValue(bottom) - GetGValue(top)) * t) / 255;
        int blue = GetBValue(top) + ((GetBValue(bottom) - GetBValue(top)) * t) / 255;
        RECT band = {rect->left, y, rect->right, y + 4};
        HBRUSH brush = CreateSolidBrush(RGB(red, green, blue));

        if (band.bottom > rect->bottom) {
            band.bottom = rect->bottom;
        }
        FillRect(dc, &band, brush);
        DeleteObject(brush);
    }
}

static void legacy_draw_background(HDC dc, const RECT *client)
{
    if (g_rt.background_image != NULL) {
        HDC mem = CreateCompatibleDC(dc);
        BITMAP bm;
        HGDIOBJ old_bitmap = SelectObject(mem, g_rt.background_image);

        GetObject(g_rt.background_image, sizeof(bm), &bm);
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchBlt(dc,
                   client->left,
                   client->top,
                   client->right - client->left,
                   client->bottom - client->top,
                   mem,
                   0,
                   0,
                   bm.bmWidth,
                   bm.bmHeight,
                   SRCCOPY);
        SelectObject(mem, old_bitmap);
        DeleteDC(mem);
        return;
    }

    legacy_draw_gradient(dc, client);
}

static void legacy_draw_panel(HDC dc, const RECT *panel)
{
    RECT shadow = {panel->left + 3, panel->top + 3, panel->right + 3, panel->bottom + 3};
    HBRUSH panel_brush = CreateSolidBrush(rt_theme_color(g_rt.info.theme.panel, RGB(238, 238, 224)));
    HBRUSH shadow_brush = CreateSolidBrush(RGB(16, 16, 32));
    HPEN dark = CreatePen(PS_SOLID, 1, RGB(70, 70, 70));
    HGDIOBJ old_pen;

    FillRect(dc, &shadow, shadow_brush);
    FillRect(dc, panel, panel_brush);
    old_pen = SelectObject(dc, dark);
    MoveToEx(dc, panel->left, panel->top, NULL);
    LineTo(dc, panel->right - 1, panel->top);
    LineTo(dc, panel->right - 1, panel->bottom - 1);
    LineTo(dc, panel->left, panel->bottom - 1);
    LineTo(dc, panel->left, panel->top);
    SelectObject(dc, old_pen);

    DeleteObject(dark);
    DeleteObject(panel_brush);
    DeleteObject(shadow_brush);
}

static COLORREF legacy_theme_text_color(const char *value, COLORREF fallback)
{
    COLORREF color = rt_theme_color(value, fallback);

    return color == RGB(0, 0, 0) ? fallback : color;
}

static void legacy_draw_brand(HDC dc)
{
    RECT brand = {8, 8, 720, 70};
    RECT company = {12, 70, 720, 94};
    HFONT font = CreateFontA(36,
                             0,
                             0,
                             0,
                             FW_BOLD,
                             TRUE,
                             FALSE,
                             FALSE,
                             DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE,
                             "Times New Roman");
    HGDIOBJ old_font;

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, legacy_theme_text_color(g_rt.info.theme.text, RGB(255, 255, 255)));
    old_font = SelectObject(dc, font != NULL ? font : g_rt.font_title);
    DrawTextA(dc, g_rt.info.app_name, -1, &brand, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, old_font);
    if (font != NULL) {
        DeleteObject(font);
    }

    if (g_rt.info.company_name[0] != '\0') {
        SetTextColor(dc, legacy_theme_text_color(g_rt.info.theme.muted_text, RGB(220, 228, 255)));
        old_font = SelectObject(dc, g_rt.font_body);
        DrawTextA(dc, g_rt.info.company_name, -1, &company, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(dc, old_font);
    }
}

static int legacy_clamp_percent(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return value;
}

static void legacy_fill_color(HDC dc, const RECT *rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);

    FillRect(dc, rect, brush);
    DeleteObject(brush);
}

static void legacy_draw_raised_rect(HDC dc, const RECT *rect, COLORREF color)
{
    RECT edge = *rect;

    legacy_fill_color(dc, rect, color);
    DrawEdge(dc, &edge, EDGE_RAISED, BF_RECT);
}

static void legacy_draw_sunken_rect(HDC dc, const RECT *rect, COLORREF color)
{
    RECT edge = *rect;

    legacy_fill_color(dc, rect, color);
    DrawEdge(dc, &edge, EDGE_SUNKEN, BF_RECT);
}

static void legacy_draw_meter_fill(HDC dc,
                                   const RECT *slot,
                                   int level,
                                   COLORREF dark,
                                   COLORREF mid,
                                   COLORREF light,
                                   int segmented)
{
    RECT inner = *slot;
    RECT fill;
    HPEN shadow;
    HPEN highlight;
    HGDIOBJ old_pen;

    InflateRect(&inner, -3, -3);
    level = legacy_clamp_percent(level);
    fill = inner;
    fill.top = fill.bottom - ((fill.bottom - fill.top) * level) / 100;

    legacy_fill_color(dc, &inner, RGB(255, 255, 248));
    if (fill.top < fill.bottom) {
        if (segmented) {
            int y;

            for (y = fill.bottom - 5; y >= fill.top; y -= 7) {
                RECT segment = {fill.left + 1, y, fill.right - 1, y + 5};

                if (segment.top < fill.top) {
                    segment.top = fill.top;
                }
                legacy_fill_color(dc, &segment, mid);
                SetPixel(dc, segment.left, segment.top, light);
                SetPixel(dc, segment.right - 1, segment.bottom - 1, dark);
            }
        } else {
            legacy_fill_color(dc, &fill, mid);
            {
                RECT gleam = {fill.left + 1, fill.top, fill.left + 3, fill.bottom};
                legacy_fill_color(dc, &gleam, light);
            }
        }
    }

    shadow = CreatePen(PS_SOLID, 1, dark);
    highlight = CreatePen(PS_SOLID, 1, light);
    old_pen = SelectObject(dc, highlight);
    MoveToEx(dc, inner.left, inner.top, NULL);
    LineTo(dc, inner.left, inner.bottom);
    SelectObject(dc, shadow);
    MoveToEx(dc, inner.right - 1, inner.top, NULL);
    LineTo(dc, inner.right - 1, inner.bottom);
    SelectObject(dc, old_pen);
    DeleteObject(shadow);
    DeleteObject(highlight);
}

static void legacy_draw_icon_button(HDC dc, const RECT *rect, int kind)
{
    RECT icon = *rect;
    HPEN outline = CreatePen(PS_SOLID, 1, RGB(72, 72, 72));
    HPEN accent = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HGDIOBJ old_pen;
    HBRUSH old_brush;
    HBRUSH yellow = CreateSolidBrush(RGB(245, 220, 66));
    HBRUSH gray = CreateSolidBrush(RGB(200, 208, 208));
    HBRUSH green = CreateSolidBrush(RGB(48, 180, 88));
    HBRUSH blue = CreateSolidBrush(RGB(42, 82, 184));

    legacy_draw_raised_rect(dc, rect, RGB(238, 238, 224));
    InflateRect(&icon, -4, -4);

    old_pen = SelectObject(dc, outline);
    old_brush = SelectObject(dc, yellow);
    if (kind == 0) {
        POINT folder[] = {
            {icon.left, icon.top + 6},
            {icon.left + 5, icon.top + 1},
            {icon.left + 12, icon.top + 1},
            {icon.left + 15, icon.top + 5},
            {icon.right, icon.top + 5},
            {icon.right - 2, icon.bottom},
            {icon.left + 2, icon.bottom}
        };
        Polygon(dc, folder, (int)(sizeof(folder) / sizeof(folder[0])));
    } else if (kind == 1) {
        SelectObject(dc, gray);
        Rectangle(dc, icon.left + 1, icon.top + 1, icon.right - 1, icon.bottom - 1);
        SelectObject(dc, blue);
        Rectangle(dc, icon.left + 4, icon.top + 4, icon.right - 4, icon.top + 9);
        SelectObject(dc, accent);
        MoveToEx(dc, icon.left + 3, icon.bottom - 5, NULL);
        LineTo(dc, icon.right - 3, icon.bottom - 5);
    } else {
        SelectObject(dc, green);
        Ellipse(dc, icon.left + 2, icon.top + 2, icon.right - 2, icon.bottom - 2);
        SelectObject(dc, blue);
        Rectangle(dc, icon.left + 6, icon.top + 7, icon.right - 6, icon.bottom - 5);
    }

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(outline);
    DeleteObject(accent);
    DeleteObject(yellow);
    DeleteObject(gray);
    DeleteObject(green);
    DeleteObject(blue);
}

static void legacy_draw_meters(HDC dc, const RECT *client)
{
    RECT block;
    RECT shadow;
    HPEN bevel_dark = CreatePen(PS_SOLID, 1, RGB(82, 82, 82));
    HPEN bevel_light = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HGDIOBJ old_pen;
    int progress = g_rt.operation_started ? g_rt.progress_value : 0;
    int levels[3];
    int i;

    block.left = 8;
    block.right = 112;
    block.bottom = client->bottom - 10;
    block.top = block.bottom - 160;
    shadow = block;
    OffsetRect(&shadow, 3, 3);
    legacy_fill_color(dc, &shadow, RGB(8, 8, 28));
    legacy_draw_raised_rect(dc, &block, RGB(238, 238, 224));

    old_pen = SelectObject(dc, bevel_light);
    MoveToEx(dc, block.left + 4, block.top + 4, NULL);
    LineTo(dc, block.right - 6, block.top + 4);
    MoveToEx(dc, block.left + 4, block.top + 4, NULL);
    LineTo(dc, block.left + 4, block.bottom - 6);
    SelectObject(dc, bevel_dark);
    MoveToEx(dc, block.left + 5, block.bottom - 6, NULL);
    LineTo(dc, block.right - 6, block.bottom - 6);
    MoveToEx(dc, block.right - 6, block.top + 5, NULL);
    LineTo(dc, block.right - 6, block.bottom - 6);
    SelectObject(dc, old_pen);

    if (!g_rt.operation_started) {
        levels[0] = 8;
        levels[1] = 5;
        levels[2] = 14;
    } else {
        levels[0] = legacy_clamp_percent(progress);
        levels[1] = legacy_clamp_percent(progress > 18 ? (progress - 18) * 125 / 82 : progress / 2);
        levels[2] = legacy_clamp_percent(progress > 55 ? (progress - 55) * 220 / 45 : progress / 4);
    }

    for (i = 0; i < 3; ++i) {
        RECT slot = {block.left + 13 + i * 34, block.top + 16, block.left + 25 + i * 34, block.top + 84};
        RECT icon = {block.left + 6 + i * 34, block.bottom - 36, block.left + 30 + i * 34, block.bottom - 12};

        legacy_draw_sunken_rect(dc, &slot, RGB(255, 255, 248));
        if (i == 2) {
            legacy_draw_meter_fill(dc,
                                   &slot,
                                   levels[i],
                                   RGB(92, 0, 72),
                                   rt_theme_color(g_rt.info.theme.progress, RGB(174, 0, 134)),
                                   RGB(248, 104, 210),
                                   1);
        } else {
            legacy_draw_meter_fill(dc,
                                   &slot,
                                   levels[i],
                                   RGB(0, 16, 120),
                                   RGB(0, 32, 176),
                                   RGB(128, 168, 255),
                                   0);
        }
        legacy_draw_icon_button(dc, &icon, i);
    }

    DeleteObject(bevel_dark);
    DeleteObject(bevel_light);
}

void rt_legacy_paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT client;
    RECT panel;

    GetClientRect(hwnd, &client);
    legacy_draw_background(dc, &client);
    legacy_draw_brand(dc);
    legacy_draw_meters(dc, &client);
    legacy_panel_rect(hwnd, &panel);
    legacy_draw_panel(dc, &panel);
    EndPaint(hwnd, &ps);
}

int rt_legacy_ctlcolor_static(HDC dc, HWND control, LRESULT *result)
{
    (void)control;

    if (!rt_legacy_style_enabled() || result == NULL) {
        return 0;
    }

    SetTextColor(dc, RGB(0, 0, 0));
    SetBkMode(dc, TRANSPARENT);
    *result = (LRESULT)g_rt.brush_bg;
    return 1;
}

void rt_legacy_layout(HWND hwnd)
{
    RECT panel;
    RECT client;
    int progress;
    int w;
    int button_y;
    size_t i;

    if (!rt_legacy_style_enabled() || hwnd == NULL) {
        return;
    }

    GetClientRect(hwnd, &client);
    legacy_panel_rect(hwnd, &panel);
    progress = legacy_is_progress_page();
    w = panel.right - panel.left;
    button_y = panel.bottom - 34;

    SendMessageA(g_rt.progress, PBM_SETBARCOLOR, 0, rt_theme_color(g_rt.info.theme.progress, RGB(0, 32, 176)));
    SendMessageA(g_rt.progress, PBM_SETBKCOLOR, 0, RGB(255, 255, 255));

    if (progress) {
        MoveWindow(g_rt.body, panel.left + 10, panel.top + 8, w - 20, 20, TRUE);
        MoveWindow(g_rt.progress_detail, panel.left + 10, panel.top + 30, w - 20, 26, TRUE);
        MoveWindow(g_rt.progress, panel.left + 10, panel.top + 58, w - 20, 20, TRUE);
        MoveWindow(g_rt.progress_text, panel.left + 10, panel.top + 82, w - 20, 20, TRUE);
        MoveWindow(g_rt.progress_more, panel.left + 118, button_y, 92, 24, TRUE);
        MoveWindow(g_rt.cancel, panel.right - 94, button_y, 82, 24, TRUE);
        MoveWindow(g_rt.next, panel.right - 184, button_y, 82, 24, TRUE);
        MoveWindow(g_rt.back, panel.right - 274, button_y, 82, 24, TRUE);
        rt_show(g_rt.cancel, 1);
        if (!g_rt.operation_done) {
            rt_show(g_rt.back, 0);
            rt_show(g_rt.next, 0);
        } else {
            rt_show(g_rt.back, 1);
            rt_show(g_rt.next, 1);
        }
        if (g_rt.progress_expanded) {
            int log_top = panel.top - 112;
            if (log_top < 96) {
                log_top = 96;
            }
            MoveWindow(g_rt.progress_log, panel.left, log_top, w, panel.top - log_top - 8, TRUE);
        }
    } else {
        MoveWindow(g_rt.title, panel.left + 14, panel.top + 12, w - 28, 22, TRUE);
        MoveWindow(g_rt.subtitle, panel.left + 14, panel.top + 40, w - 28, 34, TRUE);
        MoveWindow(g_rt.body, panel.left + 14, panel.top + 82, w - 28, 78, TRUE);
        MoveWindow(g_rt.license, panel.left + 14, panel.top + 82, w - 28, 96, TRUE);
        MoveWindow(g_rt.accept, panel.left + 14, panel.top + 184, 220, 22, TRUE);
        MoveWindow(g_rt.install_dir, panel.left + 14, panel.top + 108, w - 118, 24, TRUE);
        MoveWindow(g_rt.browse, panel.right - 92, panel.top + 107, 78, 26, TRUE);
        MoveWindow(g_rt.component_main, panel.left + 30, panel.top + 92, w - 60, 22, TRUE);
        MoveWindow(g_rt.component_reg, panel.left + 30, panel.top + 120, w - 60, 22, TRUE);
        for (i = 0; i < OS_MAX_ONLINE_COMPONENTS; ++i) {
            MoveWindow(g_rt.online_components[i],
                       panel.left + 30,
                       panel.top + 148 + (int)i * 24,
                       w - 60,
                       22,
                       TRUE);
        }
        MoveWindow(g_rt.back, panel.right - 274, button_y, 82, 24, TRUE);
        MoveWindow(g_rt.next, panel.right - 184, button_y, 82, 24, TRUE);
        MoveWindow(g_rt.cancel, panel.right - 94, button_y, 82, 24, TRUE);
        rt_show(g_rt.back, 1);
        rt_show(g_rt.next, 1);
        rt_show(g_rt.cancel, 1);
    }

    InvalidateRect(hwnd, NULL, TRUE);
}
