#include "win32_runtime.h"
#include "win32_i18n.h"

#include <stdio.h>
#include <string.h>

HWND rt_label(HWND parent, const char *text, int x, int y, int w, int h, HFONT font)
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
                               g_rt.instance,
                               NULL);
    rt_set_font(hwnd, font);
    return hwnd;
}

HWND rt_button(HWND parent, int id, const char *text, int x, int y, int w, int h)
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
                               g_rt.instance,
                               NULL);
    rt_set_font(hwnd, g_rt.font_body);
    return hwnd;
}

HWND rt_check(HWND parent, int id, const char *text, int x, int y, int w, int h, int checked)
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
                               g_rt.instance,
                               NULL);
    rt_set_font(hwnd, g_rt.font_body);
    SendMessageA(hwnd, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    return hwnd;
}

HWND rt_edit(HWND parent, int id, const char *text, int x, int y, int w, int h)
{
    HWND hwnd = CreateWindowExA(WS_EX_CLIENTEDGE,
                               "EDIT",
                               text,
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                               x,
                               y,
                               w,
                               h,
                               parent,
                               (HMENU)(INT_PTR)id,
                               g_rt.instance,
                               NULL);
    rt_set_font(hwnd, g_rt.font_body);
    SendMessageA(hwnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(6, 6));
    return hwnd;
}

HWND rt_license_box(HWND parent)
{
    HWND hwnd = CreateWindowExA(WS_EX_CLIENTEDGE,
                               "EDIT",
                               "",
                               WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                               202,
                               112,
                               424,
                               140,
                               parent,
                               NULL,
                               g_rt.instance,
                               NULL);
    rt_set_font(hwnd, g_rt.font_body);
    return hwnd;
}

static HWND rt_progress_log_box(HWND parent)
{
    HWND hwnd = CreateWindowExA(WS_EX_CLIENTEDGE,
                               "EDIT",
                               "",
                               WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                               202,
                               270,
                               424,
                               78,
                               parent,
                               (HMENU)(INT_PTR)RT_ID_PROGRESS_LOG,
                               g_rt.instance,
                               NULL);
    rt_set_font(hwnd, g_rt.font_body);
    SendMessageA(hwnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(6, 6));
    return hwnd;
}

void rt_create_controls(HWND hwnd)
{
    int i;

    g_rt.title = rt_label(hwnd, "", 202, 22, 424, 24, g_rt.font_bold);
    g_rt.subtitle = rt_label(hwnd, "", 202, 52, 424, 34, g_rt.font_body);
    g_rt.body = rt_label(hwnd, "", 202, 124, 424, 150, g_rt.font_body);
    g_rt.license = rt_license_box(hwnd);
    g_rt.accept = rt_check(hwnd, RT_ID_ACCEPT, "I accept the agreement", 202, 274, 220, 22, 0);
    g_rt.install_dir = rt_edit(hwnd, RT_ID_INSTALLDIR, g_rt.info.install_dir, 202, 180, 332, 24);
    g_rt.browse = rt_button(hwnd, RT_ID_BROWSE, os_win32_text(OS_WIN32_TEXT_BROWSE), 546, 179, 80, 26);
    g_rt.component_main = rt_check(hwnd, RT_ID_MAINCOMP, "Program files", 222, 154, 240, 22, 1);
    g_rt.component_reg = rt_check(hwnd, RT_ID_REGCOMP, "Uninstaller entry", 222, 180, 240, 22, g_rt.info.register_system);
    for (i = 0; i < OS_MAX_ONLINE_COMPONENTS; ++i) {
        g_rt.online_components[i] = rt_check(hwnd,
                                             RT_ID_ONLINE_COMPONENT_BASE + i,
                                             "",
                                             222,
                                             208 + i * 24,
                                             360,
                                             22,
                                             0);
    }
    g_rt.progress = CreateWindowExA(0,
                                    PROGRESS_CLASSA,
                                    "",
                                    WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                    202,
                                    184,
                                    424,
                                    22,
                                    hwnd,
                                    (HMENU)(INT_PTR)RT_ID_PROGRESS,
                                    g_rt.instance,
                                    NULL);
    SendMessageA(g_rt.progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageA(g_rt.progress, PBM_SETBARCOLOR, 0, rt_theme_color(g_rt.info.theme.progress, RGB(22, 163, 74)));
    SendMessageA(g_rt.progress, PBM_SETBKCOLOR, 0, RGB(224, 230, 238));
    g_rt.progress_text = rt_label(hwnd, "", 202, 214, 424, 22, g_rt.font_body);
    g_rt.progress_detail = rt_label(hwnd, "", 202, 238, 308, 44, g_rt.font_body);
    g_rt.progress_more = rt_button(hwnd, RT_ID_PROGRESS_MORE, "Details", 520, 238, 106, 26);
    g_rt.progress_log = rt_progress_log_box(hwnd);

    g_rt.back = rt_button(hwnd, RT_ID_BACK, os_win32_text(OS_WIN32_TEXT_BACK), 386, 398, 82, 26);
    g_rt.next = rt_button(hwnd, RT_ID_NEXT, os_win32_text(OS_WIN32_TEXT_NEXT), 478, 398, 82, 26);
    g_rt.cancel = rt_button(hwnd, RT_ID_CANCEL, os_win32_text(OS_WIN32_TEXT_CANCEL), 572, 398, 82, 26);
}

void rt_paint(HWND hwnd)
{
    if (rt_modern_style_enabled()) {
        rt_modern_paint(hwnd);
        return;
    }

    if (rt_legacy_style_enabled()) {
        rt_legacy_paint(hwnd);
        return;
    }

    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT header = {RT_SIDE_W, 0, RT_W, 96};
    RECT background = {RT_SIDE_W, 96, RT_W, RT_FOOTER_Y};
    RECT left = {0, 0, RT_SIDE_W, RT_H};
    RECT footer = {0, RT_FOOTER_Y, RT_W, RT_H};
    RECT top_line = {0, 0, RT_SIDE_W, 5};
    HBRUSH blue = CreateSolidBrush(rt_theme_color(g_rt.info.theme.sidebar, RGB(21, 78, 158)));
    HBRUSH dark = CreateSolidBrush(rt_theme_color(g_rt.info.theme.sidebar_dark, RGB(8, 39, 102)));
    HBRUSH overlay = CreateSolidBrush(RGB(10, 34, 76));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(160, 160, 160));
    HFONT old_font;

    FillRect(dc, &header, g_rt.brush_white);
    if (g_rt.side_image != NULL) {
        HDC mem = CreateCompatibleDC(dc);
        BITMAP bm;
        HGDIOBJ old_bitmap = SelectObject(mem, g_rt.side_image);
        GetObject(g_rt.side_image, sizeof(bm), &bm);
        StretchBlt(dc, 0, 0, RT_SIDE_W, RT_FOOTER_Y, mem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
        SelectObject(mem, old_bitmap);
        DeleteDC(mem);
    } else {
        RECT top = {0, 0, RT_SIDE_W, RT_FOOTER_Y / 2};
        RECT bottom = {0, RT_FOOTER_Y / 2, RT_SIDE_W, RT_FOOTER_Y};
        FillRect(dc, &top, blue);
        FillRect(dc, &bottom, dark);
    }
    if (g_rt.background_image != NULL) {
        HDC mem = CreateCompatibleDC(dc);
        BITMAP bm;
        HGDIOBJ old_bitmap = SelectObject(mem, g_rt.background_image);
        GetObject(g_rt.background_image, sizeof(bm), &bm);
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchBlt(dc,
                   background.left,
                   background.top,
                   background.right - background.left,
                   background.bottom - background.top,
                   mem,
                   0,
                   0,
                   bm.bmWidth,
                   bm.bmHeight,
                   SRCCOPY);
        SelectObject(mem, old_bitmap);
        DeleteDC(mem);
    }
    FillRect(dc, &top_line, dark);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    old_font = (HFONT)SelectObject(dc, g_rt.font_title);
    {
        RECT brand_name = {22, 20, RT_SIDE_W - 12, 54};
        DrawTextA(dc,
                  g_rt.info.app_name,
                  -1,
                  &brand_name,
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    SelectObject(dc, old_font);
    old_font = (HFONT)SelectObject(dc, g_rt.font_body);
    SetTextColor(dc, RGB(220, 231, 245));
    {
        RECT publisher = {24, 64, RT_SIDE_W - 12, 88};
        DrawTextA(dc,
                  g_rt.info.company_name[0] != '\0' ? g_rt.info.company_name : "Setup Wizard",
                  -1,
                  &publisher,
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    SelectObject(dc, old_font);
    if (g_rt.side_image != NULL) {
        RECT shade = {0, RT_FOOTER_Y - 72, RT_SIDE_W, RT_FOOTER_Y};
        FillRect(dc, &shade, overlay);
    }
    {
        HBRUSH footer_brush = CreateSolidBrush(rt_theme_color(g_rt.info.theme.background, RGB(240, 240, 240)));
        FillRect(dc, &footer, footer_brush);
        DeleteObject(footer_brush);
    }
    SelectObject(dc, pen);
    MoveToEx(dc, RT_SIDE_W, 96, NULL);
    LineTo(dc, RT_W, 96);
    MoveToEx(dc, 0, RT_FOOTER_Y, NULL);
    LineTo(dc, RT_W, RT_FOOTER_Y);

    DeleteObject(pen);
    DeleteObject(blue);
    DeleteObject(dark);
    DeleteObject(overlay);
    EndPaint(hwnd, &ps);
}

void rt_set_progress(int value, const char *text)
{
    char line[160];

    if (value < 0) {
        value = 0;
    } else if (value > 100) {
        value = 100;
    }

    g_rt.progress_value = value;
    SendMessageA(g_rt.progress, PBM_SETPOS, value, 0);
    snprintf(line, sizeof(line), "%d%%  %s", value, text != NULL ? text : "");
    SetWindowTextA(g_rt.progress_text, line);
    if (rt_modern_style_enabled() || rt_legacy_style_enabled()) {
        InvalidateRect(g_rt.window, NULL, FALSE);
    }
    UpdateWindow(g_rt.progress);
    UpdateWindow(g_rt.progress_text);
}

static void rt_append_log_line(const char *line)
{
    size_t current_len = strlen(g_rt.progress_log_text);
    size_t line_len = strlen(line);

    if (line_len + 2 >= sizeof(g_rt.progress_log_text)) {
        return;
    }

    if (current_len + line_len + 2 >= sizeof(g_rt.progress_log_text)) {
        size_t keep_from = sizeof(g_rt.progress_log_text) / 3;
        while (g_rt.progress_log_text[keep_from] != '\0' &&
               g_rt.progress_log_text[keep_from] != '\n') {
            ++keep_from;
        }
        if (g_rt.progress_log_text[keep_from] == '\n') {
            ++keep_from;
        }
        memmove(g_rt.progress_log_text,
                g_rt.progress_log_text + keep_from,
                strlen(g_rt.progress_log_text + keep_from) + 1);
        current_len = strlen(g_rt.progress_log_text);
    }

    memcpy(g_rt.progress_log_text + current_len, line, line_len);
    current_len += line_len;
    g_rt.progress_log_text[current_len++] = '\r';
    g_rt.progress_log_text[current_len++] = '\n';
    g_rt.progress_log_text[current_len] = '\0';
}

void rt_apply_progress_update(void)
{
    char action[160];
    char source[OS_MAX_PATH_LEN];
    char target[OS_MAX_PATH_LEN];
    char detail[OS_MAX_PATH_LEN + 160];
    char log_line[OS_MAX_PATH_LEN + 220];
    size_t completed;
    size_t total;
    int percent;

    if (!g_rt.progress_lock_ready) {
        return;
    }

    EnterCriticalSection(&g_rt.progress_lock);
    rt_copy(action, sizeof(action), g_rt.progress_action);
    rt_copy(source, sizeof(source), g_rt.progress_source);
    rt_copy(target, sizeof(target), g_rt.progress_target);
    completed = g_rt.progress_completed;
    total = g_rt.progress_total;
    percent = g_rt.progress_percent;
    LeaveCriticalSection(&g_rt.progress_lock);

    g_rt.progress_has_real_events = 1;
    rt_set_progress(percent, action[0] != '\0' ? action : "Working...");

    if (total > 0) {
        snprintf(detail,
                 sizeof(detail),
                 "%llu of %llu file(s)\r\nTo: %s",
                 (unsigned long long)completed,
                 (unsigned long long)total,
                 target[0] != '\0' ? target : g_rt.operation_install_dir);
    } else {
        snprintf(detail,
                 sizeof(detail),
                 "Target:\r\n%s",
                 target[0] != '\0' ? target : g_rt.operation_install_dir);
    }
    SetWindowTextA(g_rt.progress_detail, detail);

    if (source[0] != '\0' && target[0] != '\0') {
        snprintf(log_line, sizeof(log_line), "%3d%%  %s: %s -> %s", percent, action, source, target);
    } else if (target[0] != '\0') {
        snprintf(log_line, sizeof(log_line), "%3d%%  %s: %s", percent, action, target);
    } else {
        snprintf(log_line, sizeof(log_line), "%3d%%  %s", percent, action);
    }
    rt_append_log_line(log_line);
    SetWindowTextA(g_rt.progress_log, g_rt.progress_log_text);
    SendMessageA(g_rt.progress_log, EM_LINESCROLL, 0, 0x7fff);
}

void rt_toggle_progress_details(void)
{
    g_rt.progress_expanded = !g_rt.progress_expanded;
    SetWindowTextA(g_rt.progress_more, g_rt.progress_expanded ? "Hide details" : "Details");
    rt_show(g_rt.progress_log, g_rt.progress_expanded);
    if (rt_modern_style_enabled()) {
        rt_modern_layout(g_rt.window);
    }
    if (rt_legacy_style_enabled()) {
        rt_legacy_layout(g_rt.window);
    }
}
