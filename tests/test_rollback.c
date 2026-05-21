#include "openstaller_rollback.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <process.h>
#include <windows.h>
#define OS_PATH_SEP '\\'
#else
#include <sys/stat.h>
#include <unistd.h>
#define OS_PATH_SEP '/'
#endif

static int write_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }
    if (fwrite(text, 1, strlen(text), file) != strlen(text) || fclose(file) != 0) {
        return -1;
    }
    return 0;
}

static int read_text(const char *path, char *out, size_t out_size)
{
    FILE *file = fopen(path, "rb");
    size_t count;

    if (file == NULL || out_size == 0) {
        return -1;
    }

    count = fread(out, 1, out_size - 1, file);
    if (ferror(file) != 0) {
        fclose(file);
        return -1;
    }
    fclose(file);
    out[count] = '\0';
    return 0;
}

static int exists_file(const char *path)
{
#if defined(_WIN32)
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    return access(path, F_OK) == 0;
#endif
}

static int join_path(char *out, size_t out_size, const char *left, const char *right)
{
    int written = snprintf(out, out_size, "%s%c%s", left, OS_PATH_SEP, right);
    return written > 0 && (size_t)written < out_size ? 0 : -1;
}

static int make_dir(const char *path)
{
#if defined(_WIN32)
    return _mkdir(path) == 0 ? 0 : -1;
#else
    return mkdir(path, 0755) == 0 ? 0 : -1;
#endif
}

static void remove_file_best_effort(const char *path)
{
#if defined(_WIN32)
    DeleteFileA(path);
#else
    unlink(path);
#endif
}

static void remove_dir_best_effort(const char *path)
{
#if defined(_WIN32)
    RemoveDirectoryA(path);
#else
    rmdir(path);
#endif
}

int main(void)
{
    OsRollback rollback;
    char root[OS_MAX_PATH_LEN];
    char existing[OS_MAX_PATH_LEN];
    char created[OS_MAX_PATH_LEN];
    char text[64];
    char error[OS_MAX_MESSAGE_LEN];

#if defined(_WIN32)
    snprintf(root, sizeof(root), "%s\\openstaller-rollback-test-%u", getenv("TEMP"), (unsigned)_getpid());
#else
    snprintf(root, sizeof(root), "/tmp/openstaller-rollback-test-%u", (unsigned)getpid());
#endif

    remove_file_best_effort(existing);
    remove_file_best_effort(created);
    remove_dir_best_effort(root);
    if (make_dir(root) != 0 ||
        join_path(existing, sizeof(existing), root, "existing.txt") != 0 ||
        join_path(created, sizeof(created), root, "created.txt") != 0) {
        fprintf(stderr, "rollback test setup failed\n");
        return 1;
    }

    error[0] = '\0';
    if (write_text(existing, "before") != 0 ||
        os_rollback_begin(&rollback, error, sizeof(error)) != 0 ||
        os_rollback_capture_file(&rollback, existing, error, sizeof(error)) != 0 ||
        os_rollback_capture_file(&rollback, created, error, sizeof(error)) != 0 ||
        write_text(existing, "after") != 0 ||
        write_text(created, "new") != 0) {
        fprintf(stderr, "rollback capture failed: %s\n", error);
        return 1;
    }

    os_rollback_revert(&rollback, error, sizeof(error));

    if (read_text(existing, text, sizeof(text)) != 0 || strcmp(text, "before") != 0) {
        fprintf(stderr, "rollback did not restore an overwritten file\n");
        return 1;
    }
    if (exists_file(created)) {
        fprintf(stderr, "rollback did not remove a created file\n");
        return 1;
    }

    remove_file_best_effort(existing);
    remove_dir_best_effort(root);
    return 0;
}
