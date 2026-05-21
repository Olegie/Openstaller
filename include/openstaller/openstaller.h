#ifndef OPENSTALLER_OPENSTALLER_H
#define OPENSTALLER_OPENSTALLER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OS_MAX_PATH_LEN 1024
#define OS_MAX_URL_LEN 2048
#define OS_MAX_NAME_LEN 128
#define OS_MAX_VERSION_LEN 64
#define OS_MAX_MESSAGE_LEN 512
#define OS_MAX_PAGE_TEXT_LEN 512
#define OS_MAX_ONLINE_COMPONENTS 16
#define OS_COLOR_TEXT_LEN 16

#define OS_PAGE_WELCOME 0x00000001u
#define OS_PAGE_LICENSE 0x00000002u
#define OS_PAGE_FOLDER 0x00000004u
#define OS_PAGE_COMPONENTS 0x00000008u
#define OS_PAGE_READY 0x00000010u
#define OS_PAGE_FINISH 0x00000020u
#define OS_PAGE_DEFAULT \
    (OS_PAGE_WELCOME | OS_PAGE_LICENSE | OS_PAGE_FOLDER | OS_PAGE_COMPONENTS | OS_PAGE_READY | OS_PAGE_FINISH)

typedef enum OsInstallerStyle {
    OS_INSTALLER_STYLE_CLASSIC = 0,
    OS_INSTALLER_STYLE_MODERN = 1,
    OS_INSTALLER_STYLE_LEGACY = 2
} OsInstallerStyle;

typedef enum OsWindowStyle {
    OS_WINDOW_STYLE_FIXED = 0,
    OS_WINDOW_STYLE_RESIZABLE = 1,
    OS_WINDOW_STYLE_MAXIMIZED = 2
} OsWindowStyle;

typedef struct OsOnlineComponent {
    char name[OS_MAX_NAME_LEN];
    char description[OS_MAX_PAGE_TEXT_LEN];
    char url[OS_MAX_URL_LEN];
    char target_path[OS_MAX_PATH_LEN];
    int selected_by_default;
} OsOnlineComponent;

typedef struct OsInstallerTheme {
    char accent[OS_COLOR_TEXT_LEN];
    char progress[OS_COLOR_TEXT_LEN];
    char sidebar[OS_COLOR_TEXT_LEN];
    char sidebar_dark[OS_COLOR_TEXT_LEN];
    char background[OS_COLOR_TEXT_LEN];
    char panel[OS_COLOR_TEXT_LEN];
    char text[OS_COLOR_TEXT_LEN];
    char muted_text[OS_COLOR_TEXT_LEN];
    char legacy_top[OS_COLOR_TEXT_LEN];
    char legacy_bottom[OS_COLOR_TEXT_LEN];
} OsInstallerTheme;

typedef struct OsProjectConfig {
    char app_name[OS_MAX_NAME_LEN];
    char company_name[OS_MAX_NAME_LEN];
    char app_version[OS_MAX_VERSION_LEN];
    char source_dir[OS_MAX_PATH_LEN];
    char output_dir[OS_MAX_PATH_LEN];
    char install_dir[OS_MAX_PATH_LEN];
    char license_file[OS_MAX_PATH_LEN];
    char ui_font[OS_MAX_NAME_LEN];
    char wizard_image_file[OS_MAX_PATH_LEN];
    char background_image_file[OS_MAX_PATH_LEN];
    char installer_icon_file[OS_MAX_PATH_LEN];
    char uninstaller_icon_file[OS_MAX_PATH_LEN];
    char launcher[OS_MAX_PATH_LEN];
    char runtime_exe[OS_MAX_PATH_LEN];
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
    int include_license;
    int register_system;
    int generate_native_exe;
    int generate_windows;
    int generate_unix;
    int installer_style;
    int window_style;
    uint32_t page_flags;
    OsInstallerTheme theme;
    size_t online_component_count;
    OsOnlineComponent online_components[OS_MAX_ONLINE_COMPONENTS];
} OsProjectConfig;

typedef struct OsGenerationResult {
    char message[OS_MAX_MESSAGE_LEN];
    char package_dir[OS_MAX_PATH_LEN];
    uint64_t installer_id;
    uint64_t payload_hash;
    size_t file_count;
    int used_assembly;
} OsGenerationResult;

typedef struct OsPackageInfo {
    char app_name[OS_MAX_NAME_LEN];
    char company_name[OS_MAX_NAME_LEN];
    char app_version[OS_MAX_VERSION_LEN];
    char install_dir[OS_MAX_PATH_LEN];
    char ui_font[OS_MAX_NAME_LEN];
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
} OsPackageInfo;

typedef enum OsInstallProgressStage {
    OS_INSTALL_PROGRESS_PREPARE = 1,
    OS_INSTALL_PROGRESS_FILE = 2,
    OS_INSTALL_PROGRESS_REGISTER = 3,
    OS_INSTALL_PROGRESS_UNREGISTER = 4,
    OS_INSTALL_PROGRESS_ROLLBACK = 5,
    OS_INSTALL_PROGRESS_COMPLETE = 6
} OsInstallProgressStage;

typedef struct OsInstallProgressEvent {
    OsInstallProgressStage stage;
    const char *action;
    const char *source_path;
    const char *target_path;
    size_t completed_files;
    size_t total_files;
    int percent;
} OsInstallProgressEvent;

typedef void (*OsInstallProgressCallback)(const OsInstallProgressEvent *event, void *user_data);

void os_config_init(OsProjectConfig *config);
int os_generate_project(const OsProjectConfig *config, OsGenerationResult *result);
int os_read_package_info(const char *package_dir,
                         OsPackageInfo *info,
                         char *message,
                         size_t message_size);
int os_read_embedded_package_info(const char *exe_path,
                                  OsPackageInfo *info,
                                  char *message,
                                  size_t message_size);
int os_extract_embedded_license(const char *exe_path,
                                const char *dst_path,
                                char *message,
                                size_t message_size);
int os_extract_embedded_wizard_image(const char *exe_path,
                                     const char *dst_path,
                                     char *message,
                                     size_t message_size);
int os_extract_embedded_background_image(const char *exe_path,
                                         const char *dst_path,
                                         char *message,
                                         size_t message_size);
int os_install_package(const char *package_dir,
                       const char *override_install_dir,
                       char *message,
                       size_t message_size);
int os_install_package_with_options(const char *package_dir,
                                    const char *override_install_dir,
                                    uint64_t online_component_mask,
                                    char *message,
                                    size_t message_size);
int os_uninstall_package(const char *package_dir,
                         const char *override_install_dir,
                         char *message,
                         size_t message_size);
int os_install_embedded_package(const char *exe_path,
                                const char *override_install_dir,
                                char *message,
                                size_t message_size);
int os_install_embedded_package_with_options(const char *exe_path,
                                             const char *override_install_dir,
                                             uint64_t online_component_mask,
                                             char *message,
                                             size_t message_size);
int os_uninstall_embedded_package(const char *exe_path,
                                  const char *override_install_dir,
                                  char *message,
                                  size_t message_size);
void os_set_install_progress_callback(OsInstallProgressCallback callback, void *user_data);

uint64_t os_hash_bytes(const void *data, size_t size, uint64_t seed);
int os_hash_uses_assembly(void);
const char *os_hash_backend_name(void);

#ifdef __cplusplus
}
#endif

#endif
