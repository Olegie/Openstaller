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
    if (hwnd == NULL) {
        return;
    }
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

static void bw_fill_rect(HDC dc, const RECT *rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);

    FillRect(dc, rect, brush);
    DeleteObject(brush);
}

static void bw_draw_section(HDC dc, const RECT *rect, const char *title)
{
    RECT header = {rect->left + 16, rect->top + 12, rect->right - 16, rect->top + 32};
    HPEN border = CreatePen(PS_SOLID, 1, RGB(221, 228, 236));
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    HFONT old_font;

    bw_fill_rect(dc, rect, RGB(255, 255, 255));
    old_pen = SelectObject(dc, border);
    old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, rect->left, rect->top, rect->right, rect->bottom);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(border);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(45, 55, 72));
    old_font = (HFONT)SelectObject(dc, g_bw.font_bold);
    DrawTextA(dc, title, -1, &header, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old_font);
}

static void bw_draw_page_sections(HDC dc)
{
    if (g_bw.page == BW_PAGE_APP) {
        RECT identity = {282, 124, 926, 338};
        bw_draw_section(dc, &identity, "Project identity");
    } else if (g_bw.page == BW_PAGE_FILES) {
        RECT files = {274, 108, 930, 474};
        bw_draw_section(dc, &files, "Package inputs");
    } else if (g_bw.page == BW_PAGE_TEXTS) {
        RECT copy = {282, 124, 1018, 548};
        bw_draw_section(dc, &copy, "Wizard page copy");
    } else if (g_bw.page == BW_PAGE_OPTIONS) {
        RECT behavior = {282, 116, 742, 468};
        RECT preview = {756, 116, 1076, 468};
        RECT targets = {282, 482, 1076, 626};
        bw_draw_section(dc, &behavior, "Template and behavior");
        bw_draw_section(dc, &preview, "Live preview");
        bw_draw_section(dc, &targets, "Online downloads and outputs");
    } else if (g_bw.page == BW_PAGE_BUILD || g_bw.page == BW_PAGE_DONE) {
        RECT summary = {282, 124, 910, 458};
        bw_draw_section(dc, &summary, g_bw.page == BW_PAGE_BUILD ? "Package summary" : "Result");
    }
}

void bw_create_controls(HWND hwnd)
{
    const char *text_tabs[] = {"Welcome", "Folder", "Components", "Ready", "Finish", "Uninstall"};
    int tab_index;

    g_bw.title = bw_label(hwnd, "Application", 292, 34, 600, 28, g_bw.font_bold);
    g_bw.subtitle = bw_label(hwnd, "", 292, 70, 600, 36, g_bw.font_body);

    g_bw.app_name_label = bw_label(hwnd, "Application name", 310, 154, 150, 20, g_bw.font_body);
    g_bw.app_name = bw_edit(hwnd, 0, "", 484, 150, 376, 26, 0);
    g_bw.company_name_label = bw_label(hwnd, "Company", 310, 198, 150, 20, g_bw.font_body);
    g_bw.company_name = bw_edit(hwnd, 0, "", 484, 194, 376, 26, 0);
    g_bw.app_version_label = bw_label(hwnd, "Version", 310, 242, 150, 20, g_bw.font_body);
    g_bw.app_version = bw_edit(hwnd, 0, "", 484, 238, 150, 26, 0);
    g_bw.install_dir_label = bw_label(hwnd, "Default install folder", 310, 286, 160, 20, g_bw.font_body);
    g_bw.install_dir = bw_edit(hwnd, 0, "", 484, 282, 376, 26, 0);

    g_bw.source_dir_label = bw_label(hwnd, "Program files folder", 292, 136, 160, 20, g_bw.font_body);
    g_bw.source_dir = bw_edit(hwnd, 0, "", 484, 132, 330, 26, 0);
    bw_button(hwnd, BW_ID_SOURCE_BROWSE, os_win32_text(OS_WIN32_TEXT_BROWSE), 828, 131, 84, 28);
    g_bw.output_dir_label = bw_label(hwnd, "Output folder", 292, 176, 160, 20, g_bw.font_body);
    g_bw.output_dir = bw_edit(hwnd, 0, "", 484, 172, 330, 26, 0);
    bw_button(hwnd, BW_ID_OUTPUT_BROWSE, os_win32_text(OS_WIN32_TEXT_BROWSE), 828, 171, 84, 28);
    g_bw.license_file_label = bw_label(hwnd, "License file", 292, 216, 160, 20, g_bw.font_body);
    g_bw.license_file = bw_edit(hwnd, 0, "", 484, 212, 330, 26, 0);
    bw_button(hwnd, BW_ID_LICENSE_BROWSE, os_win32_text(OS_WIN32_TEXT_BROWSE), 828, 211, 84, 28);
    g_bw.wizard_image_label = bw_label(hwnd, "Installer side image", 292, 256, 160, 20, g_bw.font_body);
    g_bw.wizard_image = bw_edit(hwnd, 0, "", 484, 252, 330, 26, 0);
    bw_button(hwnd, BW_ID_IMAGE_BROWSE, os_win32_text(OS_WIN32_TEXT_BROWSE), 828, 251, 84, 28);
    g_bw.background_image_label = bw_label(hwnd, "Installer background", 292, 296, 160, 20, g_bw.font_body);
    g_bw.background_image = bw_edit(hwnd, 0, "", 484, 292, 330, 26, 0);
    bw_button(hwnd, BW_ID_BACKGROUND_IMAGE_BROWSE, os_win32_text(OS_WIN32_TEXT_BROWSE), 828, 291, 84, 28);
    g_bw.launcher_label = bw_label(hwnd, "Program to run", 292, 336, 160, 20, g_bw.font_body);
    g_bw.launcher = bw_edit(hwnd, 0, "", 484, 332, 428, 26, 0);
    g_bw.installer_icon_label = bw_label(hwnd, "Installer icon", 292, 392, 160, 20, g_bw.font_body);
    g_bw.installer_icon = bw_edit(hwnd, 0, "", 484, 388, 330, 26, 0);
    bw_button(hwnd, BW_ID_INSTALLER_ICON_BROWSE, os_win32_text(OS_WIN32_TEXT_BROWSE), 828, 387, 84, 28);
    g_bw.uninstaller_icon_label = bw_label(hwnd, "Uninstaller icon", 292, 432, 160, 20, g_bw.font_body);
    g_bw.uninstaller_icon = bw_edit(hwnd, 0, "", 484, 428, 330, 26, 0);
    bw_button(hwnd, BW_ID_UNINSTALLER_ICON_BROWSE, os_win32_text(OS_WIN32_TEXT_BROWSE), 828, 427, 84, 28);

    g_bw.text_page_label = bw_label(hwnd, "Edit page", 310, 150, 90, 20, g_bw.font_body);
    g_bw.text_tabs = CreateWindowExA(0,
                                     WC_TABCONTROLA,
                                     "",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_FIXEDWIDTH,
                                     310,
                                     174,
                                     690,
                                     32,
                                     hwnd,
                                     (HMENU)(INT_PTR)BW_ID_TEXT_PAGE,
                                     g_bw.instance,
                                     NULL);
    bw_set_font(g_bw.text_tabs, g_bw.font_body);
    SendMessageA(g_bw.text_tabs, TCM_SETITEMSIZE, 0, MAKELPARAM(106, 24));
    for (tab_index = 0; tab_index < 6; ++tab_index) {
        TCITEMA item;

        memset(&item, 0, sizeof(item));
        item.mask = TCIF_TEXT;
        item.pszText = (char *)text_tabs[tab_index];
        TabCtrl_InsertItem(g_bw.text_tabs, tab_index, &item);
    }
    TabCtrl_SetCurSel(g_bw.text_tabs, 0);
    g_bw.page_title_label = bw_label(hwnd, "Title", 310, 226, 90, 20, g_bw.font_body);
    g_bw.page_title = bw_edit(hwnd, 0, "", 430, 222, 500, 26, 0);
    g_bw.page_body_label = bw_label(hwnd, "Text", 310, 274, 90, 20, g_bw.font_body);
    g_bw.page_body = bw_edit(hwnd, 0, "", 430, 270, 500, 224, 1);

    g_bw.installer_style_label = bw_label(hwnd, "Template", 310, 150, 120, 20, g_bw.font_body);
    g_bw.installer_style = bw_combo(hwnd, BW_ID_INSTALLER_STYLE, 446, 146, 244, 120);
    SendMessageA(g_bw.installer_style, CB_ADDSTRING, 0, (LPARAM)"Classic wizard");
    SendMessageA(g_bw.installer_style, CB_ADDSTRING, 0, (LPARAM)"Modern suite");
    SendMessageA(g_bw.installer_style, CB_ADDSTRING, 0, (LPARAM)"Legacy full-screen");
    SendMessageA(g_bw.installer_style, CB_SETCURSEL, 0, 0);
    g_bw.ui_font_label = bw_label(hwnd, "UI font", 310, 188, 120, 20, g_bw.font_body);
    g_bw.ui_font = bw_combo(hwnd, BW_ID_UI_FONT, 446, 184, 244, 144);
    SendMessageA(g_bw.ui_font, CB_ADDSTRING, 0, (LPARAM)"System default");
    SendMessageA(g_bw.ui_font, CB_ADDSTRING, 0, (LPARAM)"Segoe UI");
    SendMessageA(g_bw.ui_font, CB_ADDSTRING, 0, (LPARAM)"Tahoma");
    SendMessageA(g_bw.ui_font, CB_ADDSTRING, 0, (LPARAM)"Trebuchet MS");
    SendMessageA(g_bw.ui_font, CB_ADDSTRING, 0, (LPARAM)"Georgia");
    SendMessageA(g_bw.ui_font, CB_ADDSTRING, 0, (LPARAM)"Consolas");
    SendMessageA(g_bw.ui_font, CB_SETCURSEL, 0, 0);
    g_bw.window_style_label = bw_label(hwnd, "Window style", 310, 226, 120, 20, g_bw.font_body);
    g_bw.window_style = bw_combo(hwnd, BW_ID_WINDOW_STYLE, 446, 222, 244, 96);
    SendMessageA(g_bw.window_style, CB_ADDSTRING, 0, (LPARAM)"Fixed native");
    SendMessageA(g_bw.window_style, CB_ADDSTRING, 0, (LPARAM)"Resizable native");
    SendMessageA(g_bw.window_style, CB_ADDSTRING, 0, (LPARAM)"Start maximized");
    SendMessageA(g_bw.window_style, CB_SETCURSEL, 0, 0);
    g_bw.page_flow_label = bw_label(hwnd, "Wizard pages", 310, 264, 120, 20, g_bw.font_body);
    {
        const char *page_names[] = {"Welcome", "License", "Folder", "Components", "Ready", "Finish"};
        int i;

        for (i = 0; i < 6; ++i) {
            int x = 446 + (i % 3) * 98;
            int y = 258 + (i / 3) * 24;

            g_bw.page_flow_checks[i] = bw_check(hwnd, BW_ID_PAGE_FLOW_BASE + i, page_names[i], x, y, 96, 22, 1);
        }
    }
    g_bw.page_flow_count = bw_label(hwnd, "6 pages", 446, 308, 244, 18, g_bw.font_body);
    g_bw.theme_colors_label = bw_label(hwnd, "Theme colors", 310, 338, 120, 20, g_bw.font_body);
    g_bw.theme_reset = bw_button(hwnd, BW_ID_THEME_RESET, "Reset", 446, 334, 72, 24);
    {
        const char *theme_names[BW_THEME_COUNT] = {
            "Accent", "Progress", "Sidebar", "Side dark", "Background",
            "Panel", "Text", "Muted", "Legacy top", "Legacy bottom"
        };
        int i;

        for (i = 0; i < BW_THEME_COUNT; ++i) {
            int col = i / 5;
            int row = i % 5;
            int x = col == 0 ? 310 : 532;
            int y = 364 + row * 24;

            g_bw.theme_name[i] = bw_label(hwnd, theme_names[i], x, y + 3, 78, 18, g_bw.font_body);
            g_bw.theme_value[i] = bw_edit(hwnd, BW_ID_THEME_COLORS + i, "", x + 82, y, 96, 22, 0);
            g_bw.theme_pick[i] = bw_button(hwnd, BW_ID_THEME_PICK_BASE + i, "...", x + 184, y - 1, 28, 24);
        }
    }
    g_bw.online_components_label = bw_label(hwnd, "Online downloads", 306, 506, 130, 20, g_bw.font_body);
    {
        const char *headers[5] = {"Name", "URL", "Target", "Ask on", "Default"};
        int header_x[5] = {306, 412, 714, 868, 982};
        int header_w[5] = {94, 292, 142, 96, 70};
        int i;

        for (i = 0; i < 5; ++i) {
            g_bw.online_header[i] = bw_label(hwnd, headers[i], header_x[i], 530, header_w[i], 18, g_bw.font_body);
        }
        for (i = 0; i < BW_ONLINE_ROWS; ++i) {
            int y = 552 + i * 24;

            g_bw.online_name[i] = bw_edit(hwnd, 0, "", 306, y, 96, 22, 0);
            g_bw.online_url[i] = bw_edit(hwnd, 0, "", 412, y, 292, 22, 0);
            g_bw.online_target[i] = bw_edit(hwnd, 0, "", 714, y, 142, 22, 0);
            g_bw.online_page[i] = bw_combo(hwnd, BW_ID_ONLINE_PAGE_BASE + i, 868, y - 1, 104, 96);
            SendMessageA(g_bw.online_page[i], CB_ADDSTRING, 0, (LPARAM)"Components");
            SendMessageA(g_bw.online_page[i], CB_ADDSTRING, 0, (LPARAM)"Ready");
            SendMessageA(g_bw.online_page[i], CB_ADDSTRING, 0, (LPARAM)"Install stage");
            SendMessageA(g_bw.online_page[i], CB_SETCURSEL, 0, 0);
            g_bw.online_default[i] = bw_check(hwnd, 0, "", 1004, y, 24, 22, 1);
        }
    }
    g_bw.native_box = bw_check(hwnd, 0, "Self-contained installer.exe", 306, 628, 210, 22, 1);
    g_bw.register_box = bw_check(hwnd, 0, "System uninstall entry", 520, 628, 190, 22, 1);
    g_bw.windows_box = bw_check(hwnd, 0, "Windows script fallback", 714, 628, 180, 22, 0);
    g_bw.unix_box = bw_check(hwnd, 0, "Linux/macOS script fallback", 902, 628, 190, 22, 0);

    g_bw.status = bw_label(hwnd, "", 310, 150, 560, 250, g_bw.font_body);
    g_bw.open_output = bw_button(hwnd, BW_ID_OPEN_OUTPUT, "Open Folder", 310, 430, 112, 28);

    g_bw.back = bw_button(hwnd, BW_ID_BACK, os_win32_text(OS_WIN32_TEXT_BACK), 844, 666, 82, 28);
    g_bw.next = bw_button(hwnd, BW_ID_NEXT, os_win32_text(OS_WIN32_TEXT_NEXT), 936, 666, 82, 28);
    g_bw.cancel = bw_button(hwnd, BW_ID_CANCEL, os_win32_text(OS_WIN32_TEXT_CANCEL), 1030, 666, 82, 28);
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
    bw_draw_page_sections(dc);

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
        RECT preview = {774, 152, 1058, 452};
        bw_draw_template_preview(dc, &preview);
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
