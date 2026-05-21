#include "win32_runtime.h"

#include <shellapi.h>
#include <stdio.h>
#include <string.h>

#if defined(OPENSTALLER_WIN2000_COMPAT)
int rt_install_dir_needs_elevation(const char *install_dir)
{
    (void)install_dir;
    return 0;
}

int rt_is_process_elevated(void)
{
    return 1;
}

int rt_relaunch_elevated_for_install(const char *install_dir)
{
    (void)install_dir;
    return 0;
}
#else
static void rt_normalize_windows_path(char *path)
{
    while (*path != '\0') {
        if (*path == '/') {
            *path = '\\';
        }
        ++path;
    }
}

static int rt_copy_env_path(const char *name, char *out, size_t out_size)
{
    DWORD length = GetEnvironmentVariableA(name, out, (DWORD)out_size);

    if (length == 0 || length >= out_size) {
        return -1;
    }

    rt_normalize_windows_path(out);
    return 0;
}

static int rt_path_has_dir_prefix(const char *path, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    char next;

    while (prefix_len > 0 && (prefix[prefix_len - 1] == '\\' || prefix[prefix_len - 1] == '/')) {
        --prefix_len;
    }

    if (prefix_len == 0 || _strnicmp(path, prefix, prefix_len) != 0) {
        return 0;
    }

    next = path[prefix_len];
    return next == '\0' || next == '\\' || next == '/';
}

int rt_install_dir_needs_elevation(const char *install_dir)
{
    char expanded[OS_MAX_PATH_LEN];
    char protected_dir[OS_MAX_PATH_LEN];
    const char *env_names[] = {
        "ProgramFiles",
        "ProgramW6432",
        "ProgramFiles(x86)",
        "SystemRoot"
    };
    size_t i;
    DWORD needed;

    if (install_dir == NULL || install_dir[0] == '\0') {
        return 0;
    }

    needed = ExpandEnvironmentStringsA(install_dir, expanded, (DWORD)sizeof(expanded));
    if (needed == 0 || needed > sizeof(expanded)) {
        rt_copy(expanded, sizeof(expanded), install_dir);
    }
    rt_normalize_windows_path(expanded);

    for (i = 0; i < sizeof(env_names) / sizeof(env_names[0]); ++i) {
        if (rt_copy_env_path(env_names[i], protected_dir, sizeof(protected_dir)) == 0 &&
            rt_path_has_dir_prefix(expanded, protected_dir)) {
            return 1;
        }
    }

    return 0;
}

int rt_is_process_elevated(void)
{
    HANDLE token;
    TOKEN_ELEVATION elevation;
    DWORD returned;
    int elevated = 0;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return 0;
    }

    if (GetTokenInformation(token,
                            TokenElevation,
                            &elevation,
                            sizeof(elevation),
                            &returned)) {
        elevated = elevation.TokenIsElevated != 0;
    }

    CloseHandle(token);
    return elevated;
}

static void rt_quote_parameter(char *out, size_t out_size, const char *value)
{
    size_t written = 0;

    if (out_size == 0) {
        return;
    }

    out[written++] = '"';
    while (*value != '\0' && written + 2 < out_size) {
        out[written++] = *value == '"' ? '\'' : *value;
        ++value;
    }
    out[written++] = '"';
    out[written] = '\0';
}

int rt_relaunch_elevated_for_install(const char *install_dir)
{
    SHELLEXECUTEINFOA exec_info;
    char params[OS_MAX_PATH_LEN + 8];

    rt_quote_parameter(params, sizeof(params), install_dir);

    memset(&exec_info, 0, sizeof(exec_info));
    exec_info.cbSize = sizeof(exec_info);
    exec_info.fMask = SEE_MASK_NOCLOSEPROCESS;
    exec_info.hwnd = g_rt.window;
    exec_info.lpVerb = "runas";
    exec_info.lpFile = g_rt.self_path;
    exec_info.lpParameters = params;
    exec_info.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExA(&exec_info)) {
        return 0;
    }

    if (exec_info.hProcess != NULL) {
        CloseHandle(exec_info.hProcess);
    }
    return 1;
}
#endif
