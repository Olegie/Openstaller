#ifndef OPENSTALLER_ICON_H
#define OPENSTALLER_ICON_H

#include <stddef.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

int os_icon_apply_to_exe(const char *exe_path,
                         const char *ico_path,
                         int uninstall_icon,
                         char *error,
                         size_t error_size);

#if defined(_WIN32)
HICON os_icon_create_default_hicon(int size, int uninstall_icon);
#endif

#endif
