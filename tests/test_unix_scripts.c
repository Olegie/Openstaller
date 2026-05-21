#include "openstaller/openstaller.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <process.h>
#define OS_PATH_SEP '\\'
#else
#include <sys/stat.h>
#include <unistd.h>
#define OS_PATH_SEP '/'
#endif

static int is_sep(char ch)
{
    return ch == '/' || ch == '\\';
}

static int copy_text(char *dst, size_t dst_size, const char *src)
{
    size_t len = strlen(src);
    if (len >= dst_size) {
        return -1;
    }
    memcpy(dst, src, len + 1);
    return 0;
}

static int append_text(char *dst, size_t dst_size, const char *src)
{
    size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);
    if (dst_len + src_len >= dst_size) {
        return -1;
    }
    memcpy(dst + dst_len, src, src_len + 1);
    return 0;
}

static int join_path(char *out, size_t out_size, const char *left, const char *right)
{
    size_t len;

    if (copy_text(out, out_size, left) != 0) {
        return -1;
    }

    len = strlen(out);
    if (len > 0 && !is_sep(out[len - 1])) {
        char sep[2] = {OS_PATH_SEP, '\0'};
        if (append_text(out, out_size, sep) != 0) {
            return -1;
        }
    }

    return append_text(out, out_size, right);
}

static int make_dir(const char *path)
{
#if defined(_WIN32)
    return _mkdir(path) == 0 ? 0 : -1;
#else
    return mkdir(path, 0755) == 0 ? 0 : -1;
#endif
}

static int write_file(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }
    if (fwrite(text, 1, strlen(text), file) != strlen(text)) {
        fclose(file);
        return -1;
    }
    return fclose(file);
}

static int file_contains(const char *path, const char *needle)
{
    FILE *file = fopen(path, "rb");
    long size;
    char *data;
    int found;

    if (file == NULL) {
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

    data = (char *)malloc((size_t)size + 1);
    if (data == NULL) {
        fclose(file);
        return 0;
    }

    if (fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return 0;
    }
    data[size] = '\0';
    fclose(file);

    found = strstr(data, needle) != NULL;
    free(data);
    return found;
}

static void remove_tree_best_effort(const char *path)
{
#if defined(_WIN32)
    char command[OS_MAX_PATH_LEN + 32];
    snprintf(command, sizeof(command), "rmdir /s /q \"%s\" >nul 2>nul", path);
    system(command);
#else
    char command[OS_MAX_PATH_LEN + 32];
    snprintf(command, sizeof(command), "rm -rf -- '%s' >/dev/null 2>&1", path);
    system(command);
#endif
}

int main(void)
{
    OsProjectConfig config;
    OsGenerationResult result;
    char root[OS_MAX_PATH_LEN];
    char source_dir[OS_MAX_PATH_LEN];
    char output_dir[OS_MAX_PATH_LEN];
    char payload_file[OS_MAX_PATH_LEN];
    char install_script[OS_MAX_PATH_LEN];
    char uninstall_script[OS_MAX_PATH_LEN];
    unsigned pid;

#if defined(_WIN32)
    const char *tmp = getenv("TEMP");
    pid = (unsigned)_getpid();
#else
    const char *tmp = "/tmp";
    pid = (unsigned)getpid();
#endif

    if (tmp == NULL || tmp[0] == '\0') {
        fprintf(stderr, "temporary directory is not available\n");
        return 1;
    }

    snprintf(root, sizeof(root), "%s%copenstaller-unix-script-test-%u", tmp, OS_PATH_SEP, pid);
    if (join_path(source_dir, sizeof(source_dir), root, "payload") != 0 ||
        join_path(output_dir, sizeof(output_dir), root, "dist") != 0 ||
        join_path(payload_file, sizeof(payload_file), source_dir, "hello.txt") != 0) {
        fprintf(stderr, "test path is too long\n");
        return 1;
    }

    remove_tree_best_effort(root);
    if (make_dir(root) != 0 || make_dir(source_dir) != 0 || write_file(payload_file, "hello\n") != 0) {
        fprintf(stderr, "cannot prepare test payload\n");
        remove_tree_best_effort(root);
        return 1;
    }

    os_config_init(&config);
    copy_text(config.app_name, sizeof(config.app_name), "Unix Script Test");
    copy_text(config.company_name, sizeof(config.company_name), "Openstaller Tests");
    copy_text(config.app_version, sizeof(config.app_version), "2.0.0");
    copy_text(config.source_dir, sizeof(config.source_dir), source_dir);
    copy_text(config.output_dir, sizeof(config.output_dir), output_dir);
    copy_text(config.install_dir, sizeof(config.install_dir), "$HOME/.local/share/unix-script-test");
    copy_text(config.launcher, sizeof(config.launcher), "bin/unix-script-test");
    config.generate_native_exe = 0;
    config.generate_windows = 0;
    config.generate_unix = 1;

    if (os_generate_project(&config, &result) != 0) {
        fprintf(stderr, "generation failed: %s\n", result.message);
        remove_tree_best_effort(root);
        return 1;
    }

    if (join_path(install_script, sizeof(install_script), result.package_dir, "install.sh") != 0 ||
        join_path(uninstall_script, sizeof(uninstall_script), result.package_dir, "uninstall.sh") != 0) {
        fprintf(stderr, "script path is too long\n");
        remove_tree_best_effort(root);
        return 1;
    }

    if (!file_contains(install_script, "UNAME=$(uname -s") ||
        !file_contains(install_script, "Library/Application Support/Openstaller") ||
        !file_contains(install_script, "XDG_DATA_HOME") ||
        !file_contains(install_script, "$APP_KEY-uninstall.command") ||
        !file_contains(install_script, "$APP_KEY.desktop") ||
        !file_contains(install_script, "update-desktop-database") ||
        !file_contains(install_script, "APP_LAUNCHER='bin/unix-script-test'") ||
        !file_contains(uninstall_script, "$APP_KEY.command") ||
        !file_contains(uninstall_script, "$APP_KEY.desktop")) {
        fprintf(stderr, "generated Unix scripts do not contain Linux/macOS integration hooks\n");
        remove_tree_best_effort(root);
        return 1;
    }

    remove_tree_best_effort(root);
    return 0;
}
