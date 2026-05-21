#ifndef OPENSTALLER_WIN32_I18N_H
#define OPENSTALLER_WIN32_I18N_H

#if !defined(_WIN32)
#error "win32_i18n is Windows-only."
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef enum OsWin32TextId {
    OS_WIN32_TEXT_BACK = 0,
    OS_WIN32_TEXT_NEXT,
    OS_WIN32_TEXT_CANCEL,
    OS_WIN32_TEXT_FINISH,
    OS_WIN32_TEXT_BROWSE,
    OS_WIN32_TEXT_INSTALL,
    OS_WIN32_TEXT_PLEASE_WAIT,
    OS_WIN32_TEXT_CLOSE,
    OS_WIN32_TEXT_COUNT
} OsWin32TextId;

void os_win32_i18n_init(HINSTANCE instance);
const char *os_win32_text(OsWin32TextId id);
void os_win32_set_window_text(HWND hwnd, OsWin32TextId id);

#endif
