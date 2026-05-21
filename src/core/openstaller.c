#include "openstaller/openstaller.h"
#include "openstaller_icon.h"
#include "openstaller_manifest.h"
#include "openstaller_pack.h"
#include "openstaller_scripts.h"
#include "openstaller_version.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <windows.h>
#define OS_PATH_SEP '\\'
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define OS_PATH_SEP '/'
#endif

#define OS_FNV_OFFSET 14695981039346656037ull
#define OS_COPY_BUFFER_SIZE 65536u

typedef struct OsCopyContext {
    const OsProjectConfig *config;
    FILE *file_list;
    uint64_t payload_hash;
    size_t file_count;
    char error[OS_MAX_MESSAGE_LEN];
} OsCopyContext;

static int os_is_sep(char ch)
{
    return ch == '/' || ch == '\\';
}

static void os_set_error(char *buffer, size_t size, const char *format, ...)
{
    va_list args;

    if (size == 0) {
        return;
    }

    va_start(args, format);
    vsnprintf(buffer, size, format, args);
    va_end(args);
    buffer[size - 1] = '\0';
}

static int os_copy_string(char *dst, size_t dst_size, const char *src)
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

static int os_append_string(char *dst, size_t dst_size, const char *src)
{
    size_t dst_len;
    size_t src_len;

    dst_len = strlen(dst);
    src_len = strlen(src);
    if (dst_len + src_len >= dst_size) {
        return -1;
    }

    memcpy(dst + dst_len, src, src_len + 1);
    return 0;
}

static int os_path_join(char *out, size_t out_size, const char *left, const char *right)
{
    size_t left_len;

    if (os_copy_string(out, out_size, left) != 0) {
        return -1;
    }

    left_len = strlen(out);
    if (left_len > 0 && !os_is_sep(out[left_len - 1])) {
        char sep[2];
        sep[0] = OS_PATH_SEP;
        sep[1] = '\0';
        if (os_append_string(out, out_size, sep) != 0) {
            return -1;
        }
    }

    return os_append_string(out, out_size, right);
}

#if defined(_WIN32)
static int os_find_downloads_icon(char *out, size_t out_size, int uninstall_icon)
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

    if (os_path_join(downloads, sizeof(downloads), downloads, "Downloads") != 0 ||
        os_path_join(pattern, sizeof(pattern), downloads, suffix) != 0) {
        return -1;
    }

    handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        return -1;
    }
    FindClose(handle);

    return os_path_join(out, out_size, downloads, data.cFileName);
}
#endif

static void os_normalize_manifest_path(char *path)
{
    char *cursor;

    for (cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
}

static void os_make_safe_name(const char *name, char *out, size_t out_size)
{
    size_t written = 0;
    int last_dash = 0;

    if (out_size == 0) {
        return;
    }

    while (*name != '\0' && written + 1 < out_size) {
        unsigned char ch = (unsigned char)*name;

        if (isalnum(ch)) {
            out[written++] = (char)tolower(ch);
            last_dash = 0;
        } else if (!last_dash && written > 0) {
            out[written++] = '-';
            last_dash = 1;
        }

        ++name;
    }

    while (written > 0 && out[written - 1] == '-') {
        --written;
    }

    if (written == 0 && out_size > 1) {
        memcpy(out, "app", 4);
        return;
    }

    out[written] = '\0';
}

static int os_mkdir_one(const char *path)
{
#if defined(_WIN32)
    if (_mkdir(path) == 0 || errno == EEXIST) {
        return 0;
    }
#else
    if (mkdir(path, 0755) == 0 || errno == EEXIST) {
        return 0;
    }
#endif
    return -1;
}

static int os_mkdirs(const char *path)
{
    char tmp[OS_MAX_PATH_LEN];
    char *cursor;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    if (os_copy_string(tmp, sizeof(tmp), path) != 0) {
        return -1;
    }

    for (cursor = tmp; *cursor != '\0'; ++cursor) {
        if (!os_is_sep(*cursor)) {
            continue;
        }

        if (cursor == tmp) {
            continue;
        }

#if defined(_WIN32)
        if (cursor == tmp + 2 && tmp[1] == ':') {
            continue;
        }
#endif

        *cursor = '\0';
        if (tmp[0] != '\0' && os_mkdir_one(tmp) != 0) {
            *cursor = OS_PATH_SEP;
            return -1;
        }
        *cursor = OS_PATH_SEP;
    }

    return os_mkdir_one(tmp);
}

static int os_copy_file_with_hash(const char *src,
                                  const char *dst,
                                  uint64_t *file_hash,
                                  uint64_t *payload_hash,
                                  uint64_t *size_out)
{
    FILE *in;
    FILE *out;
    unsigned char buffer[OS_COPY_BUFFER_SIZE];
    size_t read_count;
    uint64_t local_hash = OS_FNV_OFFSET;
    uint64_t total = 0;

    in = fopen(src, "rb");
    if (in == NULL) {
        return -1;
    }

    out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        return -1;
    }

    while ((read_count = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, read_count, out) != read_count) {
            fclose(out);
            fclose(in);
            return -1;
        }

        local_hash = os_hash_bytes(buffer, read_count, local_hash);
        *payload_hash = os_hash_bytes(buffer, read_count, *payload_hash);
        total += (uint64_t)read_count;
    }

    if (ferror(in) != 0 || fclose(out) != 0) {
        fclose(in);
        return -1;
    }

    fclose(in);
    *file_hash = local_hash;
    *size_out = total;
    return 0;
}

static int os_copy_plain_file(const char *src, const char *dst)
{
    uint64_t ignored_hash = 0;
    uint64_t ignored_payload = OS_FNV_OFFSET;
    uint64_t ignored_size = 0;

    return os_copy_file_with_hash(src, dst, &ignored_hash, &ignored_payload, &ignored_size);
}

static int os_remove_tree(const char *path, char *error, size_t error_size)
{
#if defined(_WIN32)
    WIN32_FIND_DATAA data;
    HANDLE handle;
    DWORD attrs;
    char pattern[OS_MAX_PATH_LEN];

    attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return 0;
    }

    if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        if (!DeleteFileA(path)) {
            os_set_error(error, error_size, "Cannot delete file: %s", path);
            return -1;
        }
        return 0;
    }

    if (os_path_join(pattern, sizeof(pattern), path, "*") != 0) {
        os_set_error(error, error_size, "Delete path is too long: %s", path);
        return -1;
    }

    handle = FindFirstFileA(pattern, &data);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            char child[OS_MAX_PATH_LEN];

            if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
                continue;
            }

            if (os_path_join(child, sizeof(child), path, data.cFileName) != 0) {
                FindClose(handle);
                os_set_error(error, error_size, "Delete path is too long.");
                return -1;
            }

            if (os_remove_tree(child, error, error_size) != 0) {
                FindClose(handle);
                return -1;
            }
        } while (FindNextFileA(handle, &data) != 0);
        FindClose(handle);
    }

    if (!RemoveDirectoryA(path)) {
        os_set_error(error, error_size, "Cannot remove directory: %s", path);
        return -1;
    }
    return 0;
#else
    DIR *dir;
    struct dirent *entry;
    struct stat st;

    if (lstat(path, &st) != 0) {
        return 0;
    }

    if (!S_ISDIR(st.st_mode)) {
        if (unlink(path) != 0) {
            os_set_error(error, error_size, "Cannot delete file: %s", path);
            return -1;
        }
        return 0;
    }

    dir = opendir(path);
    if (dir == NULL) {
        os_set_error(error, error_size, "Cannot read directory for deletion: %s", path);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char child[OS_MAX_PATH_LEN];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (os_path_join(child, sizeof(child), path, entry->d_name) != 0) {
            closedir(dir);
            os_set_error(error, error_size, "Delete path is too long.");
            return -1;
        }

        if (os_remove_tree(child, error, error_size) != 0) {
            closedir(dir);
            return -1;
        }
    }

    closedir(dir);
    if (rmdir(path) != 0) {
        os_set_error(error, error_size, "Cannot remove directory: %s", path);
        return -1;
    }
    return 0;
#endif
}

static int os_remove_file_if_exists(const char *path, char *error, size_t error_size)
{
#if defined(_WIN32)
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return 0;
    }
    if ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        os_set_error(error, error_size, "Expected a file but found a directory: %s", path);
        return -1;
    }
    if (!DeleteFileA(path)) {
        os_set_error(error, error_size, "Cannot delete staging file: %s", path);
        return -1;
    }
    return 0;
#else
    struct stat st;
    if (lstat(path, &st) != 0) {
        return 0;
    }
    if (S_ISDIR(st.st_mode)) {
        os_set_error(error, error_size, "Expected a file but found a directory: %s", path);
        return -1;
    }
    if (unlink(path) != 0) {
        os_set_error(error, error_size, "Cannot delete staging file: %s", path);
        return -1;
    }
    return 0;
#endif
}

static int os_record_file(OsCopyContext *ctx,
                          const char *src,
                          const char *dst,
                          const char *rel_path)
{
    uint64_t file_hash;
    uint64_t file_size;
    char manifest_path[OS_MAX_PATH_LEN];

    if (os_copy_string(manifest_path, sizeof(manifest_path), rel_path) != 0) {
        os_set_error(ctx->error, sizeof(ctx->error), "Path is too long: %s", rel_path);
        return -1;
    }

    os_normalize_manifest_path(manifest_path);
    ctx->payload_hash = os_hash_bytes(manifest_path, strlen(manifest_path), ctx->payload_hash);

    if (os_copy_file_with_hash(src, dst, &file_hash, &ctx->payload_hash, &file_size) != 0) {
        os_set_error(ctx->error, sizeof(ctx->error), "Cannot copy payload file: %s", src);
        return -1;
    }

    fprintf(ctx->file_list,
            "%016llx %10llu %s\n",
            (unsigned long long)file_hash,
            (unsigned long long)file_size,
            manifest_path);

    ctx->file_count++;
    return 0;
}

static int os_copy_tree(OsCopyContext *ctx,
                        const char *src_dir,
                        const char *dst_dir,
                        const char *rel_dir)
{
#if defined(_WIN32)
    WIN32_FIND_DATAA data;
    HANDLE handle;
    char pattern[OS_MAX_PATH_LEN];

    if (os_path_join(pattern, sizeof(pattern), src_dir, "*") != 0) {
        os_set_error(ctx->error, sizeof(ctx->error), "Path is too long under: %s", src_dir);
        return -1;
    }

    handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        os_set_error(ctx->error, sizeof(ctx->error), "Cannot read source directory: %s", src_dir);
        return -1;
    }

    do {
        char src_child[OS_MAX_PATH_LEN];
        char dst_child[OS_MAX_PATH_LEN];
        char rel_child[OS_MAX_PATH_LEN];

        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
            continue;
        }

        if (os_path_join(src_child, sizeof(src_child), src_dir, data.cFileName) != 0 ||
            os_path_join(dst_child, sizeof(dst_child), dst_dir, data.cFileName) != 0) {
            FindClose(handle);
            os_set_error(ctx->error, sizeof(ctx->error), "Path is too long: %s", data.cFileName);
            return -1;
        }

        if (rel_dir[0] == '\0') {
            if (os_copy_string(rel_child, sizeof(rel_child), data.cFileName) != 0) {
                FindClose(handle);
                os_set_error(ctx->error, sizeof(ctx->error), "Relative path is too long.");
                return -1;
            }
        } else if (os_path_join(rel_child, sizeof(rel_child), rel_dir, data.cFileName) != 0) {
            FindClose(handle);
            os_set_error(ctx->error, sizeof(ctx->error), "Relative path is too long.");
            return -1;
        }

        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (os_mkdirs(dst_child) != 0 || os_copy_tree(ctx, src_child, dst_child, rel_child) != 0) {
                FindClose(handle);
                return -1;
            }
        } else {
            if (os_record_file(ctx, src_child, dst_child, rel_child) != 0) {
                FindClose(handle);
                return -1;
            }
        }
    } while (FindNextFileA(handle, &data) != 0);

    FindClose(handle);
    return 0;
#else
    DIR *dir;
    struct dirent *entry;

    dir = opendir(src_dir);
    if (dir == NULL) {
        os_set_error(ctx->error, sizeof(ctx->error), "Cannot read source directory: %s", src_dir);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char src_child[OS_MAX_PATH_LEN];
        char dst_child[OS_MAX_PATH_LEN];
        char rel_child[OS_MAX_PATH_LEN];
        struct stat st;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (os_path_join(src_child, sizeof(src_child), src_dir, entry->d_name) != 0 ||
            os_path_join(dst_child, sizeof(dst_child), dst_dir, entry->d_name) != 0) {
            closedir(dir);
            os_set_error(ctx->error, sizeof(ctx->error), "Path is too long: %s", entry->d_name);
            return -1;
        }

        if (rel_dir[0] == '\0') {
            if (os_copy_string(rel_child, sizeof(rel_child), entry->d_name) != 0) {
                closedir(dir);
                os_set_error(ctx->error, sizeof(ctx->error), "Relative path is too long.");
                return -1;
            }
        } else if (os_path_join(rel_child, sizeof(rel_child), rel_dir, entry->d_name) != 0) {
            closedir(dir);
            os_set_error(ctx->error, sizeof(ctx->error), "Relative path is too long.");
            return -1;
        }

        if (stat(src_child, &st) != 0) {
            closedir(dir);
            os_set_error(ctx->error, sizeof(ctx->error), "Cannot stat source path: %s", src_child);
            return -1;
        }

        if (S_ISDIR(st.st_mode)) {
            if (os_mkdirs(dst_child) != 0 || os_copy_tree(ctx, src_child, dst_child, rel_child) != 0) {
                closedir(dir);
                return -1;
            }
        } else if (S_ISREG(st.st_mode)) {
            if (os_record_file(ctx, src_child, dst_child, rel_child) != 0) {
                closedir(dir);
                return -1;
            }
        }
    }

    closedir(dir);
    return 0;
#endif
}

static int os_write_native_executables(const OsProjectConfig *config,
                                       const char *package_dir,
                                       char *error,
                                       size_t error_size)
{
    char installer_path[OS_MAX_PATH_LEN];
    char uninstaller_path[OS_MAX_PATH_LEN];
#if defined(_WIN32)
    char installer_icon_path[OS_MAX_PATH_LEN];
    char uninstaller_icon_path[OS_MAX_PATH_LEN];
    const char *installer_icon = config->installer_icon_file;
    const char *uninstaller_icon = config->uninstaller_icon_file;
#endif

    if (config->runtime_exe[0] == '\0') {
        os_set_error(error, error_size, "Runtime EXE path is not configured.");
        return -1;
    }

#if defined(_WIN32)
    if (os_path_join(installer_path, sizeof(installer_path), package_dir, "installer.exe") != 0 ||
        os_path_join(uninstaller_path, sizeof(uninstaller_path), package_dir, "uninstaller.exe") != 0) {
        os_set_error(error, error_size, "Native EXE output path is too long.");
        return -1;
    }
#else
    if (os_path_join(installer_path, sizeof(installer_path), package_dir, "installer") != 0 ||
        os_path_join(uninstaller_path, sizeof(uninstaller_path), package_dir, "uninstaller") != 0) {
        os_set_error(error, error_size, "Native executable output path is too long.");
        return -1;
    }
#endif

    if (os_copy_plain_file(config->runtime_exe, installer_path) != 0 ||
        os_copy_plain_file(config->runtime_exe, uninstaller_path) != 0) {
        os_set_error(error, error_size, "Cannot copy native runtime EXE.");
        return -1;
    }

#if defined(_WIN32)
    if (installer_icon[0] == '\0' &&
        os_find_downloads_icon(installer_icon_path, sizeof(installer_icon_path), 0) == 0) {
        installer_icon = installer_icon_path;
    }
    if (uninstaller_icon[0] == '\0' &&
        os_find_downloads_icon(uninstaller_icon_path, sizeof(uninstaller_icon_path), 1) == 0) {
        uninstaller_icon = uninstaller_icon_path;
    }

    if (os_icon_apply_to_exe(installer_path,
                             installer_icon,
                             0,
                             error,
                             error_size) != 0 ||
        os_icon_apply_to_exe(uninstaller_path,
                             uninstaller_icon,
                             1,
                             error,
                             error_size) != 0) {
        return -1;
    }

    if (os_version_apply_to_exe(installer_path, config, 0, error, error_size) != 0 ||
        os_version_apply_to_exe(uninstaller_path, config, 1, error, error_size) != 0) {
        return -1;
    }
#endif

    if (os_pack_append_archive(config,
                               package_dir,
                               uninstaller_path,
                               0,
                               0,
                               NULL,
                               error,
                               error_size) != 0 ||
        os_pack_append_archive(config,
                               package_dir,
                               installer_path,
                               1,
                               config->include_license && config->license_file[0] != '\0',
                               uninstaller_path,
                               error,
                               error_size) != 0) {
        return -1;
    }

    if (!config->generate_windows && !config->generate_unix &&
        os_remove_file_if_exists(uninstaller_path, error, error_size) != 0) {
        return -1;
    }

#if !defined(_WIN32)
    chmod(installer_path, 0755);
    if (config->generate_windows || config->generate_unix) {
        chmod(uninstaller_path, 0755);
    }
#endif

    return 0;
}

static int os_write_manifest(const OsProjectConfig *config,
                             const char *package_dir,
                             const char *safe_name,
                             uint64_t payload_hash,
                             uint64_t installer_id,
                             size_t file_count)
{
    char manifest_path[OS_MAX_PATH_LEN];
    FILE *file;
    size_t online_count = config->online_component_count;
    size_t i;

    if (os_path_join(manifest_path, sizeof(manifest_path), package_dir, "manifest.openstaller") != 0) {
        return -1;
    }

    file = fopen(manifest_path, "wb");
    if (file == NULL) {
        return -1;
    }

    fputs("openstaller.manifest=1\n", file);
    os_manifest_write_value(file, "name", config->app_name);
    os_manifest_write_value(file, "company", config->company_name);
    os_manifest_write_value(file, "safe_name", safe_name);
    os_manifest_write_value(file, "version", config->app_version);
    os_manifest_write_value(file, "install_dir", config->install_dir);
    os_manifest_write_value(file, "ui_font", config->ui_font);
    os_manifest_write_value(file, "launcher", config->launcher);
    os_manifest_write_value(file,
                            "installer_style",
                            config->installer_style == OS_INSTALLER_STYLE_LEGACY
                                ? "legacy"
                                : config->installer_style == OS_INSTALLER_STYLE_MODERN ? "modern" : "classic");
    os_manifest_write_value(file,
                            "window_style",
                            config->window_style == OS_WINDOW_STYLE_RESIZABLE
                                ? "resizable"
                                : config->window_style == OS_WINDOW_STYLE_MAXIMIZED ? "maximized" : "fixed");
    fprintf(file, "page_flags=%u\n", (unsigned)config->page_flags);
    os_manifest_write_value(file, "theme.accent", config->theme.accent);
    os_manifest_write_value(file, "theme.progress", config->theme.progress);
    os_manifest_write_value(file, "theme.sidebar", config->theme.sidebar);
    os_manifest_write_value(file, "theme.sidebar_dark", config->theme.sidebar_dark);
    os_manifest_write_value(file, "theme.background", config->theme.background);
    os_manifest_write_value(file, "theme.panel", config->theme.panel);
    os_manifest_write_value(file, "theme.text", config->theme.text);
    os_manifest_write_value(file, "theme.muted_text", config->theme.muted_text);
    os_manifest_write_value(file, "theme.legacy_top", config->theme.legacy_top);
    os_manifest_write_value(file, "theme.legacy_bottom", config->theme.legacy_bottom);
    fprintf(file,
            "register_system=%d\n"
            "payload_hash=%016llx\n"
            "installer_id=%016llx\n"
            "file_count=%llu\n"
            "hash_backend=%s\n"
            "used_assembly=%d\n",
            config->register_system,
            (unsigned long long)payload_hash,
            (unsigned long long)installer_id,
            (unsigned long long)file_count,
            os_hash_backend_name(),
            os_hash_uses_assembly());
    if (online_count > OS_MAX_ONLINE_COMPONENTS) {
        online_count = OS_MAX_ONLINE_COMPONENTS;
    }
    fprintf(file, "online_count=%llu\n", (unsigned long long)online_count);
    for (i = 0; i < online_count; ++i) {
        char key[64];

        snprintf(key, sizeof(key), "online.%llu.name", (unsigned long long)i);
        os_manifest_write_value(file, key, config->online_components[i].name);
        snprintf(key, sizeof(key), "online.%llu.description", (unsigned long long)i);
        os_manifest_write_value(file, key, config->online_components[i].description);
        snprintf(key, sizeof(key), "online.%llu.url", (unsigned long long)i);
        os_manifest_write_value(file, key, config->online_components[i].url);
        snprintf(key, sizeof(key), "online.%llu.target", (unsigned long long)i);
        os_manifest_write_value(file, key, config->online_components[i].target_path);
        fprintf(file,
                "online.%llu.default=%d\n",
                (unsigned long long)i,
                config->online_components[i].selected_by_default ? 1 : 0);
    }
    os_manifest_write_value(file, "welcome_title", config->welcome_title);
    os_manifest_write_value(file, "welcome_text", config->welcome_text);
    os_manifest_write_value(file, "folder_title", config->folder_title);
    os_manifest_write_value(file, "folder_text", config->folder_text);
    os_manifest_write_value(file, "components_title", config->components_title);
    os_manifest_write_value(file, "components_text", config->components_text);
    os_manifest_write_value(file, "ready_title", config->ready_title);
    os_manifest_write_value(file, "ready_text", config->ready_text);
    os_manifest_write_value(file, "finish_title", config->finish_title);
    os_manifest_write_value(file, "finish_text", config->finish_text);
    os_manifest_write_value(file, "uninstall_title", config->uninstall_title);
    os_manifest_write_value(file, "uninstall_text", config->uninstall_text);

    fclose(file);
    return 0;
}

int os_generate_project(const OsProjectConfig *config, OsGenerationResult *result)
{
    OsCopyContext ctx;
    char safe_name[OS_MAX_NAME_LEN];
    char package_leaf[OS_MAX_NAME_LEN + OS_MAX_VERSION_LEN + 8];
    char package_dir[OS_MAX_PATH_LEN];
    char payload_dir[OS_MAX_PATH_LEN];
    char file_list_path[OS_MAX_PATH_LEN];
    char manifest_path[OS_MAX_PATH_LEN];
    char license_dst[OS_MAX_PATH_LEN];
    uint64_t installer_id;

    if (result == NULL) {
        return -1;
    }

    memset(result, 0, sizeof(*result));
    license_dst[0] = '\0';

    if (config == NULL) {
        os_set_error(result->message, sizeof(result->message), "No project configuration was provided.");
        return -1;
    }

    if (config->app_name[0] == '\0' ||
        config->app_version[0] == '\0' ||
        config->source_dir[0] == '\0' ||
        config->output_dir[0] == '\0' ||
        config->install_dir[0] == '\0') {
        os_set_error(result->message,
                     sizeof(result->message),
                     "App name, version, source directory, output directory, and install directory are required.");
        return -1;
    }

    os_make_safe_name(config->app_name, safe_name, sizeof(safe_name));
    snprintf(package_leaf, sizeof(package_leaf), "%s-%s", safe_name, config->app_version);

    if (os_path_join(package_dir, sizeof(package_dir), config->output_dir, package_leaf) != 0 ||
        os_path_join(payload_dir, sizeof(payload_dir), package_dir, "payload") != 0 ||
        os_path_join(file_list_path, sizeof(file_list_path), package_dir, "payload.files") != 0 ||
        os_path_join(manifest_path, sizeof(manifest_path), package_dir, "manifest.openstaller") != 0) {
        os_set_error(result->message, sizeof(result->message), "Output path is too long.");
        return -1;
    }

    if (os_mkdirs(config->output_dir) != 0 ||
        os_mkdirs(package_dir) != 0 ||
        os_mkdirs(payload_dir) != 0) {
        os_set_error(result->message, sizeof(result->message), "Cannot create output directory: %s", package_dir);
        return -1;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.config = config;
    ctx.payload_hash = OS_FNV_OFFSET;
    ctx.file_list = fopen(file_list_path, "wb");
    if (ctx.file_list == NULL) {
        os_set_error(result->message, sizeof(result->message), "Cannot write payload file list.");
        return -1;
    }

    fprintf(ctx.file_list, "# hash             size path\n");
    if (os_copy_tree(&ctx, config->source_dir, payload_dir, "") != 0) {
        fclose(ctx.file_list);
        os_copy_string(result->message, sizeof(result->message), ctx.error);
        return -1;
    }
    fclose(ctx.file_list);

    if (ctx.file_count == 0) {
        os_set_error(result->message, sizeof(result->message), "Source directory has no regular files.");
        return -1;
    }

    if (config->include_license && config->license_file[0] != '\0') {
        if (os_path_join(license_dst, sizeof(license_dst), package_dir, "LICENSE.txt") != 0 ||
            os_copy_plain_file(config->license_file, license_dst) != 0) {
            os_set_error(result->message, sizeof(result->message), "Cannot copy license file: %s", config->license_file);
            return -1;
        }
    }

    installer_id = os_hash_bytes(config->app_name, strlen(config->app_name), ctx.payload_hash);
    installer_id = os_hash_bytes(config->company_name, strlen(config->company_name), installer_id);
    installer_id = os_hash_bytes(config->app_version, strlen(config->app_version), installer_id);
    installer_id = os_hash_bytes(config->install_dir, strlen(config->install_dir), installer_id);
    installer_id = os_hash_bytes(&config->page_flags, sizeof(config->page_flags), installer_id);
    installer_id = os_hash_bytes(&config->theme, sizeof(config->theme), installer_id);
    {
        size_t i;
        size_t online_count = config->online_component_count;

        if (online_count > OS_MAX_ONLINE_COMPONENTS) {
            online_count = OS_MAX_ONLINE_COMPONENTS;
        }
        for (i = 0; i < online_count; ++i) {
            const OsOnlineComponent *component = &config->online_components[i];
            unsigned char selected = component->selected_by_default ? 1u : 0u;

            installer_id = os_hash_bytes(component->name, strlen(component->name), installer_id);
            installer_id = os_hash_bytes(component->description, strlen(component->description), installer_id);
            installer_id = os_hash_bytes(component->url, strlen(component->url), installer_id);
            installer_id = os_hash_bytes(component->target_path, strlen(component->target_path), installer_id);
            installer_id = os_hash_bytes(&selected, sizeof(selected), installer_id);
        }
    }

    if (os_write_manifest(config, package_dir, safe_name, ctx.payload_hash, installer_id, ctx.file_count) != 0) {
        os_set_error(result->message, sizeof(result->message), "Cannot write Openstaller manifest.");
        return -1;
    }

    if (config->generate_windows &&
        os_scripts_write_windows(config, package_dir, safe_name, installer_id) != 0) {
        os_set_error(result->message, sizeof(result->message), "Cannot write Windows installer scripts.");
        return -1;
    }

    if (config->generate_unix &&
        os_scripts_write_unix(config, package_dir, safe_name, installer_id) != 0) {
        os_set_error(result->message, sizeof(result->message), "Cannot write Unix installer scripts.");
        return -1;
    }

    if (config->generate_native_exe) {
        char native_error[OS_MAX_MESSAGE_LEN];

        native_error[0] = '\0';
        if (os_write_native_executables(config, package_dir, native_error, sizeof(native_error)) != 0) {
            os_set_error(result->message,
                         sizeof(result->message),
                         "%s",
                         native_error[0] != '\0'
                             ? native_error
                             : "Cannot write native installer/uninstaller executables. Rebuild or run from the CLI/GUI executable.");
            return -1;
        }
    }

    if (config->generate_native_exe && !config->generate_windows && !config->generate_unix) {
        char cleanup_error[OS_MAX_MESSAGE_LEN];

        cleanup_error[0] = '\0';
        if (os_remove_tree(payload_dir, cleanup_error, sizeof(cleanup_error)) != 0 ||
            os_remove_file_if_exists(file_list_path, cleanup_error, sizeof(cleanup_error)) != 0 ||
            os_remove_file_if_exists(manifest_path, cleanup_error, sizeof(cleanup_error)) != 0 ||
            (license_dst[0] != '\0' && os_remove_file_if_exists(license_dst, cleanup_error, sizeof(cleanup_error)) != 0)) {
            os_set_error(result->message, sizeof(result->message), "%s", cleanup_error);
            return -1;
        }
    }

    result->installer_id = installer_id;
    result->payload_hash = ctx.payload_hash;
    result->file_count = ctx.file_count;
    result->used_assembly = os_hash_uses_assembly();
    os_copy_string(result->package_dir, sizeof(result->package_dir), package_dir);
    os_set_error(result->message,
                 sizeof(result->message),
                 "Package created in %s with %llu file(s).",
                 package_dir,
                 (unsigned long long)ctx.file_count);

    return 0;
}
