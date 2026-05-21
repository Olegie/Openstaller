#include "openstaller_install_internal.h"

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

static int install_is_sep(char ch)
{
    return ch == '/' || ch == '\\';
}

void install_set_error(char *buffer, size_t size, const char *format, ...)
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

int install_copy(char *dst, size_t dst_size, const char *src)
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

int install_append(char *dst, size_t dst_size, const char *src)
{
    size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);

    if (dst_len + src_len >= dst_size) {
        return -1;
    }

    memcpy(dst + dst_len, src, src_len + 1);
    return 0;
}

int install_join(char *out, size_t out_size, const char *left, const char *right)
{
    size_t len;

    if (install_copy(out, out_size, left) != 0) {
        return -1;
    }

    len = strlen(out);
    if (len > 0 && !install_is_sep(out[len - 1])) {
        char sep[2] = {OS_PATH_SEP, '\0'};
        if (install_append(out, out_size, sep) != 0) {
            return -1;
        }
    }

    return install_append(out, out_size, right);
}

static int install_mkdir_one(const char *path)
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

int install_mkdirs(const char *path)
{
    char tmp[OS_MAX_PATH_LEN];
    char *cursor;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    if (install_copy(tmp, sizeof(tmp), path) != 0) {
        return -1;
    }

    for (cursor = tmp; *cursor != '\0'; ++cursor) {
        if (!install_is_sep(*cursor)) {
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
        if (tmp[0] != '\0' && install_mkdir_one(tmp) != 0) {
            *cursor = OS_PATH_SEP;
            return -1;
        }
        *cursor = OS_PATH_SEP;
    }

    return install_mkdir_one(tmp);
}

int install_path_exists(const char *path)
{
#if defined(_WIN32)
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return lstat(path, &st) == 0;
#endif
}

int install_copy_plain_file(const char *src, const char *dst)
{
    FILE *in;
    FILE *out;
    unsigned char buffer[65536];
    size_t read_count;

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
    }

    if (ferror(in) != 0 || fclose(out) != 0) {
        fclose(in);
        return -1;
    }

    fclose(in);
    return 0;
}

#if !defined(_WIN32)
int install_file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}
#endif

int install_expand_dir(const char *input, char *out, size_t out_size)
{
#if defined(_WIN32)
    DWORD needed = ExpandEnvironmentStringsA(input, out, (DWORD)out_size);
    if (needed > 0 && needed <= out_size) {
        return 0;
    }
#else
    const char *home = getenv("HOME");

    if (home != NULL && input[0] == '~' && install_is_sep(input[1])) {
        if (install_copy(out, out_size, home) != 0 ||
            install_append(out, out_size, input + 1) != 0) {
            return -1;
        }
        return 0;
    }

    if (home != NULL && strncmp(input, "$HOME", 5) == 0 && install_is_sep(input[5])) {
        if (install_copy(out, out_size, home) != 0 ||
            install_append(out, out_size, input + 5) != 0) {
            return -1;
        }
        return 0;
    }
#endif

    return install_copy(out, out_size, input);
}

int install_delete_target_is_dangerous(const char *path)
{
    size_t len;

    if (path == NULL || path[0] == '\0') {
        return 1;
    }

    len = strlen(path);
    while (len > 1 && install_is_sep(path[len - 1])) {
        --len;
    }

    if (len == 1 && install_is_sep(path[0])) {
        return 1;
    }

#if defined(_WIN32)
    if (len == 2 && path[1] == ':') {
        return 1;
    }
    if (len == 3 && path[1] == ':' && install_is_sep(path[2])) {
        return 1;
    }
#endif

    return 0;
}

int install_copy_tree_with_rollback(const char *src_dir,
                                    const char *dst_dir,
                                    OsRollback *rollback,
                                    OsInstallProgressCounter *progress,
                                    char *error,
                                    size_t error_size)
{
#if defined(_WIN32)
    WIN32_FIND_DATAA data;
    HANDLE handle;
    char pattern[OS_MAX_PATH_LEN];

    if (install_join(pattern, sizeof(pattern), src_dir, "*") != 0) {
        install_set_error(error, error_size, "Path is too long under: %s", src_dir);
        return -1;
    }

    handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        install_set_error(error, error_size, "Cannot read payload directory: %s", src_dir);
        return -1;
    }

    if (install_mkdirs(dst_dir) != 0) {
        FindClose(handle);
        install_set_error(error, error_size, "Cannot create install directory: %s", dst_dir);
        return -1;
    }

    do {
        char src_child[OS_MAX_PATH_LEN];
        char dst_child[OS_MAX_PATH_LEN];

        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
            continue;
        }

        if (install_join(src_child, sizeof(src_child), src_dir, data.cFileName) != 0 ||
            install_join(dst_child, sizeof(dst_child), dst_dir, data.cFileName) != 0) {
            FindClose(handle);
            install_set_error(error, error_size, "Install path is too long.");
            return -1;
        }

        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (install_copy_tree_with_rollback(src_child, dst_child, rollback, progress, error, error_size) != 0) {
                FindClose(handle);
                return -1;
            }
        } else {
            if (progress != NULL) {
                install_report_progress(OS_INSTALL_PROGRESS_FILE,
                                        "Installing file",
                                        src_child,
                                        dst_child,
                                        progress->completed_files,
                                        progress->total_files,
                                        install_progress_percent(progress->completed_files, progress->total_files, 12, 82));
            }
            if (os_rollback_capture_file(rollback, dst_child, error, error_size) != 0 ||
                install_copy_plain_file(src_child, dst_child) != 0) {
                FindClose(handle);
                if (error != NULL && error_size > 0 && error[0] == '\0') {
                    install_set_error(error, error_size, "Cannot install payload file: %s", src_child);
                }
                return -1;
            }
            if (progress != NULL) {
                progress->completed_files++;
                install_report_progress(OS_INSTALL_PROGRESS_FILE,
                                        "Installed file",
                                        src_child,
                                        dst_child,
                                        progress->completed_files,
                                        progress->total_files,
                                        install_progress_percent(progress->completed_files, progress->total_files, 12, 82));
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
        install_set_error(error, error_size, "Cannot read payload directory: %s", src_dir);
        return -1;
    }

    if (install_mkdirs(dst_dir) != 0) {
        closedir(dir);
        install_set_error(error, error_size, "Cannot create install directory: %s", dst_dir);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char src_child[OS_MAX_PATH_LEN];
        char dst_child[OS_MAX_PATH_LEN];
        struct stat st;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (install_join(src_child, sizeof(src_child), src_dir, entry->d_name) != 0 ||
            install_join(dst_child, sizeof(dst_child), dst_dir, entry->d_name) != 0) {
            closedir(dir);
            install_set_error(error, error_size, "Install path is too long.");
            return -1;
        }

        if (stat(src_child, &st) != 0) {
            closedir(dir);
            install_set_error(error, error_size, "Cannot stat payload path: %s", src_child);
            return -1;
        }

        if (S_ISDIR(st.st_mode)) {
            if (install_copy_tree_with_rollback(src_child, dst_child, rollback, progress, error, error_size) != 0) {
                closedir(dir);
                return -1;
            }
        } else if (S_ISREG(st.st_mode)) {
            if (progress != NULL) {
                install_report_progress(OS_INSTALL_PROGRESS_FILE,
                                        "Installing file",
                                        src_child,
                                        dst_child,
                                        progress->completed_files,
                                        progress->total_files,
                                        install_progress_percent(progress->completed_files, progress->total_files, 12, 82));
            }
            if (os_rollback_capture_file(rollback, dst_child, error, error_size) != 0 ||
                install_copy_plain_file(src_child, dst_child) != 0) {
                closedir(dir);
                if (error != NULL && error_size > 0 && error[0] == '\0') {
                    install_set_error(error, error_size, "Cannot install payload file: %s", src_child);
                }
                return -1;
            }
            if (progress != NULL) {
                progress->completed_files++;
                install_report_progress(OS_INSTALL_PROGRESS_FILE,
                                        "Installed file",
                                        src_child,
                                        dst_child,
                                        progress->completed_files,
                                        progress->total_files,
                                        install_progress_percent(progress->completed_files, progress->total_files, 12, 82));
            }
        }
    }

    closedir(dir);
    return 0;
#endif
}

int install_copy_tree_plain(const char *src_dir, const char *dst_dir, char *error, size_t error_size)
{
    return install_copy_tree_with_rollback(src_dir, dst_dir, NULL, NULL, error, error_size);
}

int install_remove_tree(const char *path, char *error, size_t error_size)
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
            if (MoveFileExA(path, NULL, MOVEFILE_DELAY_UNTIL_REBOOT)) {
                return 0;
            }
            install_set_error(error, error_size, "Cannot delete file: %s", path);
            return -1;
        }
        return 0;
    }

    if (install_join(pattern, sizeof(pattern), path, "*") != 0) {
        install_set_error(error, error_size, "Delete path is too long: %s", path);
        return -1;
    }

    handle = FindFirstFileA(pattern, &data);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            char child[OS_MAX_PATH_LEN];

            if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
                continue;
            }

            if (install_join(child, sizeof(child), path, data.cFileName) != 0) {
                FindClose(handle);
                install_set_error(error, error_size, "Delete path is too long.");
                return -1;
            }

            if (install_remove_tree(child, error, error_size) != 0) {
                FindClose(handle);
                return -1;
            }
        } while (FindNextFileA(handle, &data) != 0);
        FindClose(handle);
    }

    if (!RemoveDirectoryA(path)) {
        if (MoveFileExA(path, NULL, MOVEFILE_DELAY_UNTIL_REBOOT)) {
            return 0;
        }
        install_set_error(error, error_size, "Cannot remove directory: %s", path);
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
            install_set_error(error, error_size, "Cannot delete file: %s", path);
            return -1;
        }
        return 0;
    }

    dir = opendir(path);
    if (dir == NULL) {
        install_set_error(error, error_size, "Cannot read directory for deletion: %s", path);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char child[OS_MAX_PATH_LEN];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (install_join(child, sizeof(child), path, entry->d_name) != 0) {
            closedir(dir);
            install_set_error(error, error_size, "Delete path is too long.");
            return -1;
        }

        if (install_remove_tree(child, error, error_size) != 0) {
            closedir(dir);
            return -1;
        }
    }

    closedir(dir);
    if (rmdir(path) != 0) {
        install_set_error(error, error_size, "Cannot remove directory: %s", path);
        return -1;
    }
    return 0;
#endif
}
