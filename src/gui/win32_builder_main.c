#include "win32_builder.h"
#include "win32_i18n.h"
#include "openstaller_icon.h"

#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>

#include <string.h>

BuilderState g_bw;

static int bw_dirname(const char *path, char *out, size_t out_size)
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

static int bw_join(char *out, size_t out_size, const char *left, const char *right)
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

static int bw_find_downloads_icon(char *out, size_t out_size, int uninstall_icon)
{
    char downloads[OS_MAX_PATH_LEN];
    char pattern[OS_MAX_PATH_LEN];
    WIN32_FIND_DATAA data;
    HANDLE handle;
    DWORD length;
    const char *suffix = uninstall_icon ? "*14_08_48-Photoroom.ico" : "*14_07_46-Photoroom.ico";

    length = GetEnvironmentVariableA("USERPROFILE", downloads, (DWORD)sizeof(downloads));
    if (length == 0 || length >= sizeof(downloads)) {
        return -1;
    }

    if (bw_join(downloads, sizeof(downloads), downloads, "Downloads") != 0 ||
        bw_join(pattern, sizeof(pattern), downloads, suffix) != 0) {
        return -1;
    }

    handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        return -1;
    }
    FindClose(handle);

    return bw_join(out, out_size, downloads, data.cFileName);
}

static HICON bw_load_icon_file(const char *path, int size)
{
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    return (HICON)LoadImageA(NULL,
                             path,
                             IMAGE_ICON,
                             size,
                             size,
                             LR_LOADFROMFILE | LR_DEFAULTCOLOR);
}

void bw_set_runtime_exe(char *out, size_t out_size)
{
    char self[OS_MAX_PATH_LEN];
    char dir[OS_MAX_PATH_LEN];
    char runtime[OS_MAX_PATH_LEN];

    GetModuleFileNameA(NULL, self, (DWORD)sizeof(self));
    if (bw_dirname(self, dir, sizeof(dir)) == 0 &&
        bw_join(runtime, sizeof(runtime), dir, "openstaller-runtime.exe") == 0 &&
        GetFileAttributesA(runtime) != INVALID_FILE_ATTRIBUTES) {
        strncpy(out, runtime, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }

    strncpy(out, self, out_size - 1);
    out[out_size - 1] = '\0';
}

void bw_init_config(void)
{
    os_config_init(&g_bw.config);
    strcpy(g_bw.config.app_name, "My Application");
    strcpy(g_bw.config.company_name, "");
    strcpy(g_bw.config.install_dir, "%LOCALAPPDATA%\\MyApplication");
    bw_find_downloads_icon(g_bw.config.installer_icon_file, sizeof(g_bw.config.installer_icon_file), 0);
    bw_find_downloads_icon(g_bw.config.uninstaller_icon_file, sizeof(g_bw.config.uninstaller_icon_file), 1);
    if (ExpandEnvironmentStringsA("%USERPROFILE%\\Desktop\\Openstaller Packages",
                                  g_bw.config.output_dir,
                                  (DWORD)sizeof(g_bw.config.output_dir)) == 0) {
        strcpy(g_bw.config.output_dir, "Openstaller Packages");
    }
    bw_set_runtime_exe(g_bw.config.runtime_exe, sizeof(g_bw.config.runtime_exe));
}

static LRESULT CALLBACK bw_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        g_bw.window = hwnd;
        g_bw.font_body = bw_font(8, FW_NORMAL);
        g_bw.font_bold = bw_font(8, FW_BOLD);
        g_bw.font_title = bw_font(18, FW_NORMAL);
        g_bw.brush_bg = CreateSolidBrush(RGB(246, 248, 251));
        g_bw.brush_white = CreateSolidBrush(RGB(255, 255, 255));
        bw_create_controls(hwnd);
        SetWindowTextA(g_bw.app_name, g_bw.config.app_name);
        SetWindowTextA(g_bw.company_name, g_bw.config.company_name);
        SetWindowTextA(g_bw.app_version, g_bw.config.app_version);
        SetWindowTextA(g_bw.install_dir, g_bw.config.install_dir);
        SetWindowTextA(g_bw.output_dir, g_bw.config.output_dir);
        SetWindowTextA(g_bw.wizard_image, g_bw.config.wizard_image_file);
        SetWindowTextA(g_bw.background_image, g_bw.config.background_image_file);
        SetWindowTextA(g_bw.installer_icon, g_bw.config.installer_icon_file);
        SetWindowTextA(g_bw.uninstaller_icon, g_bw.config.uninstaller_icon_file);
        SendMessageA(g_bw.installer_style,
                     CB_SETCURSEL,
                     g_bw.config.installer_style,
                     0);
        SendMessageA(g_bw.ui_font, CB_SETCURSEL, bw_ui_font_selection(), 0);
        SendMessageA(g_bw.window_style,
                     CB_SETCURSEL,
                     g_bw.config.window_style >= OS_WINDOW_STYLE_FIXED &&
                             g_bw.config.window_style <= OS_WINDOW_STYLE_MAXIMIZED
                         ? g_bw.config.window_style
                         : OS_WINDOW_STYLE_FIXED,
                     0);
        SendMessageA(g_bw.native_box, BM_SETCHECK, g_bw.config.generate_native_exe ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageA(g_bw.register_box, BM_SETCHECK, g_bw.config.register_system ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageA(g_bw.windows_box, BM_SETCHECK, g_bw.config.generate_windows ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageA(g_bw.unix_box, BM_SETCHECK, g_bw.config.generate_unix ? BST_CHECKED : BST_UNCHECKED, 0);
        bw_set_page(BW_PAGE_APP);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case BW_ID_BACK:
            bw_back();
            return 0;
        case BW_ID_NEXT:
            bw_next();
            return 0;
        case BW_ID_CANCEL:
            DestroyWindow(hwnd);
            return 0;
        case BW_ID_SOURCE_BROWSE:
            bw_pick_source();
            return 0;
        case BW_ID_OUTPUT_BROWSE:
            bw_pick_output();
            return 0;
        case BW_ID_LICENSE_BROWSE:
            bw_pick_license();
            return 0;
        case BW_ID_IMAGE_BROWSE:
            bw_pick_wizard_image();
            return 0;
        case BW_ID_BACKGROUND_IMAGE_BROWSE:
            bw_pick_background_image();
            return 0;
        case BW_ID_INSTALLER_ICON_BROWSE:
            bw_pick_installer_icon();
            return 0;
        case BW_ID_UNINSTALLER_ICON_BROWSE:
            bw_pick_uninstaller_icon();
            return 0;
        case BW_ID_OPEN_OUTPUT:
            if (g_bw.generated && g_bw.result.package_dir[0] != '\0') {
                ShellExecuteA(hwnd, "open", g_bw.result.package_dir, NULL, NULL, SW_SHOWNORMAL);
            }
            return 0;
        case BW_ID_TEXT_PAGE:
            if (HIWORD(wparam) == CBN_SELCHANGE) {
                bw_save_text_editor();
                g_bw.selected_text_page = (int)SendMessageA(g_bw.text_page, CB_GETCURSEL, 0, 0);
                bw_load_text_editor();
            }
            return 0;
        case BW_ID_INSTALLER_STYLE:
        case BW_ID_UI_FONT:
        case BW_ID_WINDOW_STYLE:
            if (HIWORD(wparam) == CBN_SELCHANGE) {
                bw_save_visible_values();
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        case BW_ID_PAGE_FLOW:
        case BW_ID_THEME_COLORS:
            if (HIWORD(wparam) == EN_CHANGE && g_bw.page == BW_PAGE_OPTIONS) {
                bw_save_visible_values();
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        default:
            break;
        }
        break;

    case WM_LBUTTONDOWN: {
        int x = (short)LOWORD(lparam);
        int y = (short)HIWORD(lparam);
        int step = bw_step_from_point(x, y);

        if (g_bw.page == BW_PAGE_OPTIONS && y >= 144 && y <= 182) {
            int style = -1;
            if (x >= 626 && x <= 674) {
                style = OS_INSTALLER_STYLE_CLASSIC;
            } else if (x >= 684 && x <= 732) {
                style = OS_INSTALLER_STYLE_MODERN;
            } else if (x >= 742 && x <= 790) {
                style = OS_INSTALLER_STYLE_LEGACY;
            }
            if (style >= 0) {
                SendMessageA(g_bw.installer_style, CB_SETCURSEL, style, 0);
                bw_save_visible_values();
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
        }

        if (step >= 0) {
            bw_go_to_page((BwPage)step);
            return 0;
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wparam;
        HWND control = (HWND)lparam;
        SetTextColor(dc, RGB(0, 0, 0));
        SetBkMode(dc, TRANSPARENT);
        return (LRESULT)g_bw.brush_bg;
    }

    case WM_ERASEBKGND: {
        RECT rect;
        HDC dc = (HDC)wparam;
        GetClientRect(hwnd, &rect);
        FillRect(dc, &rect, g_bw.brush_bg);
        return 1;
    }

    case WM_PAINT:
        bw_paint(hwnd);
        return 0;

    case WM_DESTROY:
        if (g_bw.icon_big != NULL) {
            DestroyIcon(g_bw.icon_big);
        }
        if (g_bw.icon_small != NULL) {
            DestroyIcon(g_bw.icon_small);
        }
        DeleteObject(g_bw.font_body);
        DeleteObject(g_bw.font_bold);
        DeleteObject(g_bw.font_title);
        DeleteObject(g_bw.brush_bg);
        DeleteObject(g_bw.brush_white);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR cmd_line, int show)
{
    WNDCLASSEXA wc;
    HWND hwnd;
    MSG msg;
    DWORD window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    DWORD window_ex_style = WS_EX_DLGMODALFRAME;
    RECT window_rect = {0, 0, BW_W, BW_H};

    (void)previous;
    (void)cmd_line;

    memset(&g_bw, 0, sizeof(g_bw));
    g_bw.instance = instance;
    os_win32_i18n_init(instance);
    bw_init_config();
    g_bw.icon_big = bw_load_icon_file(g_bw.config.installer_icon_file, 32);
    g_bw.icon_small = bw_load_icon_file(g_bw.config.installer_icon_file, 16);
    if (g_bw.icon_big == NULL) {
        g_bw.icon_big = os_icon_create_default_hicon(32, 0);
    }
    if (g_bw.icon_small == NULL) {
        g_bw.icon_small = os_icon_create_default_hicon(16, 0);
    }

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = bw_proc;
    wc.hInstance = instance;
    wc.lpszClassName = "OpenstallerBuilderWizard";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = g_bw.icon_big != NULL ? g_bw.icon_big : LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = g_bw.icon_small != NULL ? g_bw.icon_small : wc.hIcon;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    if (!RegisterClassExA(&wc)) {
        return 1;
    }

    AdjustWindowRectEx(&window_rect, window_style, FALSE, window_ex_style);

    hwnd = CreateWindowExA(window_ex_style,
                           wc.lpszClassName,
                           "Openstaller Setup Builder",
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

    if (g_bw.icon_big != NULL) {
        SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_bw.icon_big);
    }
    if (g_bw.icon_small != NULL) {
        SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_bw.icon_small);
    }

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return 0;
}
