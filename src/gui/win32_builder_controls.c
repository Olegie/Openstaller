#include "win32_builder.h"
#include "win32_i18n.h"

#include <stdio.h>
#include <string.h>

#if !defined(OPENSTALLER_WIN2000_COMPAT) && defined(CLEARTYPE_QUALITY)
#define BW_FONT_QUALITY CLEARTYPE_QUALITY
#elif defined(ANTIALIASED_QUALITY)
#define BW_FONT_QUALITY ANTIALIASED_QUALITY
#else
#define BW_FONT_QUALITY DEFAULT_QUALITY
#endif

static int bw_font_height(int points)
{
    HDC dc = GetDC(NULL);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    return -MulDiv(points, dpi, 72);
}

HFONT bw_font(int points, int weight)
{
    return CreateFontA(bw_font_height(points),
                       0,
                       0,
                       0,
                       weight,
                       FALSE,
                       FALSE,
                       FALSE,
                       DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS,
                       BW_FONT_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE,
                       "MS Shell Dlg");
}

void bw_set_font(HWND hwnd, HFONT font)
{
    SendMessageA(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
}

void bw_show(HWND hwnd, int visible)
{
    ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
}

HWND bw_label(HWND parent, const char *text, int x, int y, int w, int h, HFONT font)
{
    HWND hwnd = CreateWindowExA(0,
                               "STATIC",
                               text,
                               WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                               x,
                               y,
                               w,
                               h,
                               parent,
                               NULL,
                               g_bw.instance,
                               NULL);
    bw_set_font(hwnd, font);
    return hwnd;
}

HWND bw_edit(HWND parent, int id, const char *text, int x, int y, int w, int h, int multiline)
{
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL;
    HWND hwnd;

    if (multiline) {
        style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL;
    }

    hwnd = CreateWindowExA(WS_EX_CLIENTEDGE,
                           "EDIT",
                           text,
                           style,
                           x,
                           y,
                           w,
                           h,
                           parent,
                           (HMENU)(INT_PTR)id,
                           g_bw.instance,
                           NULL);
    bw_set_font(hwnd, g_bw.font_body);
    SendMessageA(hwnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(6, 6));
    return hwnd;
}

HWND bw_button(HWND parent, int id, const char *text, int x, int y, int w, int h)
{
    HWND hwnd = CreateWindowExA(0,
                               "BUTTON",
                               text,
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                               x,
                               y,
                               w,
                               h,
                               parent,
                               (HMENU)(INT_PTR)id,
                               g_bw.instance,
                               NULL);
    bw_set_font(hwnd, g_bw.font_body);
    return hwnd;
}

HWND bw_check(HWND parent, int id, const char *text, int x, int y, int w, int h, int checked)
{
    HWND hwnd = CreateWindowExA(0,
                               "BUTTON",
                               text,
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                               x,
                               y,
                               w,
                               h,
                               parent,
                               (HMENU)(INT_PTR)id,
                               g_bw.instance,
                               NULL);
    bw_set_font(hwnd, g_bw.font_body);
    SendMessageA(hwnd, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    return hwnd;
}

HWND bw_combo(HWND parent, int id, int x, int y, int w, int h)
{
    HWND hwnd = CreateWindowExA(WS_EX_CLIENTEDGE,
                               "COMBOBOX",
                               "",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                               x,
                               y,
                               w,
                               h,
                               parent,
                               (HMENU)(INT_PTR)id,
                               g_bw.instance,
                               NULL);
    bw_set_font(hwnd, g_bw.font_body);
    return hwnd;
}

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

static void bw_draw_classic_preview(HDC dc, RECT rect)
{
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    int side_w = w > 140 ? 42 : w / 4;
    int header_h = h > 100 ? 28 : h / 4;
    int footer_h = h > 100 ? 20 : h / 5;
    RECT side = {rect.left, rect.top, rect.left + side_w, rect.bottom};
    RECT header = {side.right, rect.top, rect.right, rect.top + header_h};
    RECT body = {side.right, header.bottom, rect.right, rect.bottom - footer_h};
    RECT footer = {side.right, body.bottom, rect.right, rect.bottom};
    RECT bar = {body.left + 8, body.bottom - 14, body.right - 8, body.bottom - 8};

    bw_fill(dc, &rect, bw_theme_color(g_bw.config.theme.background, RGB(240, 240, 240)));
    bw_fill(dc, &side, bw_theme_color(g_bw.config.theme.sidebar, RGB(21, 78, 158)));
    bw_fill(dc, &header, bw_theme_color(g_bw.config.theme.panel, RGB(255, 255, 255)));
    bw_fill(dc, &body, bw_theme_color(g_bw.config.theme.background, RGB(240, 240, 240)));
    bw_fill(dc, &footer, bw_theme_color(g_bw.config.theme.panel, RGB(255, 255, 255)));
    bw_fill(dc, &bar, RGB(224, 230, 238));
    bar.right = bar.left + ((bar.right - bar.left) * 62) / 100;
    bw_fill(dc, &bar, bw_theme_color(g_bw.config.theme.progress, RGB(22, 163, 74)));
}

static void bw_draw_modern_preview(HDC dc, RECT rect)
{
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    int rail_w = w > 140 ? 44 : w / 4;
    int margin = w > 140 ? 14 : 4;
    int card_w;
    int card_h;
    int radius;
    int cx;
    int cy;
    RECT rail = {rect.left, rect.top, rect.left + rail_w, rect.bottom};
    RECT card = {rail.right + margin, rect.top + h / 4, rect.right - margin, rect.bottom - h / 5};
    RECT dot = {rail.left + rail_w / 2 - 5, rail.top + h / 4, rail.left + rail_w / 2 + 5, rail.top + h / 4 + 10};
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
    cy = (card.top + card.bottom) / 2;

    bw_fill(dc, &rect, bw_theme_color(g_bw.config.theme.background, RGB(255, 255, 255)));
    bw_fill(dc, &rail, bw_theme_color(g_bw.config.theme.sidebar, RGB(243, 244, 246)));
    bw_fill(dc, &card, bw_theme_color(g_bw.config.theme.panel, RGB(255, 255, 255)));
    bw_preview_frame(dc, &card, 0);
    bw_fill(dc, &dot, bw_theme_color(g_bw.config.theme.accent, RGB(0, 120, 212)));

    ring = CreatePen(PS_SOLID, 4, bw_theme_color(g_bw.config.theme.progress, RGB(0, 120, 212)));
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
    RECT meters = {rect.left + 6, rect.bottom - h / 2, rect.left + 6 + w / 4, rect.bottom - 6};
    RECT panel = {rect.right - w / 2 - 6, rect.bottom - h / 2, rect.right - 6, rect.bottom - 6};
    RECT bar = {panel.left + 5, panel.bottom - h / 6, panel.right - 5, panel.bottom - h / 9};

    bw_draw_legacy_gradient(dc, &rect);
    bw_fill(dc, &meters, bw_theme_color(g_bw.config.theme.panel, RGB(238, 238, 224)));
    DrawEdge(dc, &meters, EDGE_RAISED, BF_RECT);
    bw_fill(dc, &panel, bw_theme_color(g_bw.config.theme.panel, RGB(238, 238, 224)));
    DrawEdge(dc, &panel, EDGE_RAISED, BF_RECT);
    bw_fill(dc, &bar, RGB(255, 255, 248));
    bar.right = bar.left + ((bar.right - bar.left) * 54) / 100;
    bw_fill(dc, &bar, bw_theme_color(g_bw.config.theme.progress, RGB(174, 0, 134)));
}

static void bw_draw_template_preview(HDC dc)
{
    RECT title = {626, 118, 790, 136};
    RECT classic = {626, 144, 674, 182};
    RECT modern = {684, 144, 732, 182};
    RECT legacy = {742, 144, 790, 182};
    RECT selected = {626, 202, 790, 344};
    RECT label = {626, 184, 790, 202};
    const char *names[] = {"Classic", "Modern", "Legacy"};
    int style = g_bw.config.installer_style;
    HFONT old_font = (HFONT)SelectObject(dc, g_bw.font_body);

    if (style < OS_INSTALLER_STYLE_CLASSIC || style > OS_INSTALLER_STYLE_LEGACY) {
        style = OS_INSTALLER_STYLE_CLASSIC;
    }

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(87, 96, 106));
    DrawTextA(dc, "Preview", -1, &title, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

    bw_draw_classic_preview(dc, classic);
    bw_preview_frame(dc, &classic, style == OS_INSTALLER_STYLE_CLASSIC);
    bw_draw_modern_preview(dc, modern);
    bw_preview_frame(dc, &modern, style == OS_INSTALLER_STYLE_MODERN);
    bw_draw_legacy_preview(dc, legacy);
    bw_preview_frame(dc, &legacy, style == OS_INSTALLER_STYLE_LEGACY);

    SetTextColor(dc, RGB(0, 0, 0));
    DrawTextA(dc, names[style], -1, &label, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    if (style == OS_INSTALLER_STYLE_MODERN) {
        bw_draw_modern_preview(dc, selected);
    } else if (style == OS_INSTALLER_STYLE_LEGACY) {
        bw_draw_legacy_preview(dc, selected);
    } else {
        bw_draw_classic_preview(dc, selected);
    }
    bw_preview_frame(dc, &selected, 0);
    SelectObject(dc, old_font);
}

void bw_create_controls(HWND hwnd)
{
    g_bw.title = bw_label(hwnd, "Application", 282, 28, 520, 28, g_bw.font_bold);
    g_bw.subtitle = bw_label(hwnd, "", 282, 62, 520, 36, g_bw.font_body);

    g_bw.app_name_label = bw_label(hwnd, "Application name", 282, 142, 140, 20, g_bw.font_body);
    g_bw.app_name = bw_edit(hwnd, 0, "", 442, 138, 348, 26, 0);
    g_bw.company_name_label = bw_label(hwnd, "Company", 282, 184, 140, 20, g_bw.font_body);
    g_bw.company_name = bw_edit(hwnd, 0, "", 442, 180, 348, 26, 0);
    g_bw.app_version_label = bw_label(hwnd, "Version", 282, 226, 140, 20, g_bw.font_body);
    g_bw.app_version = bw_edit(hwnd, 0, "", 442, 222, 130, 26, 0);
    g_bw.install_dir_label = bw_label(hwnd, "Default install folder", 282, 268, 150, 20, g_bw.font_body);
    g_bw.install_dir = bw_edit(hwnd, 0, "", 442, 264, 348, 26, 0);

    g_bw.source_dir_label = bw_label(hwnd, "Program files folder", 282, 122, 150, 20, g_bw.font_body);
    g_bw.source_dir = bw_edit(hwnd, 0, "", 442, 118, 260, 26, 0);
    bw_button(hwnd, BW_ID_SOURCE_BROWSE, os_win32_text(OS_WIN32_TEXT_BROWSE), 716, 117, 74, 28);
    g_bw.output_dir_label = bw_label(hwnd, "Output folder", 282, 158, 150, 20, g_bw.font_body);
    g_bw.output_dir = bw_edit(hwnd, 0, "", 442, 154, 260, 26, 0);
    bw_button(hwnd, BW_ID_OUTPUT_BROWSE, os_win32_text(OS_WIN32_TEXT_BROWSE), 716, 153, 74, 28);
    g_bw.license_file_label = bw_label(hwnd, "License file", 282, 194, 150, 20, g_bw.font_body);
    g_bw.license_file = bw_edit(hwnd, 0, "", 442, 190, 260, 26, 0);
    bw_button(hwnd, BW_ID_LICENSE_BROWSE, os_win32_text(OS_WIN32_TEXT_BROWSE), 716, 189, 74, 28);
    g_bw.wizard_image_label = bw_label(hwnd, "Installer side image", 282, 230, 150, 20, g_bw.font_body);
    g_bw.wizard_image = bw_edit(hwnd, 0, "", 442, 226, 260, 26, 0);
    bw_button(hwnd, BW_ID_IMAGE_BROWSE, os_win32_text(OS_WIN32_TEXT_BROWSE), 716, 225, 74, 28);
    g_bw.background_image_label = bw_label(hwnd, "Installer background", 282, 266, 150, 20, g_bw.font_body);
    g_bw.background_image = bw_edit(hwnd, 0, "", 442, 262, 260, 26, 0);
    bw_button(hwnd, BW_ID_BACKGROUND_IMAGE_BROWSE, os_win32_text(OS_WIN32_TEXT_BROWSE), 716, 261, 74, 28);
    g_bw.launcher_label = bw_label(hwnd, "Program to run", 282, 302, 150, 20, g_bw.font_body);
    g_bw.launcher = bw_edit(hwnd, 0, "", 442, 298, 348, 26, 0);
    g_bw.installer_icon_label = bw_label(hwnd, "Installer icon", 282, 338, 150, 20, g_bw.font_body);
    g_bw.installer_icon = bw_edit(hwnd, 0, "", 442, 334, 260, 26, 0);
    bw_button(hwnd, BW_ID_INSTALLER_ICON_BROWSE, os_win32_text(OS_WIN32_TEXT_BROWSE), 716, 333, 74, 28);
    g_bw.uninstaller_icon_label = bw_label(hwnd, "Uninstaller icon", 282, 374, 150, 20, g_bw.font_body);
    g_bw.uninstaller_icon = bw_edit(hwnd, 0, "", 442, 370, 260, 26, 0);
    bw_button(hwnd, BW_ID_UNINSTALLER_ICON_BROWSE, os_win32_text(OS_WIN32_TEXT_BROWSE), 716, 369, 74, 28);

    g_bw.text_page_label = bw_label(hwnd, "Page", 282, 132, 90, 20, g_bw.font_body);
    g_bw.text_page = bw_combo(hwnd, BW_ID_TEXT_PAGE, 442, 128, 240, 160);
    SendMessageA(g_bw.text_page, CB_ADDSTRING, 0, (LPARAM)"Welcome");
    SendMessageA(g_bw.text_page, CB_ADDSTRING, 0, (LPARAM)"Install folder");
    SendMessageA(g_bw.text_page, CB_ADDSTRING, 0, (LPARAM)"Components");
    SendMessageA(g_bw.text_page, CB_ADDSTRING, 0, (LPARAM)"Ready to install");
    SendMessageA(g_bw.text_page, CB_ADDSTRING, 0, (LPARAM)"Finish");
    SendMessageA(g_bw.text_page, CB_ADDSTRING, 0, (LPARAM)"Uninstaller");
    SendMessageA(g_bw.text_page, CB_SETCURSEL, 0, 0);
    g_bw.page_title_label = bw_label(hwnd, "Title", 282, 174, 90, 20, g_bw.font_body);
    g_bw.page_title = bw_edit(hwnd, 0, "", 442, 170, 348, 26, 0);
    g_bw.page_body_label = bw_label(hwnd, "Text", 282, 216, 90, 20, g_bw.font_body);
    g_bw.page_body = bw_edit(hwnd, 0, "", 442, 212, 348, 128, 1);

    g_bw.installer_style_label = bw_label(hwnd, "Template", 282, 118, 120, 20, g_bw.font_body);
    g_bw.installer_style = bw_combo(hwnd, BW_ID_INSTALLER_STYLE, 416, 114, 188, 120);
    SendMessageA(g_bw.installer_style, CB_ADDSTRING, 0, (LPARAM)"Classic wizard");
    SendMessageA(g_bw.installer_style, CB_ADDSTRING, 0, (LPARAM)"Modern suite");
    SendMessageA(g_bw.installer_style, CB_ADDSTRING, 0, (LPARAM)"Legacy full-screen");
    SendMessageA(g_bw.installer_style, CB_SETCURSEL, 0, 0);
    g_bw.ui_font_label = bw_label(hwnd, "UI font", 282, 154, 120, 20, g_bw.font_body);
    g_bw.ui_font = bw_combo(hwnd, BW_ID_UI_FONT, 416, 150, 188, 144);
    SendMessageA(g_bw.ui_font, CB_ADDSTRING, 0, (LPARAM)"System default");
    SendMessageA(g_bw.ui_font, CB_ADDSTRING, 0, (LPARAM)"Segoe UI");
    SendMessageA(g_bw.ui_font, CB_ADDSTRING, 0, (LPARAM)"Tahoma");
    SendMessageA(g_bw.ui_font, CB_ADDSTRING, 0, (LPARAM)"Trebuchet MS");
    SendMessageA(g_bw.ui_font, CB_ADDSTRING, 0, (LPARAM)"Georgia");
    SendMessageA(g_bw.ui_font, CB_ADDSTRING, 0, (LPARAM)"Consolas");
    SendMessageA(g_bw.ui_font, CB_SETCURSEL, 0, 0);
    g_bw.window_style_label = bw_label(hwnd, "Window style", 282, 190, 120, 20, g_bw.font_body);
    g_bw.window_style = bw_combo(hwnd, BW_ID_WINDOW_STYLE, 416, 186, 188, 96);
    SendMessageA(g_bw.window_style, CB_ADDSTRING, 0, (LPARAM)"Fixed native");
    SendMessageA(g_bw.window_style, CB_ADDSTRING, 0, (LPARAM)"Resizable native");
    SendMessageA(g_bw.window_style, CB_ADDSTRING, 0, (LPARAM)"Start maximized");
    SendMessageA(g_bw.window_style, CB_SETCURSEL, 0, 0);
    g_bw.page_flow_label = bw_label(hwnd, "Pages", 282, 226, 120, 20, g_bw.font_body);
    g_bw.page_flow = bw_edit(hwnd, BW_ID_PAGE_FLOW, "", 416, 222, 188, 26, 0);
    g_bw.theme_colors_label = bw_label(hwnd, "Theme colors", 282, 264, 120, 20, g_bw.font_body);
    g_bw.theme_colors = bw_edit(hwnd, BW_ID_THEME_COLORS, "", 416, 260, 188, 92, 1);
    g_bw.online_components_label = bw_label(hwnd, "Online downloads", 282, 368, 120, 20, g_bw.font_body);
    g_bw.online_components = bw_edit(hwnd, BW_ID_ONLINE_COMPONENTS, "", 416, 364, 374, 40, 1);
    g_bw.native_box = bw_check(hwnd, 0, "Create self-contained installer.exe", 282, 412, 294, 22, 1);
    g_bw.register_box = bw_check(hwnd, 0, "Add uninstall entry to the system", 282, 436, 282, 22, 1);
    g_bw.windows_box = bw_check(hwnd, 0, "Windows script fallback", 586, 412, 204, 22, 0);
    g_bw.unix_box = bw_check(hwnd, 0, "Linux/macOS script fallback", 586, 436, 220, 22, 0);

    g_bw.status = bw_label(hwnd, "", 282, 132, 508, 190, g_bw.font_body);
    g_bw.open_output = bw_button(hwnd, BW_ID_OPEN_OUTPUT, "Open Folder", 282, 338, 112, 28);

    g_bw.back = bw_button(hwnd, BW_ID_BACK, os_win32_text(OS_WIN32_TEXT_BACK), 586, 486, 78, 28);
    g_bw.next = bw_button(hwnd, BW_ID_NEXT, os_win32_text(OS_WIN32_TEXT_NEXT), 674, 486, 78, 28);
    g_bw.cancel = bw_button(hwnd, BW_ID_CANCEL, os_win32_text(OS_WIN32_TEXT_CANCEL), 762, 486, 78, 28);
}

void bw_paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT full = {0, 0, BW_W, BW_H};
    RECT rail = {0, 0, BW_RAIL_W, BW_H};
    RECT footer = {BW_RAIL_W, BW_FOOTER_Y, BW_W, BW_H};
    RECT content = {BW_RAIL_W, 0, BW_W, BW_H};
    HBRUSH rail_brush = CreateSolidBrush(RGB(22, 27, 38));
    HBRUSH content_brush = CreateSolidBrush(RGB(246, 248, 251));
    HBRUSH footer_brush = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH accent = CreateSolidBrush(RGB(28, 122, 242));
    HBRUSH done = CreateSolidBrush(RGB(22, 163, 74));
    HPEN divider = CreatePen(PS_SOLID, 1, RGB(218, 224, 232));
    HPEN muted_pen = CreatePen(PS_SOLID, 1, RGB(77, 87, 103));
    HFONT old_font;
    const char *steps[] = {"Application", "Files", "Pages", "Options", "Build", "Finish"};
    int i;

    FillRect(dc, &full, content_brush);
    FillRect(dc, &rail, rail_brush);
    FillRect(dc, &content, content_brush);
    FillRect(dc, &footer, footer_brush);
    SelectObject(dc, divider);
    MoveToEx(dc, BW_RAIL_W, 0, NULL);
    LineTo(dc, BW_RAIL_W, BW_H);
    MoveToEx(dc, BW_RAIL_W, BW_FOOTER_Y, NULL);
    LineTo(dc, BW_W, BW_FOOTER_Y);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    old_font = (HFONT)SelectObject(dc, g_bw.font_title);
    TextOutA(dc, 26, 28, "Openstaller", 11);
    SelectObject(dc, old_font);
    old_font = (HFONT)SelectObject(dc, g_bw.font_body);
    SetTextColor(dc, RGB(154, 164, 179));
    TextOutA(dc, 28, 64, "Setup package builder", 21);

    for (i = 0; i < 6; ++i) {
        int y = BW_STEP_START_Y + i * BW_STEP_GAP_Y;
        RECT circle = {30, y - 5, 50, y + 15};
        HBRUSH step_brush = i < (int)g_bw.page ? done : i == (int)g_bw.page ? accent : rail_brush;
        HPEN step_pen = i <= (int)g_bw.page ? CreatePen(PS_SOLID, 1, i == (int)g_bw.page ? RGB(28, 122, 242) : RGB(22, 163, 74))
                                            : CreatePen(PS_SOLID, 1, RGB(77, 87, 103));
        char number[4];

        SelectObject(dc, step_pen);
        SelectObject(dc, step_brush);
        Ellipse(dc, circle.left, circle.top, circle.right, circle.bottom);
        snprintf(number, sizeof(number), "%d", i + 1);
        SetTextColor(dc, RGB(255, 255, 255));
        TextOutA(dc, i == 5 ? 36 : 37, y - 2, number, (int)strlen(number));
        SetTextColor(dc, i == (int)g_bw.page ? RGB(255, 255, 255) : RGB(154, 164, 179));
        TextOutA(dc, 66, y - 2, steps[i], (int)strlen(steps[i]));

        if (i < 5) {
            SelectObject(dc, muted_pen);
            MoveToEx(dc, 40, y + 18, NULL);
            LineTo(dc, 40, y + 39);
        }
        DeleteObject(step_pen);
    }
    SelectObject(dc, old_font);

    if (g_bw.page == BW_PAGE_OPTIONS) {
        bw_draw_template_preview(dc);
    }

    DeleteObject(divider);
    DeleteObject(muted_pen);
    DeleteObject(rail_brush);
    DeleteObject(content_brush);
    DeleteObject(footer_brush);
    DeleteObject(accent);
    DeleteObject(done);
    EndPaint(hwnd, &ps);
}
