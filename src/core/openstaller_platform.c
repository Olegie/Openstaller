#include "openstaller_platform.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define OS_PLATFORM_SEP '/'

static int platform_copy(char *dst, size_t dst_size, const char *src)
{
    size_t len;

    if (dst_size == 0) {
        return -1;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return 0;
    }

    len = strlen(src);
    if (len >= dst_size) {
        dst[0] = '\0';
        return -1;
    }

    memcpy(dst, src, len + 1);
    return 0;
}

static int platform_append(char *dst, size_t dst_size, const char *src)
{
    size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);

    if (dst_len + src_len >= dst_size) {
        return -1;
    }

    memcpy(dst + dst_len, src, src_len + 1);
    return 0;
}

static int platform_is_sep(char ch)
{
    return ch == '/' || ch == '\\';
}

static int platform_join(char *out, size_t out_size, const char *left, const char *right)
{
    size_t len;

    if (platform_copy(out, out_size, left) != 0) {
        return -1;
    }

    len = strlen(out);
    if (len > 0 && !platform_is_sep(out[len - 1])) {
        char sep[2] = {OS_PLATFORM_SEP, '\0'};
        if (platform_append(out, out_size, sep) != 0) {
            return -1;
        }
    }

    return platform_append(out, out_size, right);
}
#endif

#if defined(_WIN32)
static int platform_expand_windows_dir(const char *expr, char *out, size_t out_size)
{
    DWORD needed = ExpandEnvironmentStringsA(expr, out, (DWORD)out_size);
    return needed > 0 && needed <= out_size ? 0 : -1;
}
#else
static int platform_home_join(char *out, size_t out_size, const char *relative)
{
    const char *home = getenv("HOME");

    if (home == NULL || home[0] == '\0') {
        return -1;
    }

    return platform_join(out, out_size, home, relative);
}
#endif

const char *os_platform_family(void)
{
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unix";
#endif
}

int os_platform_registration_root(char *out, size_t out_size)
{
#if defined(_WIN32)
    return platform_expand_windows_dir("%LOCALAPPDATA%\\Openstaller", out, out_size);
#elif defined(__APPLE__)
    return platform_home_join(out, out_size, "Library/Application Support/Openstaller");
#else
    const char *xdg_data_home = getenv("XDG_DATA_HOME");
    char data_home[OS_MAX_PATH_LEN];

    if (xdg_data_home != NULL && xdg_data_home[0] != '\0') {
        return platform_join(out, out_size, xdg_data_home, "openstaller");
    }

    if (platform_home_join(data_home, sizeof(data_home), ".local/share") != 0) {
        return -1;
    }

    return platform_join(out, out_size, data_home, "openstaller");
#endif
}

int os_platform_uninstaller_root(char *out, size_t out_size)
{
#if defined(_WIN32)
    return platform_expand_windows_dir("%LOCALAPPDATA%\\Openstaller\\Uninstallers", out, out_size);
#else
    return os_platform_registration_root(out, out_size);
#endif
}

int os_platform_applications_dir(char *out, size_t out_size)
{
#if defined(_WIN32)
    return platform_expand_windows_dir("%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs", out, out_size);
#elif defined(__APPLE__)
    return platform_home_join(out, out_size, "Applications");
#else
    const char *xdg_data_home = getenv("XDG_DATA_HOME");
    char data_home[OS_MAX_PATH_LEN];

    if (xdg_data_home != NULL && xdg_data_home[0] != '\0') {
        return platform_join(out, out_size, xdg_data_home, "applications");
    }

    if (platform_home_join(data_home, sizeof(data_home), ".local/share") != 0) {
        return -1;
    }

    return platform_join(out, out_size, data_home, "applications");
#endif
}
