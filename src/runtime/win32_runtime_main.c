#include "win32_runtime.h"
#include "win32_i18n.h"

#include <objbase.h>

#include <stdio.h>
#include <string.h>

RtState g_rt;

static int rt_temp_file(char *out, size_t out_size, const char *prefix)
{
    char temp_dir[MAX_PATH];
    char temp_path[MAX_PATH];
    DWORD len = GetTempPathA(sizeof(temp_dir), temp_dir);

    if (len == 0 || len >= sizeof(temp_dir)) {
        return -1;
    }

    if (GetTempFileNameA(temp_dir, prefix, 0, temp_path) == 0) {
        return -1;
    }

    if (strlen(temp_path) >= out_size) {
        DeleteFileA(temp_path);
        return -1;
    }

    strcpy(out, temp_path);
    return 0;
}

static void rt_parse_first_arg(const char *cmd_line, char *out, size_t out_size)
{
    size_t written = 0;
    char quote = '\0';

    if (out_size == 0) {
        return;
    }
    out[0] = '\0';

    while (*cmd_line == ' ' || *cmd_line == '\t') {
        ++cmd_line;
    }

    if (*cmd_line == '"' || *cmd_line == '\'') {
        quote = *cmd_line++;
    }

    while (*cmd_line != '\0') {
        if ((quote != '\0' && *cmd_line == quote) ||
            (quote == '\0' && (*cmd_line == ' ' || *cmd_line == '\t'))) {
            break;
        }

        if (written + 1 < out_size) {
            out[written++] = *cmd_line;
        }
        ++cmd_line;
    }

    out[written] = '\0';
}

static void rt_format_window_title(char *out, size_t out_size)
{
    const char *mode = g_rt.mode == RT_MODE_INSTALL ? "Setup" : "Uninstall";

    if (g_rt.info.app_name[0] != '\0') {
        snprintf(out, out_size, "%s %s", g_rt.info.app_name, mode);
    } else {
        snprintf(out, out_size, "%s", mode);
    }
    out[out_size - 1] = '\0';
}

static HICON rt_load_window_icon(HINSTANCE instance)
{
    HICON icon = LoadIconA(instance, MAKEINTRESOURCEA(1));

    return icon != NULL ? icon : LoadIcon(NULL, IDI_APPLICATION);
}

static LRESULT CALLBACK rt_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        g_rt.window = hwnd;
        InitializeCriticalSection(&g_rt.progress_lock);
        g_rt.progress_lock_ready = 1;
        g_rt.font_body = rt_font(8, FW_NORMAL);
        g_rt.font_bold = rt_font(8, FW_BOLD);
        g_rt.font_title = rt_font(18, FW_NORMAL);
        g_rt.brush_bg = CreateSolidBrush(rt_theme_color(g_rt.info.theme.background, RGB(240, 240, 240)));
        g_rt.brush_white = CreateSolidBrush(rt_theme_color(g_rt.info.theme.panel, RGB(255, 255, 255)));
        rt_create_controls(hwnd);
        if (g_rt.has_license) {
            rt_load_text_file(g_rt.license_path, g_rt.license);
        }
        rt_set_page();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case RT_ID_BACK:
            rt_back();
            return 0;
        case RT_ID_NEXT:
            rt_next();
            return 0;
        case RT_ID_CANCEL:
            if (g_rt.operation_started && !g_rt.operation_done) {
                return 0;
            }
            DestroyWindow(hwnd);
            return 0;
        case RT_ID_BROWSE:
            rt_pick_folder();
            return 0;
        case RT_ID_ACCEPT:
            g_rt.accepted_license = SendMessageA(g_rt.accept, BM_GETCHECK, 0, 0) == BST_CHECKED;
            EnableWindow(g_rt.next, g_rt.accepted_license);
            return 0;
        case RT_ID_PROGRESS_MORE:
            rt_toggle_progress_details();
            return 0;
        default:
            break;
        }
        break;

    case WM_TIMER:
        if (wparam == RT_TIMER_PROGRESS) {
            rt_progress_tick();
            return 0;
        }
        break;

    case RT_WM_OPERATION_DONE:
        rt_operation_done();
        return 0;

    case RT_WM_PROGRESS_UPDATE:
        rt_apply_progress_update();
        if (rt_modern_style_enabled()) {
            rt_modern_layout(hwnd);
        }
        if (rt_legacy_style_enabled()) {
            rt_legacy_layout(hwnd);
        }
        return 0;

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wparam;
        HWND control = (HWND)lparam;
        LRESULT legacy_result;

        if (rt_modern_ctlcolor_static(dc, control, &legacy_result)) {
            return legacy_result;
        }

        if (rt_legacy_ctlcolor_static(dc, control, &legacy_result)) {
            return legacy_result;
        }

        SetTextColor(dc, rt_theme_color(g_rt.info.theme.text, RGB(0, 0, 0)));
        if (control == g_rt.title || control == g_rt.subtitle) {
            SetBkColor(dc, RGB(255, 255, 255));
            return (LRESULT)g_rt.brush_white;
        }
        SetBkMode(dc, TRANSPARENT);
        if (g_rt.background_image != NULL) {
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }
        return (LRESULT)g_rt.brush_bg;
    }

    case WM_ERASEBKGND: {
        RECT rect;
        HDC dc = (HDC)wparam;
        if (rt_modern_style_enabled() || rt_legacy_style_enabled()) {
            return 1;
        }
        GetClientRect(hwnd, &rect);
        FillRect(dc, &rect, g_rt.brush_bg);
        return 1;
    }

    case WM_SIZE:
        if (rt_modern_style_enabled()) {
            rt_modern_layout(hwnd);
        }
        if (rt_legacy_style_enabled()) {
            rt_legacy_layout(hwnd);
        }
        return 0;

    case WM_PAINT:
        rt_paint(hwnd);
        return 0;

    case WM_DESTROY:
        if (g_rt.side_image != NULL) {
            DeleteObject(g_rt.side_image);
        }
        if (g_rt.background_image != NULL) {
            DeleteObject(g_rt.background_image);
        }
        if (g_rt.license_path[0] != '\0' && g_rt.embedded_package) {
            DeleteFileA(g_rt.license_path);
        }
        if (g_rt.image_path[0] != '\0' && g_rt.embedded_package) {
            DeleteFileA(g_rt.image_path);
        }
        if (g_rt.background_path[0] != '\0' && g_rt.embedded_package) {
            DeleteFileA(g_rt.background_path);
        }
        DeleteObject(g_rt.font_body);
        DeleteObject(g_rt.font_bold);
        DeleteObject(g_rt.font_title);
        DeleteObject(g_rt.brush_bg);
        DeleteObject(g_rt.brush_white);
        if (g_rt.progress_lock_ready) {
            DeleteCriticalSection(&g_rt.progress_lock);
            g_rt.progress_lock_ready = 0;
        }
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR cmd_line, int show)
{
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;
    char load_message[OS_MAX_MESSAGE_LEN];
    char window_title[OS_MAX_NAME_LEN + 32];
    INITCOMMONCONTROLSEX icc;
    DWORD window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    DWORD window_ex_style = WS_EX_DLGMODALFRAME;
    RECT window_rect = {0, 0, RT_W, RT_H};
    int show_cmd = show;

    (void)previous;

    memset(&g_rt, 0, sizeof(g_rt));
    g_rt.instance = instance;
    os_win32_i18n_init(instance);
    rt_parse_first_arg(cmd_line, g_rt.command_install_dir, sizeof(g_rt.command_install_dir));
    GetModuleFileNameA(NULL, g_rt.self_path, sizeof(g_rt.self_path));
    if (rt_dirname(g_rt.self_path, g_rt.package_dir, sizeof(g_rt.package_dir)) != 0) {
        MessageBoxA(NULL, "Setup cannot find its package folder.", "Setup", MB_OK | MB_ICONERROR);
        return 1;
    }

    g_rt.mode = rt_ieq(rt_basename(g_rt.self_path), "uninstaller.exe") ? RT_MODE_UNINSTALL : RT_MODE_INSTALL;
    if (os_read_embedded_package_info(g_rt.self_path, &g_rt.info, load_message, sizeof(load_message)) == 0) {
        g_rt.embedded_package = 1;
    } else if (os_read_package_info(g_rt.package_dir, &g_rt.info, load_message, sizeof(load_message)) != 0) {
        MessageBoxA(NULL, load_message, "Setup", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (g_rt.mode == RT_MODE_INSTALL && g_rt.command_install_dir[0] != '\0') {
        rt_copy(g_rt.info.install_dir, sizeof(g_rt.info.install_dir), g_rt.command_install_dir);
    }
    rt_format_window_title(window_title, sizeof(window_title));
    if (rt_modern_style_enabled()) {
        window_rect.right = 1000;
        window_rect.bottom = 720;
    }
    if (rt_modern_style_enabled() || g_rt.info.window_style == OS_WINDOW_STYLE_RESIZABLE ||
        g_rt.info.window_style == OS_WINDOW_STYLE_MAXIMIZED) {
        window_style = WS_OVERLAPPEDWINDOW;
        window_ex_style = 0;
    }
    if (g_rt.info.window_style == OS_WINDOW_STYLE_MAXIMIZED) {
        show_cmd = SW_SHOWMAXIMIZED;
    }
    if (rt_legacy_style_enabled()) {
        window_style = WS_OVERLAPPEDWINDOW;
        window_ex_style = 0;
        rt_legacy_apply_window_show(&show_cmd);
    }

    if (g_rt.embedded_package) {
        if (g_rt.info.has_license &&
            rt_temp_file(g_rt.license_path, sizeof(g_rt.license_path), "osl") == 0 &&
            os_extract_embedded_license(g_rt.self_path, g_rt.license_path, load_message, sizeof(load_message)) == 0) {
            g_rt.has_license = g_rt.mode == RT_MODE_INSTALL;
        }

        if (g_rt.info.has_wizard_image &&
            rt_temp_file(g_rt.image_path, sizeof(g_rt.image_path), "osi") == 0 &&
            os_extract_embedded_wizard_image(g_rt.self_path, g_rt.image_path, load_message, sizeof(load_message)) == 0) {
            g_rt.side_image = (HBITMAP)LoadImageA(NULL,
                                                 g_rt.image_path,
                                                 IMAGE_BITMAP,
                                                 0,
                                                 0,
                                                 LR_LOADFROMFILE);
        }

        if (g_rt.info.has_background_image &&
            rt_temp_file(g_rt.background_path, sizeof(g_rt.background_path), "osb") == 0 &&
            os_extract_embedded_background_image(g_rt.self_path,
                                                 g_rt.background_path,
                                                 load_message,
                                                 sizeof(load_message)) == 0) {
            g_rt.background_image = (HBITMAP)LoadImageA(NULL,
                                                       g_rt.background_path,
                                                       IMAGE_BITMAP,
                                                       0,
                                                       0,
                                                       LR_LOADFROMFILE);
        }
    } else if (rt_join(g_rt.license_path, sizeof(g_rt.license_path), g_rt.package_dir, "LICENSE.txt") == 0 &&
               GetFileAttributesA(g_rt.license_path) != INVALID_FILE_ATTRIBUTES) {
        g_rt.has_license = g_rt.mode == RT_MODE_INSTALL;
    }
    rt_build_page_flow();

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = rt_proc;
    wc.hInstance = instance;
    wc.lpszClassName = "OpenstallerRuntimeWizard";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = rt_load_window_icon(instance);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    if (!RegisterClassA(&wc)) {
        return 1;
    }

    AdjustWindowRectEx(&window_rect, window_style, FALSE, window_ex_style);

    hwnd = CreateWindowExA(window_ex_style,
                           wc.lpszClassName,
                           window_title,
                           window_style,
                           CW_USEDEFAULT,
                           CW_USEDEFAULT,
                           window_rect.right - window_rect.left,
                           window_rect.bottom - window_rect.top,
                           NULL,
                           NULL,
                           instance,
                           NULL);
    if (hwnd == NULL) {
        return 1;
    }

    ShowWindow(hwnd, show_cmd);
    UpdateWindow(hwnd);

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return g_rt.operation_ok || !g_rt.operation_done ? 0 : 1;
}
