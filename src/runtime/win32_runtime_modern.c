#include "win32_runtime.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define MODERN_RAIL_W 220
#define MODERN_MARGIN 48
#define MODERN_PRIMARY RGB(0, 120, 212)
#define MODERN_PRIMARY_DARK RGB(0, 90, 158)
#define MODERN_RAIL_BG RGB(243, 244, 246)
#define MODERN_TEXT RGB(0, 0, 0)
#define MODERN_MUTED RGB(87, 96, 106)
#define MODERN_BORDER RGB(229, 231, 235)

int rt_modern_style_enabled(void)
{
    return g_rt.info.installer_style == OS_INSTALLER_STYLE_MODERN;
}

static int modern_progress_page(void)
{
    int page = rt_find_page_kind(g_rt.mode == RT_MODE_UNINSTALL ? RT_PAGE_UNINSTALL_PROGRESS : RT_PAGE_PROGRESS);

    return page >= 0 ? page : 0;
}

static int modern_is_progress_page(void)
{
    return g_rt.page == modern_progress_page();
}

static void modern_client(HWND hwnd, RECT *client)
{
    GetClientRect(hwnd, client);
    if (client->right < 760) {
        client->right = 760;
    }
    if (client->bottom < 520) {
        client->bottom = 520;
    }
}

static void modern_content_rect(HWND hwnd, RECT *out)
{
    RECT client;

    modern_client(hwnd, &client);
    out->left = MODERN_RAIL_W + MODERN_MARGIN;
    out->top = 48;
    out->right = client.right - MODERN_MARGIN;
    out->bottom = client.bottom - 72;
    if (out->right - out->left < 420) {
        out->right = out->left + 420;
    }
}

static void modern_card_rect(HWND hwnd, RECT *out)
{
    RECT content;

    modern_content_rect(hwnd, &content);
    out->left = content.left;
    out->top = modern_is_progress_page() ? content.top + 92 : content.top + 116;
    out->right = content.right;
    out->bottom = content.bottom - (modern_is_progress_page() ? 0 : 26);
}

static int modern_color_is_dark(COLORREF color)
{
    unsigned long score = (unsigned long)GetRValue(color) * 299ul +
                          (unsigned long)GetGValue(color) * 587ul +
                          (unsigned long)GetBValue(color) * 114ul;

    return score < 128000ul;
}

static void modern_draw_card(HDC dc, const RECT *rect)
{
    COLORREF panel_color = rt_theme_color(g_rt.info.theme.panel, RGB(255, 255, 255));
    HBRUSH white = CreateSolidBrush(panel_color);
    HPEN border = CreatePen(PS_SOLID, 1, MODERN_BORDER);
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;

    old_pen = SelectObject(dc, border);
    old_brush = SelectObject(dc, white);
    Rectangle(dc, rect->left, rect->top, rect->right, rect->bottom);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(border);
    DeleteObject(white);
}

static void modern_draw_steps(HDC dc, const RECT *client)
{
    COLORREF primary = rt_theme_color(g_rt.info.theme.accent, MODERN_PRIMARY);
    COLORREF rail_color = rt_theme_color(g_rt.info.theme.sidebar, MODERN_RAIL_BG);
    COLORREF text_color = rt_theme_color(g_rt.info.theme.text, MODERN_TEXT);
    COLORREF muted = rt_theme_color(g_rt.info.theme.muted_text, MODERN_MUTED);
    COLORREF rail_text = modern_color_is_dark(rail_color) ? RGB(255, 255, 255) : text_color;
    COLORREF rail_muted = modern_color_is_dark(rail_color) ? RGB(206, 220, 237) : muted;
    int i;
    int y = 152;
    HFONT old_font = (HFONT)SelectObject(dc, g_rt.font_body);

    (void)client;

    SetBkMode(dc, TRANSPARENT);
    for (i = 0; i < g_rt.page_count; ++i) {
        RECT dot = {28, y - 4, 44, y + 12};
        RECT text = {58, y - 7, MODERN_RAIL_W - 18, y + 16};
        HBRUSH brush = CreateSolidBrush(i == g_rt.page ? primary : RGB(255, 255, 255));
        HPEN pen = CreatePen(PS_SOLID, 1, i <= g_rt.page ? primary : RGB(190, 197, 208));
        HGDIOBJ old_pen = SelectObject(dc, pen);
        HGDIOBJ old_brush = SelectObject(dc, brush);

        Ellipse(dc, dot.left, dot.top, dot.right, dot.bottom);
        SelectObject(dc, old_brush);
        SelectObject(dc, old_pen);
        DeleteObject(brush);
        DeleteObject(pen);

        SetTextColor(dc, i == g_rt.page ? rail_text : rail_muted);
        DrawTextA(dc, rt_page_step_name(i), -1, &text, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        if (i + 1 < g_rt.page_count) {
            HPEN line = CreatePen(PS_SOLID, 1, RGB(210, 216, 226));
            old_pen = SelectObject(dc, line);
            MoveToEx(dc, 36, y + 15, NULL);
            LineTo(dc, 36, y + 41);
            SelectObject(dc, old_pen);
            DeleteObject(line);
        }
        y += 48;
    }

    SelectObject(dc, old_font);
}

static void modern_draw_brand(HDC dc)
{
    RECT name = {24, 28, MODERN_RAIL_W - 18, 66};
    RECT company = {26, 70, MODERN_RAIL_W - 18, 94};
    COLORREF primary = rt_theme_color(g_rt.info.theme.accent, MODERN_PRIMARY);
    COLORREF rail_color = rt_theme_color(g_rt.info.theme.sidebar, MODERN_RAIL_BG);
    COLORREF muted = rt_theme_color(g_rt.info.theme.muted_text, MODERN_MUTED);
    COLORREF brand = modern_color_is_dark(rail_color) ? RGB(255, 255, 255) : primary;
    COLORREF subbrand = modern_color_is_dark(rail_color) ? RGB(206, 220, 237) : muted;
    HFONT old_font = (HFONT)SelectObject(dc, g_rt.font_title);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, brand);
    DrawTextA(dc, g_rt.info.app_name, -1, &name, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, old_font);

    old_font = (HFONT)SelectObject(dc, g_rt.font_body);
    SetTextColor(dc, subbrand);
    DrawTextA(dc,
              g_rt.info.company_name[0] != '\0' ? g_rt.info.company_name : "Setup package",
              -1,
              &company,
              DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, old_font);
}

static void modern_draw_progress_ring(HDC dc, const RECT *card)
{
    int cx = (card->left + card->right) / 2;
    int cy = card->top + 142;
    int radius = 70;
    int stroke = 7;
    int segments = 96;
    int active = (segments * g_rt.progress_value) / 100;
    int i;
    HPEN track = CreatePen(PS_SOLID, stroke, RGB(232, 235, 240));
    COLORREF progress = rt_theme_color(g_rt.info.theme.progress, MODERN_PRIMARY);
    COLORREF progress_text = rt_theme_color(g_rt.info.theme.accent, MODERN_PRIMARY_DARK);
    HGDIOBJ old_pen = SelectObject(dc, track);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    char percent[16];
    RECT text_rect = {cx - 64, cy - 22, cx + 64, cy + 24};
    HFONT percent_font;
    HGDIOBJ old_font;

    Ellipse(dc, cx - radius, cy - radius, cx + radius, cy + radius);
    SelectObject(dc, old_pen);
    DeleteObject(track);

    old_pen = SelectObject(dc, CreatePen(PS_SOLID, stroke, progress));
    for (i = 0; i < active; ++i) {
        double a1 = (-90.0 + (360.0 * i) / segments) * 3.14159265358979323846 / 180.0;
        double a2 = (-90.0 + (360.0 * (i + 0.72)) / segments) * 3.14159265358979323846 / 180.0;
        MoveToEx(dc, cx + (int)(cos(a1) * radius), cy + (int)(sin(a1) * radius), NULL);
        LineTo(dc, cx + (int)(cos(a2) * radius), cy + (int)(sin(a2) * radius));
    }
    DeleteObject(SelectObject(dc, old_pen));
    SelectObject(dc, old_brush);

    percent_font = CreateFontA(34,
                               0,
                               0,
                               0,
                               FW_BOLD,
                               FALSE,
                               FALSE,
                               FALSE,
                               DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS,
                               DEFAULT_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE,
                               g_rt.info.ui_font[0] != '\0' ? g_rt.info.ui_font : "Segoe UI");
    old_font = SelectObject(dc, percent_font != NULL ? percent_font : g_rt.font_title);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, progress_text);
    snprintf(percent, sizeof(percent), "%d%%", g_rt.progress_value);
    DrawTextA(dc, percent, -1, &text_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old_font);
    if (percent_font != NULL) {
        DeleteObject(percent_font);
    }
}

void rt_modern_paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT client;
    RECT rail;
    RECT card;
    HBRUSH bg = CreateSolidBrush(rt_theme_color(g_rt.info.theme.background, RGB(255, 255, 255)));
    HBRUSH rail_bg = CreateSolidBrush(rt_theme_color(g_rt.info.theme.sidebar, MODERN_RAIL_BG));
    HPEN divider = CreatePen(PS_SOLID, 1, MODERN_BORDER);

    modern_client(hwnd, &client);
    rail.left = 0;
    rail.top = 0;
    rail.right = MODERN_RAIL_W;
    rail.bottom = client.bottom;

    FillRect(dc, &client, bg);
    FillRect(dc, &rail, rail_bg);
    SelectObject(dc, divider);
    MoveToEx(dc, MODERN_RAIL_W, 0, NULL);
    LineTo(dc, MODERN_RAIL_W, client.bottom);

    modern_draw_brand(dc);
    modern_draw_steps(dc, &client);
    modern_card_rect(hwnd, &card);
    modern_draw_card(dc, &card);
    if (modern_is_progress_page()) {
        modern_draw_progress_ring(dc, &card);
    }

    DeleteObject(bg);
    DeleteObject(rail_bg);
    DeleteObject(divider);
    EndPaint(hwnd, &ps);
}

int rt_modern_ctlcolor_static(HDC dc, HWND control, LRESULT *result)
{
    (void)control;

    if (!rt_modern_style_enabled() || result == NULL) {
        return 0;
    }

    SetTextColor(dc, rt_theme_color(g_rt.info.theme.text, MODERN_TEXT));
    SetBkMode(dc, TRANSPARENT);
    *result = (LRESULT)GetStockObject(NULL_BRUSH);
    return 1;
}

void rt_modern_layout(HWND hwnd)
{
    RECT content;
    RECT card;
    int card_w;
    int button_y;
    int progress = modern_is_progress_page();
    size_t i;

    if (!rt_modern_style_enabled() || hwnd == NULL || g_rt.back == NULL) {
        return;
    }

    modern_content_rect(hwnd, &content);
    modern_card_rect(hwnd, &card);
    card_w = card.right - card.left;
    button_y = content.bottom + 22;

    if (button_y + 30 > card.bottom + 62) {
        button_y = card.bottom + 22;
    }

    MoveWindow(g_rt.title, content.left, content.top, content.right - content.left, 28, TRUE);
    MoveWindow(g_rt.subtitle, content.left, content.top + 42, content.right - content.left, 42, TRUE);

    if (progress) {
        rt_show(g_rt.progress, 0);
        MoveWindow(g_rt.body, card.left + 28, card.top + 24, card_w - 56, 28, TRUE);
        MoveWindow(g_rt.progress_text, card.left + 36, card.top + 220, card_w - 72, 24, TRUE);
        MoveWindow(g_rt.progress_detail, card.left + 36, card.top + 252, card_w - 190, 44, TRUE);
        MoveWindow(g_rt.progress_more, card.right - 136, card.top + 252, 104, 28, TRUE);
        if (g_rt.progress_expanded) {
            int log_height = card.bottom - card.top - 330;
            if (log_height < 48) {
                log_height = 48;
            }
            MoveWindow(g_rt.progress_log, card.left + 28, card.top + 306, card_w - 56, log_height, TRUE);
        }
        if (!g_rt.operation_done) {
            rt_show(g_rt.back, 0);
            rt_show(g_rt.next, 0);
        } else {
            rt_show(g_rt.back, 1);
            rt_show(g_rt.next, 1);
        }
    } else {
        MoveWindow(g_rt.body, card.left + 28, card.top + 26, card_w - 56, 92, TRUE);
        MoveWindow(g_rt.license, card.left + 28, card.top + 24, card_w - 56, card.bottom - card.top - 82, TRUE);
        MoveWindow(g_rt.accept, card.left + 28, card.bottom - 48, 260, 24, TRUE);
        MoveWindow(g_rt.install_dir, card.left + 28, card.top + 74, card_w - 160, 26, TRUE);
        MoveWindow(g_rt.browse, card.right - 116, card.top + 73, 88, 28, TRUE);
        MoveWindow(g_rt.component_main, card.left + 38, card.top + 44, card_w - 76, 24, TRUE);
        MoveWindow(g_rt.component_reg, card.left + 38, card.top + 76, card_w - 76, 24, TRUE);
        for (i = 0; i < OS_MAX_ONLINE_COMPONENTS; ++i) {
            MoveWindow(g_rt.online_components[i],
                       card.left + 38,
                       card.top + 108 + (int)i * 28,
                       card_w - 76,
                       24,
                       TRUE);
        }
        rt_show(g_rt.back, 1);
        rt_show(g_rt.next, 1);
    }

    MoveWindow(g_rt.back, content.right - 282, button_y, 86, 30, TRUE);
    MoveWindow(g_rt.next, content.right - 188, button_y, 86, 30, TRUE);
    MoveWindow(g_rt.cancel, content.right - 94, button_y, 86, 30, TRUE);
    rt_show(g_rt.cancel, 1);
    InvalidateRect(hwnd, NULL, TRUE);
}
