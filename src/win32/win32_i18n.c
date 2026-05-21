#include "win32_i18n.h"

#include <commctrl.h>

#include <string.h>

#define OS_I18N_TEXT_CAP 80

static char g_win32_text[OS_WIN32_TEXT_COUNT][OS_I18N_TEXT_CAP];
static int g_win32_i18n_ready;

static void os_i18n_copy(OsWin32TextId id, const char *text)
{
    strncpy(g_win32_text[id], text, OS_I18N_TEXT_CAP - 1);
    g_win32_text[id][OS_I18N_TEXT_CAP - 1] = '\0';
}

static void os_i18n_defaults(void)
{
    os_i18n_copy(OS_WIN32_TEXT_BACK, "< Back");
    os_i18n_copy(OS_WIN32_TEXT_NEXT, "Next >");
    os_i18n_copy(OS_WIN32_TEXT_CANCEL, "Cancel");
    os_i18n_copy(OS_WIN32_TEXT_FINISH, "Finish");
    os_i18n_copy(OS_WIN32_TEXT_BROWSE, "Browse...");
    os_i18n_copy(OS_WIN32_TEXT_INSTALL, "Install");
    os_i18n_copy(OS_WIN32_TEXT_PLEASE_WAIT, "Please wait");
    os_i18n_copy(OS_WIN32_TEXT_CLOSE, "Close");
}

static void os_i18n_load_string(const char *module_name, UINT id, OsWin32TextId target, int append_ellipsis)
{
    HMODULE module;
    char buffer[OS_I18N_TEXT_CAP];
    int length;

    module = LoadLibraryExA(module_name, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (module == NULL) {
        return;
    }

    length = LoadStringA(module, id, buffer, (int)sizeof(buffer));
    if (length > 0) {
        buffer[sizeof(buffer) - 1] = '\0';
        os_i18n_copy(target, buffer);
        if (append_ellipsis && strlen(g_win32_text[target]) + 3 < OS_I18N_TEXT_CAP) {
            strcat(g_win32_text[target], "...");
        }
    }

    FreeLibrary(module);
}

void os_win32_i18n_init(HINSTANCE instance)
{
    INITCOMMONCONTROLSEX icc;

    if (g_win32_i18n_ready) {
        return;
    }

    os_i18n_defaults();

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    os_i18n_load_string("user32.dll", 801, OS_WIN32_TEXT_CANCEL, 0);
    os_i18n_load_string("user32.dll", 807, OS_WIN32_TEXT_CLOSE, 0);
    os_i18n_load_string("shell32.dll", 9015, OS_WIN32_TEXT_BROWSE, 1);
    os_i18n_load_string("setupapi.dll", 2002, OS_WIN32_TEXT_INSTALL, 0);
    os_i18n_load_string("appwiz.cpl", 89, OS_WIN32_TEXT_PLEASE_WAIT, 0);
    (void)instance;

    g_win32_i18n_ready = 1;
}

const char *os_win32_text(OsWin32TextId id)
{
    if ((int)id < 0 || id >= OS_WIN32_TEXT_COUNT) {
        return "";
    }

    return g_win32_text[id];
}

void os_win32_set_window_text(HWND hwnd, OsWin32TextId id)
{
    SetWindowTextA(hwnd, os_win32_text(id));
}
