#include "openstaller_online.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#define OS_PATH_SEP '\\'
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define OS_PATH_SEP '/'
#endif

#define OS_ONLINE_BUFFER_SIZE 65536u

static int online_is_sep(char ch)
{
    return ch == '/' || ch == '\\';
}

static int online_copy(char *dst, size_t dst_size, const char *src)
{
    size_t len;

    if (dst_size == 0) {
        return -1;
    }

    len = src == NULL ? 0 : strlen(src);
    if (len >= dst_size) {
        dst[0] = '\0';
        return -1;
    }

    if (src == NULL) {
        dst[0] = '\0';
    } else {
        memcpy(dst, src, len + 1);
    }
    return 0;
}

static int online_append(char *dst, size_t dst_size, const char *src)
{
    size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);

    if (dst_len + src_len >= dst_size) {
        return -1;
    }

    memcpy(dst + dst_len, src, src_len + 1);
    return 0;
}

static int online_parent_dir(char *out, size_t out_size, const char *path)
{
    size_t len;

    if (online_copy(out, out_size, path) != 0) {
        return -1;
    }

    len = strlen(out);
    while (len > 0 && !online_is_sep(out[len - 1])) {
        --len;
    }
    if (len == 0) {
        out[0] = '\0';
        return 0;
    }
    while (len > 1 && online_is_sep(out[len - 1])) {
        --len;
    }
    out[len] = '\0';
    return 0;
}

static int online_target_is_safe(const char *path)
{
    const char *segment = path;
    const char *cursor;

    if (path == NULL || path[0] == '\0' || online_is_sep(path[0])) {
        return 0;
    }

#if defined(_WIN32)
    if (path[1] == ':' || (online_is_sep(path[0]) && online_is_sep(path[1]))) {
        return 0;
    }
#endif

    for (cursor = path; ; ++cursor) {
        if (*cursor == '\0' || online_is_sep(*cursor)) {
            size_t len = (size_t)(cursor - segment);

            if (len == 0 ||
                (len == 1 && segment[0] == '.') ||
                (len == 2 && segment[0] == '.' && segment[1] == '.')) {
                return 0;
            }

            if (*cursor == '\0') {
                break;
            }
            segment = cursor + 1;
        }
    }

    return 1;
}

static void online_sanitize_leaf(const char *name, char *out, size_t out_size)
{
    size_t written = 0;

    if (out_size == 0) {
        return;
    }

    while (name != NULL && *name != '\0' && written + 1 < out_size) {
        unsigned char ch = (unsigned char)*name++;

        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '.' ||
            ch == '_' ||
            ch == '-') {
            out[written++] = (char)ch;
        } else if (written > 0 && out[written - 1] != '-') {
            out[written++] = '-';
        }
    }

    while (written > 0 && out[written - 1] == '-') {
        --written;
    }

    if (written == 0) {
        online_copy(out, out_size, "online-component.bin");
        return;
    }

    out[written] = '\0';
}

static void online_derive_target_name(const OsOnlineComponent *component, char *out, size_t out_size)
{
    const char *url = component->url;
    const char *end = url + strlen(url);
    const char *cursor;
    const char *leaf = url;
    char raw[OS_MAX_PATH_LEN];
    size_t len;

    while (end > url && (end[-1] == '?' || end[-1] == '#')) {
        --end;
    }
    for (cursor = url; cursor < end; ++cursor) {
        if (*cursor == '?' || *cursor == '#') {
            end = cursor;
            break;
        }
        if (*cursor == '/') {
            leaf = cursor + 1;
        }
    }

    len = (size_t)(end - leaf);
    if (len == 0 || len >= sizeof(raw)) {
        online_sanitize_leaf(component->name, out, out_size);
        return;
    }

    memcpy(raw, leaf, len);
    raw[len] = '\0';
    online_sanitize_leaf(raw, out, out_size);
}

static int online_target_path(const OsOnlineComponent *component, char *out, size_t out_size)
{
    char relative[OS_MAX_PATH_LEN];

    if (component->target_path[0] != '\0') {
        if (online_copy(relative, sizeof(relative), component->target_path) != 0) {
            return -1;
        }
    } else {
        online_derive_target_name(component, relative, sizeof(relative));
    }

    if (!online_target_is_safe(relative)) {
        return -1;
    }

    return online_copy(out, out_size, relative);
}

static int online_temp_path(char *out, size_t out_size, const char *dst_path)
{
    if (online_copy(out, out_size, dst_path) != 0 ||
        online_append(out, out_size, ".openstaller-download") != 0) {
        return -1;
    }
    return 0;
}

static int online_prepare_destination(const char *dst_path, char *error, size_t error_size)
{
    char parent[OS_MAX_PATH_LEN];

    if (online_parent_dir(parent, sizeof(parent), dst_path) != 0) {
        install_set_error(error, error_size, "Online component path is too long.");
        return -1;
    }

    if (parent[0] != '\0' && install_mkdirs(parent) != 0) {
        install_set_error(error, error_size, "Cannot create online component folder: %s", parent);
        return -1;
    }

    return 0;
}

static int online_normalize_url(const char *url, char *out, size_t out_size)
{
    if (strncmp(url, "https://", 8) != 0 && strncmp(url, "http://", 7) != 0) {
        return -1;
    }

    if (online_copy(out, out_size, url) != 0) {
        return -1;
    }

    if (strstr(out, "dropbox.com") != NULL && strstr(out, "dl=1") == NULL) {
        char *dl = strstr(out, "dl=0");

        if (dl != NULL) {
            dl[3] = '1';
            return 0;
        }

        if (strchr(out, '?') != NULL) {
            return online_append(out, out_size, "&dl=1");
        }
        return online_append(out, out_size, "?dl=1");
    }

    return 0;
}

#if defined(_WIN32)
static int online_delete_file(const char *path)
{
    return DeleteFileA(path) || GetLastError() == ERROR_FILE_NOT_FOUND;
}

static int online_replace_file(const char *tmp_path, const char *dst_path)
{
    if (MoveFileExA(tmp_path, dst_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        return 0;
    }
    DeleteFileA(dst_path);
    return MoveFileA(tmp_path, dst_path) ? 0 : -1;
}

static int online_download_to_file(const char *url, const char *tmp_path, char *error, size_t error_size)
{
    HINTERNET internet;
    HINTERNET request;
    FILE *out;
    char normalized_url[OS_MAX_URL_LEN + 16];
    unsigned char buffer[OS_ONLINE_BUFFER_SIZE];
    DWORD read_count = 0;

    if (online_normalize_url(url, normalized_url, sizeof(normalized_url)) != 0) {
        install_set_error(error, error_size, "Online component URL must be HTTP/HTTPS and fit inside the manifest.");
        return -1;
    }

    internet = InternetOpenA("Openstaller",
                             INTERNET_OPEN_TYPE_PRECONFIG,
                             NULL,
                             NULL,
                             0);
    if (internet == NULL) {
        install_set_error(error, error_size, "Cannot initialize Windows internet services.");
        return -1;
    }

    request = InternetOpenUrlA(internet,
                               normalized_url,
                               NULL,
                               0,
                               INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION,
                               0);
    if (request == NULL) {
        InternetCloseHandle(internet);
        install_set_error(error, error_size, "Cannot download online component: %s", normalized_url);
        return -1;
    }

    out = fopen(tmp_path, "wb");
    if (out == NULL) {
        InternetCloseHandle(request);
        InternetCloseHandle(internet);
        install_set_error(error, error_size, "Cannot create temporary download file: %s", tmp_path);
        return -1;
    }

    for (;;) {
        read_count = 0;
        if (!InternetReadFile(request, buffer, sizeof(buffer), &read_count)) {
            fclose(out);
            InternetCloseHandle(request);
            InternetCloseHandle(internet);
            online_delete_file(tmp_path);
            install_set_error(error, error_size, "The online component download stopped unexpectedly.");
            return -1;
        }
        if (read_count == 0) {
            break;
        }
        if (fwrite(buffer, 1, read_count, out) != read_count) {
            fclose(out);
            InternetCloseHandle(request);
            InternetCloseHandle(internet);
            online_delete_file(tmp_path);
            install_set_error(error, error_size, "Cannot write downloaded component: %s", tmp_path);
            return -1;
        }
    }

    if (fclose(out) != 0) {
        InternetCloseHandle(request);
        InternetCloseHandle(internet);
        online_delete_file(tmp_path);
        install_set_error(error, error_size, "Cannot finalize downloaded component: %s", tmp_path);
        return -1;
    }

    InternetCloseHandle(request);
    InternetCloseHandle(internet);
    return 0;
}
#else
static int online_delete_file(const char *path)
{
    return unlink(path) == 0 || access(path, F_OK) != 0;
}

static int online_replace_file(const char *tmp_path, const char *dst_path)
{
    return rename(tmp_path, dst_path);
}

static int online_download_to_file(const char *url, const char *tmp_path, char *error, size_t error_size)
{
    char normalized_url[OS_MAX_URL_LEN + 16];
    pid_t pid;
    int status;

    if (online_normalize_url(url, normalized_url, sizeof(normalized_url)) != 0) {
        install_set_error(error, error_size, "Online component URL must be HTTP/HTTPS and fit inside the manifest.");
        return -1;
    }

    pid = fork();
    if (pid < 0) {
        install_set_error(error, error_size, "Cannot start curl for online component download.");
        return -1;
    }

    if (pid == 0) {
        execlp("curl", "curl", "-L", "-f", "-sS", "-o", tmp_path, normalized_url, (char *)NULL);
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0 ||
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {
        online_delete_file(tmp_path);
        install_set_error(error,
                          error_size,
                          "Cannot download online component. Install curl or check the URL: %s",
                          normalized_url);
        return -1;
    }

    return 0;
}
#endif

uint64_t os_online_default_mask(const OsPackageManifest *manifest)
{
    uint64_t mask = 0;
    size_t i;
    size_t count;

    if (manifest == NULL) {
        return 0;
    }

    count = manifest->online_component_count;
    if (count > OS_MAX_ONLINE_COMPONENTS) {
        count = OS_MAX_ONLINE_COMPONENTS;
    }

    for (i = 0; i < count; ++i) {
        if (manifest->online_components[i].selected_by_default &&
            manifest->online_components[i].url[0] != '\0') {
            mask |= ((uint64_t)1u << i);
        }
    }

    return mask;
}

size_t os_online_selected_count(const OsPackageManifest *manifest, uint64_t selected_mask)
{
    size_t selected = 0;
    size_t i;
    size_t count;

    if (manifest == NULL) {
        return 0;
    }

    count = manifest->online_component_count;
    if (count > OS_MAX_ONLINE_COMPONENTS) {
        count = OS_MAX_ONLINE_COMPONENTS;
    }

    for (i = 0; i < count; ++i) {
        if ((selected_mask & ((uint64_t)1u << i)) != 0 &&
            manifest->online_components[i].url[0] != '\0') {
            selected++;
        }
    }

    return selected;
}

int os_online_install_components(const OsPackageManifest *manifest,
                                 const char *install_dir,
                                 uint64_t selected_mask,
                                 OsRollback *rollback,
                                 OsInstallProgressCounter *progress,
                                 char *error,
                                 size_t error_size)
{
    size_t i;
    size_t count;

    if (manifest == NULL || install_dir == NULL) {
        install_set_error(error, error_size, "Online install plan is invalid.");
        return -1;
    }

    count = manifest->online_component_count;
    if (count > OS_MAX_ONLINE_COMPONENTS) {
        count = OS_MAX_ONLINE_COMPONENTS;
    }

    for (i = 0; i < count; ++i) {
        const OsOnlineComponent *component = &manifest->online_components[i];
        char relative[OS_MAX_PATH_LEN];
        char dst_path[OS_MAX_PATH_LEN];
        char tmp_path[OS_MAX_PATH_LEN];
        const char *display_name;

        if ((selected_mask & ((uint64_t)1u << i)) == 0 || component->url[0] == '\0') {
            continue;
        }

        display_name = component->name[0] != '\0' ? component->name : "Online component";
        if (online_target_path(component, relative, sizeof(relative)) != 0 ||
            install_join(dst_path, sizeof(dst_path), install_dir, relative) != 0 ||
            online_temp_path(tmp_path, sizeof(tmp_path), dst_path) != 0) {
            install_set_error(error, error_size, "Online component target is unsafe or too long: %s", display_name);
            return -1;
        }

        if (online_prepare_destination(dst_path, error, error_size) != 0) {
            return -1;
        }

        install_report_progress(OS_INSTALL_PROGRESS_FILE,
                                "Downloading online component",
                                component->url,
                                dst_path,
                                progress != NULL ? progress->completed_files : 0,
                                progress != NULL ? progress->total_files : 0,
                                progress != NULL
                                    ? install_progress_percent(progress->completed_files, progress->total_files, 12, 82)
                                    : 50);

        online_delete_file(tmp_path);
        if (online_download_to_file(component->url, tmp_path, error, error_size) != 0) {
            return -1;
        }

        if (os_rollback_capture_file(rollback, dst_path, error, error_size) != 0 ||
            online_replace_file(tmp_path, dst_path) != 0) {
            online_delete_file(tmp_path);
            if (error != NULL && error_size > 0 && error[0] == '\0') {
                install_set_error(error, error_size, "Cannot install downloaded component: %s", display_name);
            }
            return -1;
        }

        if (progress != NULL) {
            progress->completed_files++;
            install_report_progress(OS_INSTALL_PROGRESS_FILE,
                                    "Installed online component",
                                    component->url,
                                    dst_path,
                                    progress->completed_files,
                                    progress->total_files,
                                    install_progress_percent(progress->completed_files, progress->total_files, 12, 82));
        }
    }

    return 0;
}
