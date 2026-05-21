#include "openstaller_rollback.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <windows.h>
#define OS_PATH_SEP '\\'
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define OS_PATH_SEP '/'
#endif

static int rb_is_sep(char ch)
{
    return ch == '/' || ch == '\\';
}

static void rb_set_error(char *buffer, size_t size, const char *format, ...)
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

static int rb_copy(char *dst, size_t dst_size, const char *src)
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

static int rb_append(char *dst, size_t dst_size, const char *src)
{
    size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);

    if (dst_len + src_len >= dst_size) {
        return -1;
    }

    memcpy(dst + dst_len, src, src_len + 1);
    return 0;
}

static int rb_join(char *out, size_t out_size, const char *left, const char *right)
{
    size_t len;

    if (rb_copy(out, out_size, left) != 0) {
        return -1;
    }

    len = strlen(out);
    if (len > 0 && !rb_is_sep(out[len - 1])) {
        char sep[2] = {OS_PATH_SEP, '\0'};
        if (rb_append(out, out_size, sep) != 0) {
            return -1;
        }
    }

    return rb_append(out, out_size, right);
}

static int rb_parent_dir(char *out, size_t out_size, const char *path)
{
    size_t len;

    if (rb_copy(out, out_size, path) != 0) {
        return -1;
    }

    len = strlen(out);
    while (len > 0 && !rb_is_sep(out[len - 1])) {
        --len;
    }
    if (len == 0) {
        out[0] = '\0';
        return 0;
    }
    while (len > 1 && rb_is_sep(out[len - 1])) {
        --len;
    }
    out[len] = '\0';
    return 0;
}

static int rb_mkdir_one(const char *path)
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

static int rb_mkdirs(const char *path)
{
    char tmp[OS_MAX_PATH_LEN];
    char *cursor;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    if (rb_copy(tmp, sizeof(tmp), path) != 0) {
        return -1;
    }

    for (cursor = tmp; *cursor != '\0'; ++cursor) {
        if (!rb_is_sep(*cursor) || cursor == tmp) {
            continue;
        }
#if defined(_WIN32)
        if (cursor == tmp + 2 && tmp[1] == ':') {
            continue;
        }
#endif
        *cursor = '\0';
        if (tmp[0] != '\0' && rb_mkdir_one(tmp) != 0) {
            *cursor = OS_PATH_SEP;
            return -1;
        }
        *cursor = OS_PATH_SEP;
    }

    return rb_mkdir_one(tmp);
}

static int rb_target_kind(const char *path)
{
#if defined(_WIN32)
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return 0;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0 ? 2 : 1;
#else
    struct stat st;
    if (lstat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode) ? 2 : 1;
#endif
}

static int rb_copy_file(const char *src, const char *dst)
{
    FILE *in;
    FILE *out;
    unsigned char buffer[65536];
    size_t read_count;
    char parent[OS_MAX_PATH_LEN];

    if (rb_parent_dir(parent, sizeof(parent), dst) != 0 ||
        (parent[0] != '\0' && rb_mkdirs(parent) != 0)) {
        return -1;
    }

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

static void rb_delete_file_best_effort(const char *path)
{
#if defined(_WIN32)
    DeleteFileA(path);
#else
    unlink(path);
#endif
}

static void rb_remove_dir_best_effort(const char *path)
{
#if defined(_WIN32)
    RemoveDirectoryA(path);
#else
    rmdir(path);
#endif
}

static int rb_temp_root(char *out, size_t out_size)
{
#if defined(_WIN32)
    DWORD written = GetTempPathA((DWORD)out_size, out);
    return written > 0 && written < out_size ? 0 : -1;
#else
    const char *tmp = getenv("TMPDIR");
    if (tmp == NULL || tmp[0] == '\0') {
        tmp = "/tmp";
    }
    return rb_copy(out, out_size, tmp);
#endif
}

void os_rollback_init(OsRollback *rollback)
{
    if (rollback != NULL) {
        memset(rollback, 0, sizeof(*rollback));
    }
}

int os_rollback_begin(OsRollback *rollback, char *error, size_t error_size)
{
    char root[OS_MAX_PATH_LEN];
    char name[64];

    if (rollback == NULL) {
        rb_set_error(error, error_size, "Rollback journal is required.");
        return -1;
    }

    os_rollback_init(rollback);
    if (rb_temp_root(root, sizeof(root)) != 0) {
        rb_set_error(error, error_size, "Cannot locate temporary folder for rollback journal.");
        return -1;
    }

#if defined(_WIN32)
    snprintf(name, sizeof(name), "openstaller-rollback-%lu-%lu", (unsigned long)GetCurrentProcessId(), (unsigned long)GetTickCount());
#else
    snprintf(name, sizeof(name), "openstaller-rollback-%lu-%lu", (unsigned long)getpid(), (unsigned long)time(NULL));
#endif

    if (rb_join(rollback->staging_dir, sizeof(rollback->staging_dir), root, name) != 0 ||
        rb_mkdirs(rollback->staging_dir) != 0) {
        rb_set_error(error, error_size, "Cannot create rollback staging folder.");
        return -1;
    }

    return 0;
}

static int rb_append_entry(OsRollback *rollback, const OsRollbackEntry *entry)
{
    OsRollbackEntry *next;
    size_t capacity;

    if (rollback->count == rollback->capacity) {
        capacity = rollback->capacity == 0 ? 16u : rollback->capacity * 2u;
        next = (OsRollbackEntry *)realloc(rollback->entries, capacity * sizeof(*rollback->entries));
        if (next == NULL) {
            return -1;
        }
        rollback->entries = next;
        rollback->capacity = capacity;
    }

    rollback->entries[rollback->count++] = *entry;
    return 0;
}

int os_rollback_capture_file(OsRollback *rollback, const char *target, char *error, size_t error_size)
{
    OsRollbackEntry entry;
    char name[64];
    int kind;
    size_t i;

    if (rollback == NULL) {
        return 0;
    }

    for (i = 0; i < rollback->count; ++i) {
        if (strcmp(rollback->entries[i].target, target) == 0) {
            return 0;
        }
    }

    memset(&entry, 0, sizeof(entry));
    if (rb_copy(entry.target, sizeof(entry.target), target) != 0) {
        rb_set_error(error, error_size, "Rollback target path is too long.");
        return -1;
    }

    kind = rb_target_kind(target);
    if (kind == 2) {
        rb_set_error(error, error_size, "Cannot overwrite directory with payload file: %s", target);
        return -1;
    }

    entry.existed = kind == 1;
    if (entry.existed) {
        snprintf(name, sizeof(name), "%08llu.bak", (unsigned long long)rollback->count);
        if (rb_join(entry.backup, sizeof(entry.backup), rollback->staging_dir, name) != 0 ||
            rb_copy_file(target, entry.backup) != 0) {
            rb_set_error(error, error_size, "Cannot create rollback backup for: %s", target);
            return -1;
        }
    }

    if (rb_append_entry(rollback, &entry) != 0) {
        rb_set_error(error, error_size, "Not enough memory for rollback journal.");
        return -1;
    }

    return 0;
}

void os_rollback_commit(OsRollback *rollback)
{
    size_t i;

    if (rollback == NULL) {
        return;
    }

    for (i = 0; i < rollback->count; ++i) {
        if (rollback->entries[i].backup[0] != '\0') {
            rb_delete_file_best_effort(rollback->entries[i].backup);
        }
    }
    if (rollback->staging_dir[0] != '\0') {
        rb_remove_dir_best_effort(rollback->staging_dir);
    }
    free(rollback->entries);
    os_rollback_init(rollback);
}

void os_rollback_revert(OsRollback *rollback, char *error, size_t error_size)
{
    size_t i;
    int failed = 0;

    if (rollback == NULL) {
        return;
    }

    i = rollback->count;
    while (i > 0) {
        OsRollbackEntry *entry = &rollback->entries[--i];

        if (entry->existed) {
            if (rb_copy_file(entry->backup, entry->target) != 0) {
                failed = 1;
            }
        } else {
            rb_delete_file_best_effort(entry->target);
        }
    }

    if (failed && error != NULL && error_size > 0 && error[0] == '\0') {
        rb_set_error(error, error_size, "Installation failed and rollback could not restore every file.");
    }

    os_rollback_commit(rollback);
}
