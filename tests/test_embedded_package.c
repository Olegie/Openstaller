#include "openstaller/openstaller.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <process.h>
#include <windows.h>
#define OS_PATH_SEP '\\'
#else
#include <unistd.h>
#define OS_PATH_SEP '/'
#endif

typedef struct TestPackHeader {
    char magic[16];
    uint32_t version;
    uint32_t entry_count;
} TestPackHeader;

typedef struct TestPackEntryHeader {
    uint32_t type;
    uint32_t path_len;
    uint64_t size;
    uint32_t flags;
    uint32_t reserved;
    uint64_t original_size;
} TestPackEntryHeader;

typedef struct TestPackFooter {
    char magic[16];
    uint64_t archive_offset;
    uint64_t archive_size;
    uint32_t entry_count;
    uint32_t reserved;
} TestPackFooter;

static int embedded_payload_is_compressed(const char *path)
{
    static const char magic[16] = {
        'O', 'S', 'T', 'A', 'L', 'L', 'E', 'R', 'P', 'K', 'G', '1', 0, 0, 0, 0
    };
    FILE *file = fopen(path, "rb");
    TestPackFooter footer;
    TestPackHeader header;
    uint32_t i;

    if (file == NULL) {
        return 0;
    }
    if (fseek(file, -(long)sizeof(footer), SEEK_END) != 0 ||
        fread(&footer, 1, sizeof(footer), file) != sizeof(footer) ||
        memcmp(footer.magic, magic, sizeof(magic)) != 0 ||
        fseek(file, (long)footer.archive_offset, SEEK_SET) != 0 ||
        fread(&header, 1, sizeof(header), file) != sizeof(header) ||
        memcmp(header.magic, magic, sizeof(magic)) != 0 ||
        header.version < 2) {
        fclose(file);
        return 0;
    }

    for (i = 0; i < header.entry_count; ++i) {
        TestPackEntryHeader entry;
        if (fread(&entry, 1, sizeof(entry), file) != sizeof(entry) ||
            fseek(file, (long)entry.path_len, SEEK_CUR) != 0) {
            fclose(file);
            return 0;
        }

        if (entry.type == 2) {
            fclose(file);
            return (entry.flags & 0x00000001u) != 0 && entry.original_size > 0;
        }

        if (fseek(file, (long)entry.size, SEEK_CUR) != 0) {
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return 0;
}

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

static int file_exists(const char *path)
{
#if defined(_WIN32)
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    return access(path, R_OK) == 0;
#endif
}

static int dir_exists(const char *path)
{
#if defined(_WIN32)
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    return access(path, F_OK) == 0;
#endif
}

#if defined(_WIN32)
static int start_menu_shortcut_exists(const char *folder_name, const char *shortcut_name)
{
    char root[OS_MAX_PATH_LEN];
    char folder[OS_MAX_PATH_LEN];
    char shortcut[OS_MAX_PATH_LEN];
    DWORD needed;

    needed = ExpandEnvironmentStringsA("%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs",
                                       root,
                                       (DWORD)sizeof(root));
    if (needed == 0 || needed > sizeof(root)) {
        return 0;
    }

    if (join_path(folder, sizeof(folder), root, folder_name) != 0 ||
        join_path(shortcut, sizeof(shortcut), folder, shortcut_name) != 0 ||
        append_text(shortcut, sizeof(shortcut), ".lnk") != 0) {
        return 0;
    }

    return file_exists(shortcut);
}
#endif

static int write_test_bmp(const char *path)
{
    static const unsigned char bmp[] = {
        0x42, 0x4d, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
        0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x13, 0x0b,
        0x00, 0x00, 0x13, 0x0b, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x20,
        0xf0, 0x30, 0x60, 0xff, 0x00, 0x00, 0x18, 0xa8,
        0x3c, 0x1c, 0x74, 0xd4, 0x00, 0x00
    };
    FILE *file = fopen(path, "wb");
    int ok;

    if (file == NULL) {
        return -1;
    }

    ok = fwrite(bmp, 1, sizeof(bmp), file) == sizeof(bmp);
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok ? 0 : -1;
}

#if defined(_WIN32)
static int exe_has_icon_group(const char *path)
{
    HMODULE module = LoadLibraryExA(path, NULL, LOAD_LIBRARY_AS_DATAFILE);
    HRSRC resource;
    DWORD size;

    if (module == NULL) {
        return 0;
    }

    resource = FindResourceA(module, MAKEINTRESOURCEA(1), RT_GROUP_ICON);
    size = resource != NULL ? SizeofResource(module, resource) : 0;
    FreeLibrary(module);
    return resource != NULL && size > 0;
}

static int exe_has_version_info(const char *path)
{
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeA(path, &handle);
    void *data;
    VS_FIXEDFILEINFO *info;
    UINT info_size = 0;
    int ok;

    (void)handle;
    if (size == 0) {
        return 0;
    }

    data = malloc(size);
    if (data == NULL) {
        return 0;
    }

    ok = GetFileVersionInfoA(path, 0, size, data) != 0 &&
         VerQueryValueA(data, "\\", (LPVOID *)&info, &info_size) != 0 &&
         info_size >= sizeof(*info) &&
         info->dwSignature == 0xfeef04bd;
    free(data);
    return ok;
}

static int exe_version_string_equals(const char *path, const char *name, const char *expected)
{
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeA(path, &handle);
    void *data;
    char query[128];
    char *value = NULL;
    UINT value_size = 0;
    int ok;

    (void)handle;
    if (size == 0) {
        return 0;
    }

    data = malloc(size);
    if (data == NULL) {
        return 0;
    }

    snprintf(query, sizeof(query), "\\StringFileInfo\\040904b0\\%s", name);
    ok = GetFileVersionInfoA(path, 0, size, data) != 0 &&
         VerQueryValueA(data, query, (LPVOID *)&value, &value_size) != 0 &&
         value != NULL &&
         strcmp(value, expected) == 0;
    free(data);
    return ok;
}
#endif

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

typedef struct ProgressState {
    const char *install_root;
    int file_events;
    int complete_events;
    int saw_install_target;
    int max_percent;
} ProgressState;

static void progress_callback(const OsInstallProgressEvent *event, void *user_data)
{
    ProgressState *state = (ProgressState *)user_data;

    if (event == NULL || state == NULL) {
        return;
    }

    if (event->percent > state->max_percent) {
        state->max_percent = event->percent;
    }
    if (event->stage == OS_INSTALL_PROGRESS_FILE) {
        state->file_events++;
    }
    if (event->stage == OS_INSTALL_PROGRESS_COMPLETE) {
        state->complete_events++;
    }
    if (event->target_path != NULL && strstr(event->target_path, state->install_root) != NULL) {
        state->saw_install_target = 1;
    }
}

int main(int argc, char **argv)
{
    OsProjectConfig config;
    OsGenerationResult result;
    char output_root[OS_MAX_PATH_LEN];
    char install_root[OS_MAX_PATH_LEN];
    char installer_path[OS_MAX_PATH_LEN];
    char uninstaller_path[OS_MAX_PATH_LEN];
    char installed_file[OS_MAX_PATH_LEN];
    char installed_uninstaller[OS_MAX_PATH_LEN];
    char staging_path[OS_MAX_PATH_LEN];
    char background_path[OS_MAX_PATH_LEN];
    char extracted_background_path[OS_MAX_PATH_LEN];
    char message[OS_MAX_MESSAGE_LEN];

    if (argc < 3) {
        fprintf(stderr, "usage: test_embedded_package RUNTIME_EXE SAMPLE_PAYLOAD\n");
        return 2;
    }

    os_config_init(&config);
    copy_text(config.app_name, sizeof(config.app_name), "Embedded Test App");
    copy_text(config.company_name, sizeof(config.company_name), "Embedded Test Company");
    copy_text(config.app_version, sizeof(config.app_version), "1.0.0");
    copy_text(config.source_dir, sizeof(config.source_dir), argv[2]);
    copy_text(config.runtime_exe, sizeof(config.runtime_exe), argv[1]);
    copy_text(config.launcher, sizeof(config.launcher), "hello.txt");
    config.generate_native_exe = 1;
    config.generate_windows = 0;
    config.generate_unix = 0;
    config.installer_style = OS_INSTALLER_STYLE_MODERN;
    config.window_style = OS_WINDOW_STYLE_RESIZABLE;
    config.page_flags = OS_PAGE_FOLDER | OS_PAGE_COMPONENTS | OS_PAGE_READY | OS_PAGE_FINISH;
    copy_text(config.ui_font, sizeof(config.ui_font), "Segoe UI");
    copy_text(config.theme.accent, sizeof(config.theme.accent), "#0B7A75");
    copy_text(config.theme.progress, sizeof(config.theme.progress), "#C026D3");
    copy_text(config.theme.background, sizeof(config.theme.background), "#F8FAFC");
    copy_text(config.theme.legacy_top, sizeof(config.theme.legacy_top), "#0B1B68");
    config.online_component_count = 1;
    copy_text(config.online_components[0].name, sizeof(config.online_components[0].name), "Optional Model");
    copy_text(config.online_components[0].description,
              sizeof(config.online_components[0].description),
              "Optional online component used for metadata coverage.");
    copy_text(config.online_components[0].url,
              sizeof(config.online_components[0].url),
              "https://example.invalid/openstaller-test-model.bin");
    copy_text(config.online_components[0].target_path,
              sizeof(config.online_components[0].target_path),
              "models/openstaller-test-model.bin");
    config.online_components[0].selected_by_default = 0;

#if defined(_WIN32)
    snprintf(output_root, sizeof(output_root), "%s\\openstaller-test-package-%u", getenv("TEMP"), (unsigned)_getpid());
    snprintf(install_root, sizeof(install_root), "%s\\openstaller-test-install-%u", getenv("TEMP"), (unsigned)_getpid());
    snprintf(background_path, sizeof(background_path), "%s\\openstaller-test-background-%u.bmp", getenv("TEMP"), (unsigned)_getpid());
#else
    snprintf(output_root, sizeof(output_root), "/tmp/openstaller-test-package-%u", (unsigned)getpid());
    snprintf(install_root, sizeof(install_root), "/tmp/openstaller-test-install-%u", (unsigned)getpid());
    snprintf(background_path, sizeof(background_path), "/tmp/openstaller-test-background-%u.bmp", (unsigned)getpid());
#endif
    copy_text(config.output_dir, sizeof(config.output_dir), output_root);
    copy_text(config.install_dir, sizeof(config.install_dir), install_root);
    copy_text(config.background_image_file, sizeof(config.background_image_file), background_path);

    remove_tree_best_effort(output_root);
    remove_tree_best_effort(install_root);
    remove(background_path);

    if (write_test_bmp(background_path) != 0) {
        fprintf(stderr, "cannot write test background bitmap\n");
        return 1;
    }

    if (os_generate_project(&config, &result) != 0) {
        fprintf(stderr, "generate failed: %s\n", result.message);
        return 1;
    }

    if (join_path(installer_path, sizeof(installer_path), result.package_dir, "installer.exe") != 0 ||
        join_path(uninstaller_path, sizeof(uninstaller_path), result.package_dir, "uninstaller.exe") != 0 ||
        join_path(staging_path, sizeof(staging_path), result.package_dir, "payload") != 0 ||
        join_path(extracted_background_path, sizeof(extracted_background_path), result.package_dir, "background-out.bmp") != 0) {
        fprintf(stderr, "path too long\n");
        return 1;
    }

    if (!file_exists(installer_path) || file_exists(uninstaller_path) || dir_exists(staging_path)) {
        fprintf(stderr, "native EXE package is not self-contained\n");
        return 1;
    }

#if defined(OPENSTALLER_HAVE_ZLIB)
    if (!embedded_payload_is_compressed(installer_path)) {
        fprintf(stderr, "embedded payload was not stored with compression\n");
        return 1;
    }
#endif

#if defined(_WIN32)
    if (!exe_has_icon_group(installer_path)) {
        fprintf(stderr, "native EXE package does not contain an installer icon\n");
        return 1;
    }

    if (!exe_has_version_info(installer_path)) {
        fprintf(stderr, "native EXE package does not contain Windows version metadata\n");
        return 1;
    }

    if (!exe_version_string_equals(installer_path, "ProductName", "Embedded Test App") ||
        !exe_version_string_equals(installer_path, "FileDescription", "Embedded Test App Setup")) {
        fprintf(stderr, "generated installer does not contain product-specific version metadata\n");
        return 1;
    }
#endif

    {
        OsPackageInfo info;
        if (os_read_embedded_package_info(installer_path, &info, message, sizeof(message)) != 0 ||
            strcmp(info.app_name, "Embedded Test App") != 0 ||
            strcmp(info.company_name, "Embedded Test Company") != 0 ||
            info.file_count != 1 ||
            !info.has_background_image ||
            info.installer_style != OS_INSTALLER_STYLE_MODERN ||
            info.window_style != OS_WINDOW_STYLE_RESIZABLE ||
            info.page_flags != (OS_PAGE_FOLDER | OS_PAGE_COMPONENTS | OS_PAGE_READY | OS_PAGE_FINISH) ||
            strcmp(info.ui_font, "Segoe UI") != 0 ||
            strcmp(info.theme.accent, "#0B7A75") != 0 ||
            strcmp(info.theme.progress, "#C026D3") != 0 ||
            info.online_component_count != 1 ||
            strcmp(info.online_components[0].name, "Optional Model") != 0 ||
            strcmp(info.online_components[0].url, "https://example.invalid/openstaller-test-model.bin") != 0 ||
            strcmp(info.online_components[0].target_path, "models/openstaller-test-model.bin") != 0 ||
            info.online_components[0].selected_by_default != 0) {
            fprintf(stderr, "embedded branding metadata is wrong: %s\n", message);
            return 1;
        }
        if (os_extract_embedded_background_image(installer_path,
                                                 extracted_background_path,
                                                 message,
                                                 sizeof(message)) != 0 ||
            !file_exists(extracted_background_path)) {
            fprintf(stderr, "embedded background image was not extractable: %s\n", message);
            return 1;
        }
    }

    {
        ProgressState progress;
        int install_status;

        memset(&progress, 0, sizeof(progress));
        progress.install_root = install_root;
        os_set_install_progress_callback(progress_callback, &progress);
        install_status = os_install_embedded_package(installer_path, install_root, message, sizeof(message));
        os_set_install_progress_callback(NULL, NULL);

        if (install_status != 0) {
            fprintf(stderr, "embedded install failed: %s\n", message);
            return 1;
        }
        if (progress.file_events == 0 ||
            progress.complete_events == 0 ||
            !progress.saw_install_target ||
            progress.max_percent < 100) {
            fprintf(stderr, "embedded install did not emit useful progress events\n");
            return 1;
        }
    }

    if (join_path(installed_file, sizeof(installed_file), install_root, "hello.txt") != 0 ||
#if defined(_WIN32)
        join_path(installed_uninstaller, sizeof(installed_uninstaller), install_root, "uninstaller.exe") != 0 ||
#else
        join_path(installed_uninstaller, sizeof(installed_uninstaller), install_root, "uninstaller") != 0 ||
#endif
        !file_exists(installed_file) ||
        !file_exists(installed_uninstaller)) {
        fprintf(stderr, "embedded payload was not installed\n");
        return 1;
    }

#if defined(_WIN32)
    if (!exe_has_icon_group(installed_uninstaller) ||
        !exe_has_version_info(installed_uninstaller) ||
        !exe_version_string_equals(installed_uninstaller, "ProductName", "Embedded Test App") ||
        !exe_version_string_equals(installed_uninstaller, "FileDescription", "Embedded Test App Uninstaller")) {
        fprintf(stderr, "installed embedded uninstaller does not contain product-specific metadata\n");
        return 1;
    }
#endif

#if defined(_WIN32)
    if (!start_menu_shortcut_exists("Embedded Test App", "Embedded Test App") ||
        !start_menu_shortcut_exists("Embedded Test App", "Uninstall Embedded Test App")) {
        fprintf(stderr, "embedded install did not create Windows Start Menu shortcuts\n");
        return 1;
    }
#endif

    if (os_uninstall_embedded_package(installed_uninstaller, install_root, message, sizeof(message)) != 0) {
        fprintf(stderr, "embedded uninstall failed: %s\n", message);
        return 1;
    }

    if (dir_exists(install_root)) {
        fprintf(stderr, "embedded uninstall left install directory behind\n");
        return 1;
    }

#if defined(_WIN32)
    if (start_menu_shortcut_exists("Embedded Test App", "Embedded Test App") ||
        start_menu_shortcut_exists("Embedded Test App", "Uninstall Embedded Test App")) {
        fprintf(stderr, "embedded uninstall left Windows Start Menu shortcuts behind\n");
        return 1;
    }
#endif

    remove_tree_best_effort(output_root);
    remove(background_path);
    return 0;
}
