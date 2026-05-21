#ifndef OPENSTALLER_MANIFEST_H
#define OPENSTALLER_MANIFEST_H

#include "openstaller/openstaller.h"

#include <stddef.h>
#include <stdio.h>

typedef struct OsPackageManifest {
    char app_name[OS_MAX_NAME_LEN];
    char company_name[OS_MAX_NAME_LEN];
    char app_version[OS_MAX_VERSION_LEN];
    char safe_name[OS_MAX_NAME_LEN];
    char install_dir[OS_MAX_PATH_LEN];
    char ui_font[OS_MAX_NAME_LEN];
    char launcher[OS_MAX_PATH_LEN];
    char installer_id[32];
    char welcome_title[OS_MAX_PAGE_TEXT_LEN];
    char welcome_text[OS_MAX_PAGE_TEXT_LEN];
    char folder_title[OS_MAX_PAGE_TEXT_LEN];
    char folder_text[OS_MAX_PAGE_TEXT_LEN];
    char components_title[OS_MAX_PAGE_TEXT_LEN];
    char components_text[OS_MAX_PAGE_TEXT_LEN];
    char ready_title[OS_MAX_PAGE_TEXT_LEN];
    char ready_text[OS_MAX_PAGE_TEXT_LEN];
    char finish_title[OS_MAX_PAGE_TEXT_LEN];
    char finish_text[OS_MAX_PAGE_TEXT_LEN];
    char uninstall_title[OS_MAX_PAGE_TEXT_LEN];
    char uninstall_text[OS_MAX_PAGE_TEXT_LEN];
    int register_system;
    int has_license;
    int has_wizard_image;
    int has_background_image;
    int installer_style;
    int window_style;
    uint32_t page_flags;
    OsInstallerTheme theme;
    size_t file_count;
    size_t online_component_count;
    OsOnlineComponent online_components[OS_MAX_ONLINE_COMPONENTS];
} OsPackageManifest;

void os_manifest_write_value(FILE *file, const char *key, const char *value);
void os_manifest_init_defaults(OsPackageManifest *manifest);
int os_read_manifest_buffer(const char *data,
                            size_t data_size,
                            OsPackageManifest *manifest,
                            char *error,
                            size_t error_size);
int os_read_package_manifest(const char *package_dir,
                             OsPackageManifest *manifest,
                             char *error,
                             size_t error_size);
int os_package_key(char *out, size_t out_size, const OsPackageManifest *manifest);
void os_manifest_to_info(const OsPackageManifest *manifest, OsPackageInfo *info);

#endif
