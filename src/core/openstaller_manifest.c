#include "openstaller_manifest.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define OS_PATH_SEP '\\'
#else
#define OS_PATH_SEP '/'
#endif

static int manifest_is_sep(char ch)
{
    return ch == '/' || ch == '\\';
}

static void manifest_set_error(char *buffer, size_t size, const char *format, ...)
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

static int manifest_copy(char *dst, size_t dst_size, const char *src)
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

static int manifest_append(char *dst, size_t dst_size, const char *src)
{
    size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);

    if (dst_len + src_len >= dst_size) {
        return -1;
    }

    memcpy(dst + dst_len, src, src_len + 1);
    return 0;
}

static int manifest_join(char *out, size_t out_size, const char *left, const char *right)
{
    size_t len;

    if (manifest_copy(out, out_size, left) != 0) {
        return -1;
    }

    len = strlen(out);
    if (len > 0 && !manifest_is_sep(out[len - 1])) {
        char sep[2] = {OS_PATH_SEP, '\0'};
        if (manifest_append(out, out_size, sep) != 0) {
            return -1;
        }
    }

    return manifest_append(out, out_size, right);
}

static void manifest_trim_line(char *line)
{
    size_t len = strlen(line);

    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ' || line[len - 1] == '\t')) {
        line[--len] = '\0';
    }
}

static int manifest_decode(char *dst, size_t dst_size, const char *src)
{
    size_t written = 0;

    if (dst_size == 0) {
        return -1;
    }

    while (*src != '\0') {
        char ch = *src++;

        if (ch == '\\' && *src != '\0') {
            char escaped = *src++;
            if (escaped == 'n') {
                ch = '\n';
            } else if (escaped == 'r') {
                ch = '\r';
            } else if (escaped == 't') {
                ch = '\t';
            } else {
                ch = escaped;
            }
        }

        if (written + 1 >= dst_size) {
            dst[0] = '\0';
            return -1;
        }
        dst[written++] = ch;
    }

    dst[written] = '\0';
    return 0;
}

static int manifest_set_online(OsPackageManifest *manifest, const char *key, const char *value)
{
    const char *cursor;
    const char *field;
    char *end;
    unsigned long index;
    OsOnlineComponent *component;

    if (strncmp(key, "online.", 7) != 0) {
        return 0;
    }

    cursor = key + 7;
    index = strtoul(cursor, &end, 10);
    if (end == cursor || *end != '.' || index >= OS_MAX_ONLINE_COMPONENTS) {
        return 0;
    }

    field = end + 1;
    component = &manifest->online_components[index];
    if ((size_t)index + 1 > manifest->online_component_count) {
        manifest->online_component_count = (size_t)index + 1;
    }

    if (strcmp(field, "name") == 0) {
        return manifest_decode(component->name, sizeof(component->name), value);
    }
    if (strcmp(field, "description") == 0) {
        return manifest_decode(component->description, sizeof(component->description), value);
    }
    if (strcmp(field, "url") == 0) {
        return manifest_decode(component->url, sizeof(component->url), value);
    }
    if (strcmp(field, "target") == 0) {
        return manifest_decode(component->target_path, sizeof(component->target_path), value);
    }
    if (strcmp(field, "default") == 0) {
        component->selected_by_default = atoi(value) != 0;
        return 0;
    }

    return 0;
}

static int manifest_set_theme(OsPackageManifest *manifest, const char *key, const char *value)
{
    const char *field;

    if (strncmp(key, "theme.", 6) != 0) {
        return 0;
    }

    field = key + 6;
    if (strcmp(field, "accent") == 0) {
        return manifest_decode(manifest->theme.accent, sizeof(manifest->theme.accent), value);
    }
    if (strcmp(field, "progress") == 0) {
        return manifest_decode(manifest->theme.progress, sizeof(manifest->theme.progress), value);
    }
    if (strcmp(field, "sidebar") == 0) {
        return manifest_decode(manifest->theme.sidebar, sizeof(manifest->theme.sidebar), value);
    }
    if (strcmp(field, "sidebar_dark") == 0) {
        return manifest_decode(manifest->theme.sidebar_dark, sizeof(manifest->theme.sidebar_dark), value);
    }
    if (strcmp(field, "background") == 0) {
        return manifest_decode(manifest->theme.background, sizeof(manifest->theme.background), value);
    }
    if (strcmp(field, "panel") == 0) {
        return manifest_decode(manifest->theme.panel, sizeof(manifest->theme.panel), value);
    }
    if (strcmp(field, "text") == 0) {
        return manifest_decode(manifest->theme.text, sizeof(manifest->theme.text), value);
    }
    if (strcmp(field, "muted_text") == 0 || strcmp(field, "muted") == 0) {
        return manifest_decode(manifest->theme.muted_text, sizeof(manifest->theme.muted_text), value);
    }
    if (strcmp(field, "legacy_top") == 0) {
        return manifest_decode(manifest->theme.legacy_top, sizeof(manifest->theme.legacy_top), value);
    }
    if (strcmp(field, "legacy_bottom") == 0) {
        return manifest_decode(manifest->theme.legacy_bottom, sizeof(manifest->theme.legacy_bottom), value);
    }

    return 0;
}

void os_manifest_write_value(FILE *file, const char *key, const char *value)
{
    fputs(key, file);
    fputc('=', file);

    while (*value != '\0') {
        if (*value == '\\') {
            fputs("\\\\", file);
        } else if (*value == '\n') {
            fputs("\\n", file);
        } else if (*value == '\r') {
            fputs("\\r", file);
        } else if (*value == '\t') {
            fputs("\\t", file);
        } else {
            fputc(*value, file);
        }
        ++value;
    }

    fputc('\n', file);
}

static int manifest_set(OsPackageManifest *manifest, const char *key, const char *value)
{
    if (strcmp(key, "name") == 0) {
        return manifest_decode(manifest->app_name, sizeof(manifest->app_name), value);
    }
    if (strcmp(key, "company") == 0) {
        return manifest_decode(manifest->company_name, sizeof(manifest->company_name), value);
    }
    if (strcmp(key, "safe_name") == 0) {
        return manifest_decode(manifest->safe_name, sizeof(manifest->safe_name), value);
    }
    if (strcmp(key, "version") == 0) {
        return manifest_decode(manifest->app_version, sizeof(manifest->app_version), value);
    }
    if (strcmp(key, "install_dir") == 0) {
        return manifest_decode(manifest->install_dir, sizeof(manifest->install_dir), value);
    }
    if (strcmp(key, "ui_font") == 0) {
        return manifest_decode(manifest->ui_font, sizeof(manifest->ui_font), value);
    }
    if (strcmp(key, "launcher") == 0) {
        return manifest_decode(manifest->launcher, sizeof(manifest->launcher), value);
    }
    if (strcmp(key, "installer_id") == 0) {
        return manifest_decode(manifest->installer_id, sizeof(manifest->installer_id), value);
    }
    if (strcmp(key, "file_count") == 0) {
        manifest->file_count = (size_t)strtoull(value, NULL, 10);
        return 0;
    }
    if (strcmp(key, "online_count") == 0) {
        manifest->online_component_count = (size_t)strtoull(value, NULL, 10);
        if (manifest->online_component_count > OS_MAX_ONLINE_COMPONENTS) {
            manifest->online_component_count = OS_MAX_ONLINE_COMPONENTS;
        }
        return 0;
    }
    if (strcmp(key, "page_flags") == 0) {
        manifest->page_flags = (uint32_t)strtoul(value, NULL, 0);
        return 0;
    }
    if (manifest_set_online(manifest, key, value) != 0) {
        return -1;
    }
    if (manifest_set_theme(manifest, key, value) != 0) {
        return -1;
    }
    if (strcmp(key, "installer_style") == 0) {
        if (strcmp(value, "modern") == 0) {
            manifest->installer_style = OS_INSTALLER_STYLE_MODERN;
        } else if (strcmp(value, "legacy") == 0) {
            manifest->installer_style = OS_INSTALLER_STYLE_LEGACY;
        } else {
            manifest->installer_style = OS_INSTALLER_STYLE_CLASSIC;
        }
        return 0;
    }
    if (strcmp(key, "window_style") == 0) {
        if (strcmp(value, "resizable") == 0) {
            manifest->window_style = OS_WINDOW_STYLE_RESIZABLE;
        } else if (strcmp(value, "maximized") == 0) {
            manifest->window_style = OS_WINDOW_STYLE_MAXIMIZED;
        } else {
            manifest->window_style = OS_WINDOW_STYLE_FIXED;
        }
        return 0;
    }
    if (strcmp(key, "welcome_title") == 0) {
        return manifest_decode(manifest->welcome_title, sizeof(manifest->welcome_title), value);
    }
    if (strcmp(key, "welcome_text") == 0) {
        return manifest_decode(manifest->welcome_text, sizeof(manifest->welcome_text), value);
    }
    if (strcmp(key, "folder_title") == 0) {
        return manifest_decode(manifest->folder_title, sizeof(manifest->folder_title), value);
    }
    if (strcmp(key, "folder_text") == 0) {
        return manifest_decode(manifest->folder_text, sizeof(manifest->folder_text), value);
    }
    if (strcmp(key, "components_title") == 0) {
        return manifest_decode(manifest->components_title, sizeof(manifest->components_title), value);
    }
    if (strcmp(key, "components_text") == 0) {
        return manifest_decode(manifest->components_text, sizeof(manifest->components_text), value);
    }
    if (strcmp(key, "ready_title") == 0) {
        return manifest_decode(manifest->ready_title, sizeof(manifest->ready_title), value);
    }
    if (strcmp(key, "ready_text") == 0) {
        return manifest_decode(manifest->ready_text, sizeof(manifest->ready_text), value);
    }
    if (strcmp(key, "finish_title") == 0) {
        return manifest_decode(manifest->finish_title, sizeof(manifest->finish_title), value);
    }
    if (strcmp(key, "finish_text") == 0) {
        return manifest_decode(manifest->finish_text, sizeof(manifest->finish_text), value);
    }
    if (strcmp(key, "uninstall_title") == 0) {
        return manifest_decode(manifest->uninstall_title, sizeof(manifest->uninstall_title), value);
    }
    if (strcmp(key, "uninstall_text") == 0) {
        return manifest_decode(manifest->uninstall_text, sizeof(manifest->uninstall_text), value);
    }
    if (strcmp(key, "register_system") == 0) {
        manifest->register_system = atoi(value) != 0;
    }

    return 0;
}

void os_manifest_init_defaults(OsPackageManifest *manifest)
{
    memset(manifest, 0, sizeof(*manifest));
    manifest->register_system = 1;
    manifest->installer_style = OS_INSTALLER_STYLE_CLASSIC;
    manifest->window_style = OS_WINDOW_STYLE_FIXED;
    manifest->page_flags = OS_PAGE_DEFAULT;
    manifest_copy(manifest->theme.accent, sizeof(manifest->theme.accent), "#0078D4");
    manifest_copy(manifest->theme.progress, sizeof(manifest->theme.progress), "#0078D4");
    manifest_copy(manifest->theme.sidebar, sizeof(manifest->theme.sidebar), "#154E9E");
    manifest_copy(manifest->theme.sidebar_dark, sizeof(manifest->theme.sidebar_dark), "#082766");
    manifest_copy(manifest->theme.background, sizeof(manifest->theme.background), "#F6F8FB");
    manifest_copy(manifest->theme.panel, sizeof(manifest->theme.panel), "#FFFFFF");
    manifest_copy(manifest->theme.text, sizeof(manifest->theme.text), "#000000");
    manifest_copy(manifest->theme.muted_text, sizeof(manifest->theme.muted_text), "#57606A");
    manifest_copy(manifest->theme.legacy_top, sizeof(manifest->theme.legacy_top), "#0012E8");
    manifest_copy(manifest->theme.legacy_bottom, sizeof(manifest->theme.legacy_bottom), "#000012");
    manifest_copy(manifest->welcome_title, sizeof(manifest->welcome_title), "Welcome to the Setup Wizard");
    manifest_copy(manifest->welcome_text,
                  sizeof(manifest->welcome_text),
                  "This wizard will guide you through the installation. Click Next to continue.");
    manifest_copy(manifest->folder_title, sizeof(manifest->folder_title), "Choose Install Location");
    manifest_copy(manifest->folder_text,
                  sizeof(manifest->folder_text),
                  "Choose the folder where the application will be installed.");
    manifest_copy(manifest->components_title, sizeof(manifest->components_title), "Choose Components");
    manifest_copy(manifest->components_text,
                  sizeof(manifest->components_text),
                  "Select the components you want to install.");
    manifest_copy(manifest->ready_title, sizeof(manifest->ready_title), "Ready to Install");
    manifest_copy(manifest->ready_text,
                  sizeof(manifest->ready_text),
                  "Setup is ready to begin installing the application.");
    manifest_copy(manifest->finish_title, sizeof(manifest->finish_title), "Installation Complete");
    manifest_copy(manifest->finish_text,
                  sizeof(manifest->finish_text),
                  "The application has been installed successfully.");
    manifest_copy(manifest->uninstall_title, sizeof(manifest->uninstall_title), "Uninstall Application");
    manifest_copy(manifest->uninstall_text,
                  sizeof(manifest->uninstall_text),
                  "This wizard will remove the application from your computer.");
}

static int manifest_validate(const OsPackageManifest *manifest, char *error, size_t error_size)
{
    if (manifest->app_name[0] == '\0' ||
        manifest->safe_name[0] == '\0' ||
        manifest->install_dir[0] == '\0' ||
        manifest->installer_id[0] == '\0') {
        manifest_set_error(error, error_size, "Package manifest is incomplete.");
        return -1;
    }

    return 0;
}

int os_read_manifest_buffer(const char *data,
                            size_t data_size,
                            OsPackageManifest *manifest,
                            char *error,
                            size_t error_size)
{
    char *copy;
    char *line;

    os_manifest_init_defaults(manifest);
    copy = (char *)malloc(data_size + 1);
    if (copy == NULL) {
        manifest_set_error(error, error_size, "Not enough memory to read package manifest.");
        return -1;
    }

    memcpy(copy, data, data_size);
    copy[data_size] = '\0';

    line = strtok(copy, "\n");
    while (line != NULL) {
        char *equals;

        manifest_trim_line(line);
        equals = strchr(line, '=');
        if (equals != NULL && equals != line) {
            *equals = '\0';
            if (manifest_set(manifest, line, equals + 1) != 0) {
                free(copy);
                manifest_set_error(error, error_size, "Manifest value is too long: %s", line);
                return -1;
            }
        }

        line = strtok(NULL, "\n");
    }

    free(copy);
    return manifest_validate(manifest, error, error_size);
}

int os_read_package_manifest(const char *package_dir,
                             OsPackageManifest *manifest,
                             char *error,
                             size_t error_size)
{
    char manifest_path[OS_MAX_PATH_LEN];
    char line[OS_MAX_URL_LEN + OS_MAX_PATH_LEN];
    FILE *file;

    os_manifest_init_defaults(manifest);

    if (manifest_join(manifest_path, sizeof(manifest_path), package_dir, "manifest.openstaller") != 0) {
        manifest_set_error(error, error_size, "Package path is too long.");
        return -1;
    }

    file = fopen(manifest_path, "rb");
    if (file == NULL) {
        manifest_set_error(error, error_size, "Cannot open package manifest: %s", manifest_path);
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char *equals;

        manifest_trim_line(line);
        equals = strchr(line, '=');
        if (equals == NULL || equals == line) {
            continue;
        }

        *equals = '\0';
        if (manifest_set(manifest, line, equals + 1) != 0) {
            fclose(file);
            manifest_set_error(error, error_size, "Manifest value is too long: %s", line);
            return -1;
        }
    }

    fclose(file);
    return manifest_validate(manifest, error, error_size);
}

int os_package_key(char *out, size_t out_size, const OsPackageManifest *manifest)
{
    if (manifest_copy(out, out_size, manifest->safe_name) != 0 ||
        manifest_append(out, out_size, "-") != 0 ||
        manifest_append(out, out_size, manifest->installer_id) != 0) {
        return -1;
    }
    return 0;
}

void os_manifest_to_info(const OsPackageManifest *manifest, OsPackageInfo *info)
{
    memset(info, 0, sizeof(*info));
    manifest_copy(info->app_name, sizeof(info->app_name), manifest->app_name);
    manifest_copy(info->company_name, sizeof(info->company_name), manifest->company_name);
    manifest_copy(info->app_version, sizeof(info->app_version), manifest->app_version);
    manifest_copy(info->install_dir, sizeof(info->install_dir), manifest->install_dir);
    manifest_copy(info->ui_font, sizeof(info->ui_font), manifest->ui_font);
    manifest_copy(info->welcome_title, sizeof(info->welcome_title), manifest->welcome_title);
    manifest_copy(info->welcome_text, sizeof(info->welcome_text), manifest->welcome_text);
    manifest_copy(info->folder_title, sizeof(info->folder_title), manifest->folder_title);
    manifest_copy(info->folder_text, sizeof(info->folder_text), manifest->folder_text);
    manifest_copy(info->components_title, sizeof(info->components_title), manifest->components_title);
    manifest_copy(info->components_text, sizeof(info->components_text), manifest->components_text);
    manifest_copy(info->ready_title, sizeof(info->ready_title), manifest->ready_title);
    manifest_copy(info->ready_text, sizeof(info->ready_text), manifest->ready_text);
    manifest_copy(info->finish_title, sizeof(info->finish_title), manifest->finish_title);
    manifest_copy(info->finish_text, sizeof(info->finish_text), manifest->finish_text);
    manifest_copy(info->uninstall_title, sizeof(info->uninstall_title), manifest->uninstall_title);
    manifest_copy(info->uninstall_text, sizeof(info->uninstall_text), manifest->uninstall_text);
    info->register_system = manifest->register_system;
    info->has_license = manifest->has_license;
    info->has_wizard_image = manifest->has_wizard_image;
    info->has_background_image = manifest->has_background_image;
    info->installer_style = manifest->installer_style;
    info->window_style = manifest->window_style;
    info->page_flags = manifest->page_flags;
    info->theme = manifest->theme;
    info->file_count = manifest->file_count;
    info->online_component_count = manifest->online_component_count;
    if (info->online_component_count > OS_MAX_ONLINE_COMPONENTS) {
        info->online_component_count = OS_MAX_ONLINE_COMPONENTS;
    }
    memcpy(info->online_components,
           manifest->online_components,
           info->online_component_count * sizeof(info->online_components[0]));
}
