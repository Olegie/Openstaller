#include "openstaller_install_internal.h"
#include "openstaller_platform.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

int install_uninstaller_store_dir(const OsPackageManifest *manifest, char *out, size_t out_size)
{
    char root[OS_MAX_PATH_LEN];
    char package_key[OS_MAX_NAME_LEN + 40];

    if (os_package_key(package_key, sizeof(package_key), manifest) != 0) {
        return -1;
    }

    if (os_platform_uninstaller_root(root, sizeof(root)) != 0) {
        return -1;
    }

    return install_join(out, out_size, root, package_key);
}

#if defined(_WIN32)
static int install_reg_set_string(HKEY key, const char *name, const char *value)
{
    return RegSetValueExA(key,
                          name,
                          0,
                          REG_SZ,
                          (const BYTE *)value,
                          (DWORD)(strlen(value) + 1)) == ERROR_SUCCESS ? 0 : -1;
}

static int install_windows_abs_path(const char *path)
{
    return path != NULL &&
           ((path[0] != '\0' && path[1] == ':') ||
            (path[0] == '\\' && path[1] == '\\'));
}

static void install_windows_shortcut_name(const char *name, char *out, size_t out_size)
{
    size_t written = 0;

    if (out_size == 0) {
        return;
    }

    while (name != NULL && *name != '\0' && written + 1 < out_size) {
        unsigned char ch = (unsigned char)*name++;

        if (ch < 32 || strchr("<>:\"/\\|?*", ch) != NULL) {
            if (written > 0 && out[written - 1] != ' ') {
                out[written++] = ' ';
            }
        } else {
            out[written++] = (char)ch;
        }
    }

    while (written > 0 && (out[written - 1] == ' ' || out[written - 1] == '.')) {
        --written;
    }

    if (written == 0) {
        install_copy(out, out_size, "Application");
        return;
    }

    out[written] = '\0';
}

static int install_windows_start_menu_dir(const OsPackageManifest *manifest, char *out, size_t out_size)
{
    char root[OS_MAX_PATH_LEN];
    char folder[OS_MAX_NAME_LEN];

    if (os_platform_applications_dir(root, sizeof(root)) != 0) {
        return -1;
    }

    install_windows_shortcut_name(manifest->app_name, folder, sizeof(folder));
    return install_join(out, out_size, root, folder);
}

static int install_windows_to_wide(const char *path, WCHAR *out, size_t out_count)
{
    int written;

    if (path == NULL || out == NULL || out_count == 0) {
        return -1;
    }

    written = MultiByteToWideChar(CP_ACP, 0, path, -1, out, (int)out_count);
    return written > 0 && (size_t)written <= out_count ? 0 : -1;
}

static int install_windows_create_shortcut(const char *shortcut_path,
                                           const char *target_path,
                                           const char *arguments,
                                           const char *working_dir,
                                           const char *description,
                                           char *error,
                                           size_t error_size)
{
    IShellLinkA *link;
    IPersistFile *persist;
    WCHAR wide_shortcut[OS_MAX_PATH_LEN];
    HRESULT init_hr;
    HRESULT hr;
    int did_init = 0;

    init_hr = CoInitialize(NULL);
    if (SUCCEEDED(init_hr)) {
        did_init = 1;
    } else if (init_hr != RPC_E_CHANGED_MODE) {
        install_set_error(error, error_size, "Cannot initialize Windows shell integration.");
        return -1;
    }

    hr = CoCreateInstance(&CLSID_ShellLink,
                          NULL,
                          CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkA,
                          (void **)&link);
    if (FAILED(hr)) {
        if (did_init) {
            CoUninitialize();
        }
        install_set_error(error, error_size, "Cannot create Windows Start Menu shortcut.");
        return -1;
    }

    IShellLinkA_SetPath(link, target_path);
    if (arguments != NULL && arguments[0] != '\0') {
        IShellLinkA_SetArguments(link, arguments);
    }
    if (working_dir != NULL && working_dir[0] != '\0') {
        IShellLinkA_SetWorkingDirectory(link, working_dir);
    }
    if (description != NULL && description[0] != '\0') {
        IShellLinkA_SetDescription(link, description);
    }

    hr = IShellLinkA_QueryInterface(link, &IID_IPersistFile, (void **)&persist);
    if (FAILED(hr) || install_windows_to_wide(shortcut_path, wide_shortcut, sizeof(wide_shortcut) / sizeof(wide_shortcut[0])) != 0) {
        IShellLinkA_Release(link);
        if (did_init) {
            CoUninitialize();
        }
        install_set_error(error, error_size, "Cannot prepare Windows Start Menu shortcut path.");
        return -1;
    }

    hr = IPersistFile_Save(persist, wide_shortcut, TRUE);
    IPersistFile_Release(persist);
    IShellLinkA_Release(link);
    if (did_init) {
        CoUninitialize();
    }

    if (FAILED(hr)) {
        install_set_error(error, error_size, "Cannot write Windows Start Menu shortcut: %s", shortcut_path);
        return -1;
    }

    return 0;
}

static int install_windows_launcher_path(const OsPackageManifest *manifest,
                                         const char *install_dir,
                                         char *out,
                                         size_t out_size)
{
    if (manifest->launcher[0] == '\0') {
        return -1;
    }

    if (install_windows_abs_path(manifest->launcher)) {
        return install_copy(out, out_size, manifest->launcher);
    }

    return install_join(out, out_size, install_dir, manifest->launcher);
}

static int install_windows_uninstaller_path(const char *package_dir, char *out, size_t out_size)
{
    DWORD attrs;

    if (install_join(out, out_size, package_dir, "uninstaller.exe") == 0) {
        attrs = GetFileAttributesA(out);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            return 0;
        }
    }

    if (install_join(out, out_size, package_dir, "uninstall.bat") == 0) {
        attrs = GetFileAttributesA(out);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            return 0;
        }
    }

    return install_join(out, out_size, package_dir, "uninstaller.exe");
}

static int install_windows_register_start_menu(const char *package_dir,
                                               const OsPackageManifest *manifest,
                                               const char *install_dir,
                                               char *error,
                                               size_t error_size)
{
    char folder[OS_MAX_PATH_LEN];
    char app_name[OS_MAX_NAME_LEN];
    char uninstall_name[OS_MAX_NAME_LEN + 32];
    char shortcut_path[OS_MAX_PATH_LEN];
    char launcher_path[OS_MAX_PATH_LEN];
    char uninstaller_path[OS_MAX_PATH_LEN];
    char uninstall_args[OS_MAX_PATH_LEN + 4];

    if (install_windows_start_menu_dir(manifest, folder, sizeof(folder)) != 0 ||
        install_mkdirs(folder) != 0) {
        install_set_error(error, error_size, "Cannot create Windows Start Menu folder.");
        return -1;
    }

    install_windows_shortcut_name(manifest->app_name, app_name, sizeof(app_name));
    if (manifest->launcher[0] != '\0') {
        if (install_windows_launcher_path(manifest, install_dir, launcher_path, sizeof(launcher_path)) != 0 ||
            install_join(shortcut_path, sizeof(shortcut_path), folder, app_name) != 0 ||
            install_append(shortcut_path, sizeof(shortcut_path), ".lnk") != 0 ||
            install_windows_create_shortcut(shortcut_path,
                                            launcher_path,
                                            NULL,
                                            install_dir,
                                            manifest->app_name,
                                            error,
                                            error_size) != 0) {
            return -1;
        }
    }

    if (install_windows_uninstaller_path(package_dir, uninstaller_path, sizeof(uninstaller_path)) != 0 ||
        snprintf(uninstall_args, sizeof(uninstall_args), "\"%s\"", install_dir) >= (int)sizeof(uninstall_args) ||
        snprintf(uninstall_name, sizeof(uninstall_name), "Uninstall %s", app_name) >= (int)sizeof(uninstall_name) ||
        install_join(shortcut_path, sizeof(shortcut_path), folder, uninstall_name) != 0 ||
        install_append(shortcut_path, sizeof(shortcut_path), ".lnk") != 0 ||
        install_windows_create_shortcut(shortcut_path,
                                        uninstaller_path,
                                        uninstall_args,
                                        package_dir,
                                        uninstall_name,
                                        error,
                                        error_size) != 0) {
        return -1;
    }

    return 0;
}

static void install_windows_unregister_start_menu(const OsPackageManifest *manifest)
{
    char folder[OS_MAX_PATH_LEN];
    char app_name[OS_MAX_NAME_LEN];
    char uninstall_name[OS_MAX_NAME_LEN + 32];
    char shortcut_path[OS_MAX_PATH_LEN];

    if (install_windows_start_menu_dir(manifest, folder, sizeof(folder)) != 0) {
        return;
    }

    install_windows_shortcut_name(manifest->app_name, app_name, sizeof(app_name));
    if (install_join(shortcut_path, sizeof(shortcut_path), folder, app_name) == 0 &&
        install_append(shortcut_path, sizeof(shortcut_path), ".lnk") == 0) {
        DeleteFileA(shortcut_path);
    }

    if (snprintf(uninstall_name, sizeof(uninstall_name), "Uninstall %s", app_name) < (int)sizeof(uninstall_name) &&
        install_join(shortcut_path, sizeof(shortcut_path), folder, uninstall_name) == 0 &&
        install_append(shortcut_path, sizeof(shortcut_path), ".lnk") == 0) {
        DeleteFileA(shortcut_path);
    }

    RemoveDirectoryA(folder);
}

static LONG install_reg_delete_tree_legacy(HKEY root, const char *subkey)
{
    HKEY key;
    LONG status;

    status = RegOpenKeyExA(root, subkey, 0, KEY_READ | KEY_WRITE, &key);
    if (status != ERROR_SUCCESS) {
        return status;
    }

    for (;;) {
        char child_name[256];
        DWORD child_size = sizeof(child_name);
        FILETIME write_time;

        status = RegEnumKeyExA(key, 0, child_name, &child_size, NULL, NULL, NULL, &write_time);
        if (status == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (status != ERROR_SUCCESS) {
            RegCloseKey(key);
            return status;
        }

        status = install_reg_delete_tree_legacy(key, child_name);
        if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
            RegCloseKey(key);
            return status;
        }
    }

    RegCloseKey(key);
    return RegDeleteKeyA(root, subkey);
}

int install_register_package(const char *package_dir,
                             const OsPackageManifest *manifest,
                             const char *install_dir,
                             char *error,
                             size_t error_size)
{
    HKEY key;
    char key_name[OS_MAX_PATH_LEN];
    char package_key[OS_MAX_NAME_LEN + 40];
    char uninstaller_path[OS_MAX_PATH_LEN];
    char uninstall_string[OS_MAX_PATH_LEN * 2];
    DWORD disposition;

    if (!manifest->register_system) {
        return 0;
    }

    if (os_package_key(package_key, sizeof(package_key), manifest) != 0 ||
        install_copy(key_name, sizeof(key_name), "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") != 0 ||
        install_append(key_name, sizeof(key_name), package_key) != 0 ||
        install_join(uninstaller_path, sizeof(uninstaller_path), package_dir, "uninstaller.exe") != 0) {
        install_set_error(error, error_size, "Registration path is too long.");
        return -1;
    }

    snprintf(uninstall_string, sizeof(uninstall_string), "\"%s\" \"%s\"", uninstaller_path, install_dir);

    if (RegCreateKeyExA(HKEY_CURRENT_USER,
                        key_name,
                        0,
                        NULL,
                        0,
                        KEY_SET_VALUE,
                        NULL,
                        &key,
                        &disposition) != ERROR_SUCCESS) {
        install_set_error(error, error_size, "Cannot create Windows uninstall registration.");
        return -1;
    }

    (void)disposition;
    if (install_reg_set_string(key, "DisplayName", manifest->app_name) != 0 ||
        install_reg_set_string(key, "DisplayVersion", manifest->app_version) != 0 ||
        install_reg_set_string(key, "InstallLocation", install_dir) != 0 ||
        install_reg_set_string(key, "Publisher", manifest->company_name[0] != '\0' ? manifest->company_name : manifest->app_name) != 0 ||
        install_reg_set_string(key, "UninstallString", uninstall_string) != 0) {
        RegCloseKey(key);
        install_set_error(error, error_size, "Cannot write Windows uninstall registration.");
        return -1;
    }

    RegCloseKey(key);
    if (install_windows_register_start_menu(package_dir, manifest, install_dir, error, error_size) != 0) {
#if defined(OPENSTALLER_WIN2000_COMPAT)
        install_reg_delete_tree_legacy(HKEY_CURRENT_USER, key_name);
#else
        RegDeleteTreeA(HKEY_CURRENT_USER, key_name);
#endif
        return -1;
    }

    return 0;
}

int install_unregister_package(const OsPackageManifest *manifest, char *error, size_t error_size)
{
    char key_name[OS_MAX_PATH_LEN];
    char package_key[OS_MAX_NAME_LEN + 40];
    LONG status;

    if (!manifest->register_system) {
        return 0;
    }

    if (os_package_key(package_key, sizeof(package_key), manifest) != 0 ||
        install_copy(key_name, sizeof(key_name), "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") != 0 ||
        install_append(key_name, sizeof(key_name), package_key) != 0) {
        install_set_error(error, error_size, "Registration path is too long.");
        return -1;
    }

#if defined(OPENSTALLER_WIN2000_COMPAT)
    status = install_reg_delete_tree_legacy(HKEY_CURRENT_USER, key_name);
#else
    status = RegDeleteTreeA(HKEY_CURRENT_USER, key_name);
#endif
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
        install_set_error(error, error_size, "Cannot delete Windows uninstall registration.");
        return -1;
    }

    install_windows_unregister_start_menu(manifest);
    return 0;
}
#else
static int install_resolve_unix_uninstall_command(const char *package_dir, char *out, size_t out_size)
{
    char native_path[OS_MAX_PATH_LEN];

    if (install_join(native_path, sizeof(native_path), package_dir, "uninstaller") == 0 &&
        install_file_exists(native_path)) {
        return install_copy(out, out_size, native_path);
    }

    return install_join(out, out_size, package_dir, "uninstall.sh");
}

static int install_write_linux_desktop_entry(const char *path,
                                             const OsPackageManifest *manifest,
                                             const char *package_key,
                                             const char *install_dir,
                                             char *error,
                                             size_t error_size)
{
    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        install_set_error(error, error_size, "Cannot write desktop launcher: %s", path);
        return -1;
    }

    fprintf(file,
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=%s\n"
            "Exec=\"%s/%s\"\n"
            "Terminal=false\n"
            "Categories=Utility;\n"
            "X-Openstaller-Package=%s\n"
            "X-Openstaller-Publisher=%s\n",
            manifest->app_name,
            install_dir,
            manifest->launcher,
            package_key,
            manifest->company_name);

    if (fclose(file) != 0) {
        install_set_error(error, error_size, "Cannot finish desktop launcher: %s", path);
        return -1;
    }

    chmod(path, 0644);
    return 0;
}

static int install_write_macos_launch_command(const char *path,
                                              const OsPackageManifest *manifest,
                                              const char *install_dir,
                                              char *error,
                                              size_t error_size)
{
    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        install_set_error(error, error_size, "Cannot write macOS launcher command: %s", path);
        return -1;
    }

    fprintf(file,
            "#!/bin/sh\n"
            "exec \"%s/%s\" \"$@\"\n",
            install_dir,
            manifest->launcher);

    if (fclose(file) != 0) {
        install_set_error(error, error_size, "Cannot finish macOS launcher command: %s", path);
        return -1;
    }

    chmod(path, 0755);
    return 0;
}

static int install_write_macos_uninstall_command(const char *path,
                                                 const char *uninstall_command,
                                                 const char *install_dir,
                                                 char *error,
                                                 size_t error_size)
{
    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        install_set_error(error, error_size, "Cannot write macOS uninstall command: %s", path);
        return -1;
    }

    fprintf(file,
            "#!/bin/sh\n"
            "exec \"%s\" \"%s\"\n",
            uninstall_command,
            install_dir);

    if (fclose(file) != 0) {
        install_set_error(error, error_size, "Cannot finish macOS uninstall command: %s", path);
        return -1;
    }

    chmod(path, 0755);
    return 0;
}

int install_register_package(const char *package_dir,
                             const OsPackageManifest *manifest,
                             const char *install_dir,
                             char *error,
                             size_t error_size)
{
    char root[OS_MAX_PATH_LEN];
    char app_dir[OS_MAX_PATH_LEN];
    char applications_dir[OS_MAX_PATH_LEN];
    char package_key[OS_MAX_NAME_LEN + 40];
    char manifest_path[OS_MAX_PATH_LEN];
    char uninstall_command[OS_MAX_PATH_LEN];
    char desktop_path[OS_MAX_PATH_LEN];
    char launch_command_path[OS_MAX_PATH_LEN];
    char uninstall_command_path[OS_MAX_PATH_LEN];
    FILE *file;

    if (!manifest->register_system) {
        return 0;
    }

    desktop_path[0] = '\0';
    launch_command_path[0] = '\0';
    uninstall_command_path[0] = '\0';

    if (os_platform_registration_root(root, sizeof(root)) != 0) {
        install_set_error(error, error_size, "HOME is not set; cannot register package.");
        return -1;
    }

    if (os_package_key(package_key, sizeof(package_key), manifest) != 0 ||
        install_join(app_dir, sizeof(app_dir), root, package_key) != 0 ||
        install_join(manifest_path, sizeof(manifest_path), app_dir, "manifest") != 0 ||
        install_resolve_unix_uninstall_command(package_dir, uninstall_command, sizeof(uninstall_command)) != 0) {
        install_set_error(error, error_size, "Registration path is too long.");
        return -1;
    }

    if (install_mkdirs(app_dir) != 0) {
        install_set_error(error, error_size, "Cannot create Unix-like uninstall registration.");
        return -1;
    }

    if (os_platform_applications_dir(applications_dir, sizeof(applications_dir)) == 0) {
        if (install_mkdirs(applications_dir) != 0) {
            install_set_error(error, error_size, "Cannot create user applications directory.");
            return -1;
        }

#if defined(__APPLE__)
        if (manifest->launcher[0] != '\0') {
            if (install_join(launch_command_path, sizeof(launch_command_path), applications_dir, package_key) != 0 ||
                install_append(launch_command_path, sizeof(launch_command_path), ".command") != 0) {
                install_set_error(error, error_size, "macOS launcher command path is too long.");
                return -1;
            }

            if (install_write_macos_launch_command(launch_command_path, manifest, install_dir, error, error_size) != 0) {
                return -1;
            }
        }

        if (install_join(uninstall_command_path, sizeof(uninstall_command_path), applications_dir, package_key) != 0 ||
            install_append(uninstall_command_path, sizeof(uninstall_command_path), "-uninstall.command") != 0 ||
            install_write_macos_uninstall_command(uninstall_command_path,
                                                 uninstall_command,
                                                 install_dir,
                                                 error,
                                                 error_size) != 0) {
            return -1;
        }
#else
        if (manifest->launcher[0] != '\0' &&
            (install_join(desktop_path, sizeof(desktop_path), applications_dir, package_key) != 0 ||
             install_append(desktop_path, sizeof(desktop_path), ".desktop") != 0 ||
             install_write_linux_desktop_entry(desktop_path,
                                               manifest,
                                               package_key,
                                               install_dir,
                                               error,
                                               error_size) != 0)) {
            return -1;
        }
#endif
    }

    file = fopen(manifest_path, "wb");
    if (file == NULL) {
        install_set_error(error, error_size, "Cannot write Unix-like uninstall registration.");
        return -1;
    }

    fprintf(file,
            "platform=%s\n"
            "name=%s\n"
            "company=%s\n"
            "version=%s\n"
            "install_dir=%s\n"
            "package_dir=%s\n"
            "uninstall=%s\n"
            "desktop_entry=%s\n"
            "launch_command=%s\n"
            "uninstall_command=%s\n",
            os_platform_family(),
            manifest->app_name,
            manifest->company_name,
            manifest->app_version,
            install_dir,
            package_dir,
            uninstall_command,
            desktop_path,
            launch_command_path,
            uninstall_command_path);
    fclose(file);
    return 0;
}

int install_unregister_package(const OsPackageManifest *manifest, char *error, size_t error_size)
{
    char root[OS_MAX_PATH_LEN];
    char app_dir[OS_MAX_PATH_LEN];
    char applications_dir[OS_MAX_PATH_LEN];
    char package_key[OS_MAX_NAME_LEN + 40];

    if (!manifest->register_system) {
        return 0;
    }

    if (os_platform_registration_root(root, sizeof(root)) != 0) {
        return 0;
    }

    if (os_package_key(package_key, sizeof(package_key), manifest) != 0 ||
        install_join(app_dir, sizeof(app_dir), root, package_key) != 0) {
        install_set_error(error, error_size, "Registration path is too long.");
        return -1;
    }

    if (os_platform_applications_dir(applications_dir, sizeof(applications_dir)) == 0) {
        char shortcut[OS_MAX_PATH_LEN];

#if defined(__APPLE__)
        if (install_join(shortcut, sizeof(shortcut), applications_dir, package_key) == 0 &&
            install_append(shortcut, sizeof(shortcut), ".command") == 0) {
            unlink(shortcut);
        }

        if (install_join(shortcut, sizeof(shortcut), applications_dir, package_key) == 0 &&
            install_append(shortcut, sizeof(shortcut), "-uninstall.command") == 0) {
            unlink(shortcut);
        }
#else
        if (install_join(shortcut, sizeof(shortcut), applications_dir, package_key) == 0 &&
            install_append(shortcut, sizeof(shortcut), ".desktop") == 0) {
            unlink(shortcut);
        }
#endif
    }

    return install_remove_tree(app_dir, error, error_size);
}
#endif
