#include "openstaller/openstaller.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

static void print_help(void)
{
    puts("Openstaller CLI");
    puts("");
    puts("Usage:");
    puts("  openstaller-cli --name NAME --source DIR --output DIR --install-dir DIR [options]");
    puts("");
    puts("Options:");
    puts("  --version VALUE       Application version, default 0.1.0");
    puts("  --company NAME        Publisher or company name used for branding");
    puts("  --license FILE        Copy a license into the package and ask for acceptance");
    puts("  --wizard-image FILE   Embed a BMP image for the installer's left panel");
    puts("  --background-image FILE  Embed a BMP image for the installer's page background");
    puts("  --installer-style STYLE  Installer UI style: classic, modern, or legacy");
    puts("  --ui-font NAME        Font face used by the generated installer UI");
    puts("  --window-style STYLE  Window style: fixed, resizable, or maximized");
    puts("  --pages LIST          Page flow: full, compact, minimal, or comma-separated names");
    puts("  --theme-accent HEX    Accent color, for example #0078D4");
    puts("  --theme-progress HEX  Progress color");
    puts("  --theme-sidebar HEX   Classic/modern side color");
    puts("  --theme-sidebar-dark HEX  Classic dark side color");
    puts("  --theme-background HEX  Main background color");
    puts("  --theme-panel HEX     Panel/card color");
    puts("  --theme-text HEX      Main text color");
    puts("  --theme-muted HEX     Secondary text color");
    puts("  --theme-legacy-top HEX     Legacy gradient top color");
    puts("  --theme-legacy-bottom HEX  Legacy gradient bottom color");
    puts("  --online-component NAME URL TARGET  Add a selected online download");
    puts("  --online-optional NAME URL TARGET   Add an optional online download");
    puts("  --online-description TEXT           Description for the last online download");
    puts("  --online-page PAGE                  Ask page for last download: components, ready, or install");
    puts("  --installer-icon FILE Embed a custom ICO icon into installer.exe");
    puts("  --uninstaller-icon FILE Embed a custom ICO icon into uninstaller.exe");
    puts("  --launcher PATH       Relative launcher path used for Unix .desktop registration");
    puts("  --no-native-exe       Do not emit installer/uninstaller executables");
    puts("  --no-register         Do not register uninstall metadata in the system");
    puts("  --windows-only        Generate install.bat and uninstall.bat only");
    puts("  --unix-only           Generate install.sh and uninstall.sh only");
    puts("  --run-install DIR     Install an already generated package directory");
    puts("  --run-uninstall DIR   Uninstall an already generated package directory");
    puts("  --hash-backend        Print the active C/Assembly hash backend");
    puts("  --help                Show this help");
}

static int copy_arg(char *dst, size_t dst_size, const char *value, const char *name)
{
    if (strlen(value) >= dst_size) {
        fprintf(stderr, "Value for %s is too long.\n", name);
        return -1;
    }

    strcpy(dst, value);
    return 0;
}

static int cli_is_sep(char ch)
{
    return ch == '/' || ch == '\\';
}

static const char *cli_basename(const char *path)
{
    const char *base = path;

    while (*path != '\0') {
        if (cli_is_sep(*path)) {
            base = path + 1;
        }
        ++path;
    }

    return base;
}

static int cli_dirname(const char *path, char *out, size_t out_size)
{
    size_t len = strlen(path);

    while (len > 0 && !cli_is_sep(path[len - 1])) {
        --len;
    }

    if (len == 0) {
        return copy_arg(out, out_size, ".", "package-dir");
    }

    while (len > 1 && cli_is_sep(path[len - 1])) {
        --len;
    }

    if (len >= out_size) {
        return -1;
    }

    memcpy(out, path, len);
    out[len] = '\0';
    return 0;
}

static int cli_join(char *out, size_t out_size, const char *left, const char *right)
{
    size_t len;

    if (copy_arg(out, out_size, left, "path") != 0) {
        return -1;
    }

    len = strlen(out);
    if (len > 0 && !cli_is_sep(out[len - 1])) {
        if (len + 2 >= out_size) {
            return -1;
        }
#if defined(_WIN32)
        out[len++] = '\\';
#else
        out[len++] = '/';
#endif
        out[len] = '\0';
    }

    if (len + strlen(right) >= out_size) {
        return -1;
    }
    strcat(out, right);
    return 0;
}

static int cli_file_exists(const char *path)
{
#if defined(_WIN32)
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    return access(path, X_OK) == 0;
#endif
}

static void cli_set_runtime_path(OsProjectConfig *config, const char *self_path)
{
    char dir[OS_MAX_PATH_LEN];
    char runtime[OS_MAX_PATH_LEN];

#if defined(_WIN32)
    const char *runtime_name = "openstaller-runtime.exe";
#else
    const char *runtime_name = "openstaller-runtime";
#endif

    if (cli_dirname(self_path, dir, sizeof(dir)) == 0 &&
        cli_join(runtime, sizeof(runtime), dir, runtime_name) == 0 &&
        cli_file_exists(runtime)) {
        copy_arg(config->runtime_exe, sizeof(config->runtime_exe), runtime, "runtime-exe");
        return;
    }

    copy_arg(config->runtime_exe, sizeof(config->runtime_exe), self_path, "runtime-exe");
}

static int cli_name_equals(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return 0;
        }
        ++left;
        ++right;
    }

    return *left == '\0' && *right == '\0';
}

static int cli_self_path(const char *argv0, char *out, size_t out_size)
{
#if defined(_WIN32)
    DWORD length = GetModuleFileNameA(NULL, out, (DWORD)out_size);
    if (length > 0 && length < out_size) {
        return 0;
    }
#elif defined(__linux__)
    ssize_t length = readlink("/proc/self/exe", out, out_size - 1);
    if (length > 0 && (size_t)length < out_size) {
        out[length] = '\0';
        return 0;
    }
#endif

    return copy_arg(out, out_size, argv0, "argv0");
}

static int parse_installer_style(OsProjectConfig *config, const char *value)
{
    if (cli_name_equals(value, "classic")) {
        config->installer_style = OS_INSTALLER_STYLE_CLASSIC;
        return 0;
    }
    if (cli_name_equals(value, "modern")) {
        config->installer_style = OS_INSTALLER_STYLE_MODERN;
        return 0;
    }
    if (cli_name_equals(value, "legacy")) {
        config->installer_style = OS_INSTALLER_STYLE_LEGACY;
        return 0;
    }
    fprintf(stderr, "Unknown installer style: %s\n", value);
    return -1;
}

static int parse_window_style(OsProjectConfig *config, const char *value)
{
    if (cli_name_equals(value, "fixed")) {
        config->window_style = OS_WINDOW_STYLE_FIXED;
        return 0;
    }
    if (cli_name_equals(value, "resizable")) {
        config->window_style = OS_WINDOW_STYLE_RESIZABLE;
        return 0;
    }
    if (cli_name_equals(value, "maximized") || cli_name_equals(value, "maximise")) {
        config->window_style = OS_WINDOW_STYLE_MAXIMIZED;
        return 0;
    }
    fprintf(stderr, "Unknown window style: %s\n", value);
    return -1;
}

static int parse_pages(OsProjectConfig *config, const char *value)
{
    char copy[256];
    char *token;
    uint32_t flags = 0;

    if (cli_name_equals(value, "full")) {
        config->page_flags = OS_PAGE_DEFAULT;
        return 0;
    }
    if (cli_name_equals(value, "compact")) {
        config->page_flags = OS_PAGE_FOLDER | OS_PAGE_COMPONENTS | OS_PAGE_READY | OS_PAGE_FINISH;
        return 0;
    }
    if (cli_name_equals(value, "minimal")) {
        config->page_flags = OS_PAGE_FOLDER | OS_PAGE_READY | OS_PAGE_FINISH;
        return 0;
    }

    if (copy_arg(copy, sizeof(copy), value, "--pages") != 0) {
        return -1;
    }

    token = strtok(copy, ",");
    while (token != NULL) {
        char *end;
        while (*token == ' ' || *token == '\t') {
            ++token;
        }
        end = token + strlen(token);
        while (end > token && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
            *--end = '\0';
        }
        if (cli_name_equals(token, "welcome")) {
            flags |= OS_PAGE_WELCOME;
        } else if (cli_name_equals(token, "license")) {
            flags |= OS_PAGE_LICENSE;
        } else if (cli_name_equals(token, "folder") || cli_name_equals(token, "location")) {
            flags |= OS_PAGE_FOLDER;
        } else if (cli_name_equals(token, "components")) {
            flags |= OS_PAGE_COMPONENTS;
        } else if (cli_name_equals(token, "ready")) {
            flags |= OS_PAGE_READY;
        } else if (cli_name_equals(token, "finish") || cli_name_equals(token, "complete")) {
            flags |= OS_PAGE_FINISH;
        } else if (token[0] != '\0') {
            fprintf(stderr, "Unknown page name: %s\n", token);
            return -1;
        }
        token = strtok(NULL, ",");
    }

    config->page_flags = flags != 0 ? flags : OS_PAGE_DEFAULT;
    return 0;
}

static int cli_add_online_component(OsProjectConfig *config,
                                    const char *name,
                                    const char *url,
                                    const char *target,
                                    int selected_by_default)
{
    OsOnlineComponent *component;

    if (config->online_component_count >= OS_MAX_ONLINE_COMPONENTS) {
        fprintf(stderr, "Too many online components. The limit is %d.\n", OS_MAX_ONLINE_COMPONENTS);
        return -1;
    }

    component = &config->online_components[config->online_component_count++];
    memset(component, 0, sizeof(*component));
    if (copy_arg(component->name, sizeof(component->name), name, "--online-component name") != 0 ||
        copy_arg(component->url, sizeof(component->url), url, "--online-component url") != 0 ||
        copy_arg(component->target_path, sizeof(component->target_path), target, "--online-component target") != 0) {
        return -1;
    }
    component->selected_by_default = selected_by_default;
    copy_arg(component->page, sizeof(component->page), "components", "--online-page");
    return 0;
}

static int cli_maybe_run_packaged_mode(int argc, char **argv, const char *self_path)
{
    const char *name = cli_basename(self_path);
    char package_dir[OS_MAX_PATH_LEN];
    char message[OS_MAX_MESSAGE_LEN];
    const char *override_dir = argc > 1 ? argv[1] : NULL;
    int status;

    if (cli_name_equals(name, "installer.exe") || cli_name_equals(name, "installer")) {
        status = os_install_embedded_package(self_path, override_dir, message, sizeof(message));
        if (status == 0) {
            puts(message);
            return 0;
        }
        if (cli_dirname(self_path, package_dir, sizeof(package_dir)) != 0) {
            fprintf(stderr, "Cannot resolve package directory.\n");
            return 2;
        }
        status = os_install_package(package_dir, override_dir, message, sizeof(message));
        puts(message);
        return status == 0 ? 0 : 1;
    }

    if (cli_name_equals(name, "uninstaller.exe") || cli_name_equals(name, "uninstaller")) {
        status = os_uninstall_embedded_package(self_path, override_dir, message, sizeof(message));
        if (status == 0) {
            puts(message);
            return 0;
        }
        if (cli_dirname(self_path, package_dir, sizeof(package_dir)) != 0) {
            fprintf(stderr, "Cannot resolve package directory.\n");
            return 2;
        }
        status = os_uninstall_package(package_dir, override_dir, message, sizeof(message));
        puts(message);
        return status == 0 ? 0 : 1;
    }

    return -1;
}

int main(int argc, char **argv)
{
    OsProjectConfig config;
    OsGenerationResult result;
    char self_path[OS_MAX_PATH_LEN];
    int i;
    int packaged_status;

    os_config_init(&config);
    if (cli_self_path(argv[0], self_path, sizeof(self_path)) != 0) {
        fprintf(stderr, "Cannot resolve current executable path.\n");
        return 2;
    }

    packaged_status = cli_maybe_run_packaged_mode(argc, argv, self_path);
    if (packaged_status >= 0) {
        return packaged_status;
    }

    cli_set_runtime_path(&config, self_path);

    if (argc == 1) {
        print_help();
        return 0;
    }

    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const char *value = NULL;

        if (strcmp(arg, "--help") == 0) {
            print_help();
            return 0;
        }

        if (strcmp(arg, "--hash-backend") == 0) {
            printf("%s\n", os_hash_backend_name());
            return 0;
        }

        if (strcmp(arg, "--no-register") == 0) {
            config.register_system = 0;
            continue;
        }

        if (strcmp(arg, "--no-native-exe") == 0) {
            config.generate_native_exe = 0;
            continue;
        }

        if (strcmp(arg, "--run-install") == 0) {
            char message[OS_MAX_MESSAGE_LEN];
            const char *override_dir;
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing package directory for %s.\n", arg);
                return 2;
            }
            value = argv[++i];
            override_dir = (i + 1 < argc) ? argv[i + 1] : NULL;
            if (os_install_package(value, override_dir, message, sizeof(message)) != 0) {
                fprintf(stderr, "%s\n", message);
                return 1;
            }
            puts(message);
            return 0;
        }

        if (strcmp(arg, "--run-uninstall") == 0) {
            char message[OS_MAX_MESSAGE_LEN];
            const char *override_dir;
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing package directory for %s.\n", arg);
                return 2;
            }
            value = argv[++i];
            override_dir = (i + 1 < argc) ? argv[i + 1] : NULL;
            if (os_uninstall_package(value, override_dir, message, sizeof(message)) != 0) {
                fprintf(stderr, "%s\n", message);
                return 1;
            }
            puts(message);
            return 0;
        }

        if (strcmp(arg, "--windows-only") == 0) {
            config.generate_windows = 1;
            config.generate_unix = 0;
            continue;
        }

        if (strcmp(arg, "--unix-only") == 0) {
            config.generate_windows = 0;
            config.generate_unix = 1;
            continue;
        }

        if (strcmp(arg, "--online-component") == 0 || strcmp(arg, "--online-optional") == 0) {
            if (i + 3 >= argc) {
                fprintf(stderr, "Missing NAME URL TARGET for %s.\n", arg);
                return 2;
            }
            if (cli_add_online_component(&config,
                                         argv[i + 1],
                                         argv[i + 2],
                                         argv[i + 3],
                                         strcmp(arg, "--online-component") == 0) != 0) {
                return 2;
            }
            i += 3;
            continue;
        }

        if (strcmp(arg, "--online-description") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing value for %s.\n", arg);
                return 2;
            }
            if (config.online_component_count == 0) {
                fprintf(stderr, "%s must follow an online component.\n", arg);
                return 2;
            }
            if (copy_arg(config.online_components[config.online_component_count - 1].description,
                         sizeof(config.online_components[0].description),
                         argv[++i],
                         arg) != 0) {
                return 2;
            }
            continue;
        }

        if (strcmp(arg, "--online-page") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing value for %s.\n", arg);
                return 2;
            }
            if (config.online_component_count == 0) {
                fprintf(stderr, "%s must follow an online component.\n", arg);
                return 2;
            }
            if (copy_arg(config.online_components[config.online_component_count - 1].page,
                         sizeof(config.online_components[0].page),
                         argv[++i],
                         arg) != 0) {
                return 2;
            }
            continue;
        }

        if (i + 1 >= argc) {
            fprintf(stderr, "Missing value for %s.\n", arg);
            return 2;
        }

        value = argv[++i];
        if (strcmp(arg, "--name") == 0) {
            if (copy_arg(config.app_name, sizeof(config.app_name), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--company") == 0) {
            if (copy_arg(config.company_name, sizeof(config.company_name), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--version") == 0) {
            if (copy_arg(config.app_version, sizeof(config.app_version), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--source") == 0) {
            if (copy_arg(config.source_dir, sizeof(config.source_dir), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--output") == 0) {
            if (copy_arg(config.output_dir, sizeof(config.output_dir), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--install-dir") == 0) {
            if (copy_arg(config.install_dir, sizeof(config.install_dir), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--license") == 0) {
            config.include_license = 1;
            if (copy_arg(config.license_file, sizeof(config.license_file), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--wizard-image") == 0) {
            if (copy_arg(config.wizard_image_file, sizeof(config.wizard_image_file), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--background-image") == 0) {
            if (copy_arg(config.background_image_file, sizeof(config.background_image_file), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--installer-style") == 0) {
            if (parse_installer_style(&config, value) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--ui-font") == 0) {
            if (copy_arg(config.ui_font, sizeof(config.ui_font), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--window-style") == 0) {
            if (parse_window_style(&config, value) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--pages") == 0) {
            if (parse_pages(&config, value) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--theme-accent") == 0) {
            if (copy_arg(config.theme.accent, sizeof(config.theme.accent), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--theme-progress") == 0) {
            if (copy_arg(config.theme.progress, sizeof(config.theme.progress), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--theme-sidebar") == 0) {
            if (copy_arg(config.theme.sidebar, sizeof(config.theme.sidebar), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--theme-sidebar-dark") == 0) {
            if (copy_arg(config.theme.sidebar_dark, sizeof(config.theme.sidebar_dark), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--theme-background") == 0) {
            if (copy_arg(config.theme.background, sizeof(config.theme.background), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--theme-panel") == 0) {
            if (copy_arg(config.theme.panel, sizeof(config.theme.panel), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--theme-text") == 0) {
            if (copy_arg(config.theme.text, sizeof(config.theme.text), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--theme-muted") == 0) {
            if (copy_arg(config.theme.muted_text, sizeof(config.theme.muted_text), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--theme-legacy-top") == 0) {
            if (copy_arg(config.theme.legacy_top, sizeof(config.theme.legacy_top), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--theme-legacy-bottom") == 0) {
            if (copy_arg(config.theme.legacy_bottom, sizeof(config.theme.legacy_bottom), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--installer-icon") == 0) {
            if (copy_arg(config.installer_icon_file, sizeof(config.installer_icon_file), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--uninstaller-icon") == 0) {
            if (copy_arg(config.uninstaller_icon_file, sizeof(config.uninstaller_icon_file), value, arg) != 0) {
                return 2;
            }
        } else if (strcmp(arg, "--launcher") == 0) {
            if (copy_arg(config.launcher, sizeof(config.launcher), value, arg) != 0) {
                return 2;
            }
        } else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return 2;
        }
    }

    if (!config.generate_native_exe && !config.generate_windows && !config.generate_unix) {
        fprintf(stderr, "No package target was enabled.\n");
        return 2;
    }

    if (os_generate_project(&config, &result) != 0) {
        fprintf(stderr, "Openstaller error: %s\n", result.message);
        return 1;
    }

    printf("%s\n", result.message);
    return 0;
}
