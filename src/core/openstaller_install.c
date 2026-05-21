#include "openstaller/openstaller.h"
#include "openstaller_install_internal.h"
#include "openstaller_manifest.h"
#include "openstaller_online.h"
#include "openstaller_pack.h"

#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static OsInstallProgressCallback g_install_progress_callback;
static void *g_install_progress_user_data;

void os_set_install_progress_callback(OsInstallProgressCallback callback, void *user_data)
{
    g_install_progress_callback = callback;
    g_install_progress_user_data = user_data;
}

int install_progress_percent(size_t completed, size_t total, int start, int end)
{
    int span = end - start;

    if (total == 0 || span <= 0) {
        return start;
    }

    if (completed > total) {
        completed = total;
    }

    return start + (int)((completed * (size_t)span) / total);
}

void install_report_progress(OsInstallProgressStage stage,
                             const char *action,
                             const char *source_path,
                             const char *target_path,
                             size_t completed_files,
                             size_t total_files,
                             int percent)
{
    OsInstallProgressEvent event;

    if (g_install_progress_callback == NULL) {
        return;
    }

    memset(&event, 0, sizeof(event));
    event.stage = stage;
    event.action = action;
    event.source_path = source_path;
    event.target_path = target_path;
    event.completed_files = completed_files;
    event.total_files = total_files;
    event.percent = percent;
    g_install_progress_callback(&event, g_install_progress_user_data);
}

static void install_rollback_fresh_dir(const char *install_dir, int install_dir_existed)
{
    char rollback_error[OS_MAX_MESSAGE_LEN];

    if (install_dir_existed) {
        return;
    }

    rollback_error[0] = '\0';
    (void)install_remove_tree(install_dir, rollback_error, sizeof(rollback_error));
}

int os_read_package_info(const char *package_dir, OsPackageInfo *info, char *message, size_t message_size)
{
    OsPackageManifest manifest;
    char error[OS_MAX_MESSAGE_LEN];

    if (info == NULL) {
        install_set_error(message, message_size, "Package info output is required.");
        return -1;
    }

    if (os_read_package_manifest(package_dir, &manifest, error, sizeof(error)) != 0) {
        install_copy(message, message_size, error);
        return -1;
    }

    os_manifest_to_info(&manifest, info);
    install_set_error(message, message_size, "Loaded package information.");
    return 0;
}

int os_read_embedded_package_info(const char *exe_path,
                                  OsPackageInfo *info,
                                  char *message,
                                  size_t message_size)
{
    if (exe_path == NULL || exe_path[0] == '\0') {
        install_set_error(message, message_size, "Native EXE path is required.");
        return -1;
    }
    return os_pack_read_info(exe_path, info, message, message_size);
}

int os_extract_embedded_license(const char *exe_path,
                                const char *dst_path,
                                char *message,
                                size_t message_size)
{
    return os_pack_extract_license(exe_path, dst_path, message, message_size);
}

int os_extract_embedded_wizard_image(const char *exe_path,
                                     const char *dst_path,
                                     char *message,
                                     size_t message_size)
{
    return os_pack_extract_wizard_image(exe_path, dst_path, message, message_size);
}

int os_extract_embedded_background_image(const char *exe_path,
                                         const char *dst_path,
                                         char *message,
                                         size_t message_size)
{
    return os_pack_extract_background_image(exe_path, dst_path, message, message_size);
}

int os_install_package(const char *package_dir,
                       const char *override_install_dir,
                       char *message,
                       size_t message_size)
{
    OsPackageManifest manifest;
    char error[OS_MAX_MESSAGE_LEN];

    if (package_dir != NULL &&
        package_dir[0] != '\0' &&
        os_read_package_manifest(package_dir, &manifest, error, sizeof(error)) == 0) {
        return os_install_package_with_options(package_dir,
                                               override_install_dir,
                                               os_online_default_mask(&manifest),
                                               message,
                                               message_size);
    }

    return os_install_package_with_options(package_dir, override_install_dir, 0, message, message_size);
}

int os_install_package_with_options(const char *package_dir,
                                    const char *override_install_dir,
                                    uint64_t online_component_mask,
                                    char *message,
                                    size_t message_size)
{
    OsPackageManifest manifest;
    char error[OS_MAX_MESSAGE_LEN];
    char payload_dir[OS_MAX_PATH_LEN];
    char install_dir[OS_MAX_PATH_LEN];
    const char *raw_install_dir;
    int install_dir_existed;
    OsRollback rollback;
    OsInstallProgressCounter progress;

    if (message != NULL && message_size > 0) {
        message[0] = '\0';
    }
    error[0] = '\0';

    if (package_dir == NULL || package_dir[0] == '\0') {
        install_set_error(message, message_size, "Package directory is required.");
        return -1;
    }

    if (os_read_package_manifest(package_dir, &manifest, error, sizeof(error)) != 0) {
        install_copy(message, message_size, error);
        return -1;
    }

    raw_install_dir = (override_install_dir != NULL && override_install_dir[0] != '\0')
                          ? override_install_dir
                          : manifest.install_dir;
    if (install_expand_dir(raw_install_dir, install_dir, sizeof(install_dir)) != 0 ||
        install_delete_target_is_dangerous(install_dir)) {
        install_set_error(message, message_size, "Install directory is invalid.");
        return -1;
    }

    if (install_join(payload_dir, sizeof(payload_dir), package_dir, "payload") != 0) {
        install_set_error(message, message_size, "Payload path is too long.");
        return -1;
    }

    install_dir_existed = install_path_exists(install_dir);
    if (os_rollback_begin(&rollback, error, sizeof(error)) != 0) {
        install_copy(message, message_size, error);
        return -1;
    }

    progress.completed_files = 0;
    progress.total_files = manifest.file_count + os_online_selected_count(&manifest, online_component_mask);
    install_report_progress(OS_INSTALL_PROGRESS_PREPARE,
                            "Preparing installation plan",
                            payload_dir,
                            install_dir,
                            0,
                            progress.total_files,
                            8);

    if (install_copy_tree_with_rollback(payload_dir, install_dir, &rollback, &progress, error, sizeof(error)) != 0) {
        char primary_error[OS_MAX_MESSAGE_LEN];

        install_copy(primary_error, sizeof(primary_error), error);
        install_report_progress(OS_INSTALL_PROGRESS_ROLLBACK,
                                "Rolling back file changes",
                                NULL,
                                install_dir,
                                progress.completed_files,
                                progress.total_files,
                                90);
        os_rollback_revert(&rollback, error, sizeof(error));
        install_rollback_fresh_dir(install_dir, install_dir_existed);
        install_copy(message, message_size, primary_error);
        return -1;
    }

    if (os_online_install_components(&manifest,
                                     install_dir,
                                     online_component_mask,
                                     &rollback,
                                     &progress,
                                     error,
                                     sizeof(error)) != 0) {
        char primary_error[OS_MAX_MESSAGE_LEN];

        install_copy(primary_error, sizeof(primary_error), error);
        install_report_progress(OS_INSTALL_PROGRESS_ROLLBACK,
                                "Rolling back online component failure",
                                NULL,
                                install_dir,
                                progress.completed_files,
                                progress.total_files,
                                90);
        os_rollback_revert(&rollback, error, sizeof(error));
        install_rollback_fresh_dir(install_dir, install_dir_existed);
        install_copy(message, message_size, primary_error);
        return -1;
    }

    install_report_progress(OS_INSTALL_PROGRESS_REGISTER,
                            "Writing uninstall registration",
                            package_dir,
                            install_dir,
                            progress.completed_files,
                            progress.total_files,
                            88);

    if (install_register_package(package_dir, &manifest, install_dir, error, sizeof(error)) != 0) {
        char primary_error[OS_MAX_MESSAGE_LEN];

        install_copy(primary_error, sizeof(primary_error), error);
        install_report_progress(OS_INSTALL_PROGRESS_ROLLBACK,
                                "Rolling back registration failure",
                                NULL,
                                install_dir,
                                progress.completed_files,
                                progress.total_files,
                                92);
        os_rollback_revert(&rollback, error, sizeof(error));
        install_rollback_fresh_dir(install_dir, install_dir_existed);
        install_copy(message, message_size, primary_error);
        return -1;
    }

    os_rollback_commit(&rollback);
    install_report_progress(OS_INSTALL_PROGRESS_COMPLETE,
                            "Installation complete",
                            NULL,
                            install_dir,
                            progress.completed_files,
                            progress.total_files,
                            100);
    install_set_error(message,
                      message_size,
                      "Installed %s to %s and registered native uninstaller metadata.",
                      manifest.app_name,
                      install_dir);
    return 0;
}

int os_uninstall_package(const char *package_dir,
                         const char *override_install_dir,
                         char *message,
                         size_t message_size)
{
    OsPackageManifest manifest;
    char error[OS_MAX_MESSAGE_LEN];
    char install_dir[OS_MAX_PATH_LEN];
    const char *raw_install_dir;

    if (message != NULL && message_size > 0) {
        message[0] = '\0';
    }
    error[0] = '\0';

    if (package_dir == NULL || package_dir[0] == '\0') {
        install_set_error(message, message_size, "Package directory is required.");
        return -1;
    }

    if (os_read_package_manifest(package_dir, &manifest, error, sizeof(error)) != 0) {
        install_copy(message, message_size, error);
        return -1;
    }

    raw_install_dir = (override_install_dir != NULL && override_install_dir[0] != '\0')
                          ? override_install_dir
                          : manifest.install_dir;
    if (install_expand_dir(raw_install_dir, install_dir, sizeof(install_dir)) != 0 ||
        install_delete_target_is_dangerous(install_dir)) {
        install_set_error(message, message_size, "Install directory is invalid.");
        return -1;
    }

    install_report_progress(OS_INSTALL_PROGRESS_PREPARE,
                            "Preparing removal plan",
                            NULL,
                            install_dir,
                            0,
                            manifest.file_count,
                            10);

    if (install_remove_tree(install_dir, error, sizeof(error)) != 0) {
        install_copy(message, message_size, error);
        return -1;
    }

    install_report_progress(OS_INSTALL_PROGRESS_UNREGISTER,
                            "Removing uninstall registration",
                            NULL,
                            install_dir,
                            manifest.file_count,
                            manifest.file_count,
                            88);

    if (install_unregister_package(&manifest, error, sizeof(error)) != 0) {
        install_copy(message, message_size, error);
        return -1;
    }

    install_report_progress(OS_INSTALL_PROGRESS_COMPLETE,
                            "Removal complete",
                            NULL,
                            install_dir,
                            manifest.file_count,
                            manifest.file_count,
                            100);
    install_set_error(message,
                      message_size,
                      "Uninstalled %s from %s and removed native uninstaller metadata.",
                      manifest.app_name,
                      install_dir);
    return 0;
}

int os_install_embedded_package(const char *exe_path,
                                const char *override_install_dir,
                                char *message,
                                size_t message_size)
{
    OsPackageManifest manifest;
    char error[OS_MAX_MESSAGE_LEN];

    if (exe_path != NULL &&
        exe_path[0] != '\0' &&
        os_pack_read_manifest(exe_path, &manifest, error, sizeof(error)) == 0) {
        return os_install_embedded_package_with_options(exe_path,
                                                        override_install_dir,
                                                        os_online_default_mask(&manifest),
                                                        message,
                                                        message_size);
    }

    return os_install_embedded_package_with_options(exe_path, override_install_dir, 0, message, message_size);
}

int os_install_embedded_package_with_options(const char *exe_path,
                                             const char *override_install_dir,
                                             uint64_t online_component_mask,
                                             char *message,
                                             size_t message_size)
{
    OsPackageManifest manifest;
    char error[OS_MAX_MESSAGE_LEN];
    char install_dir[OS_MAX_PATH_LEN];
    char uninstaller_path[OS_MAX_PATH_LEN];
    const char *raw_install_dir;
    int install_dir_existed;
    OsRollback rollback;
    OsInstallProgressCounter progress;

    if (message != NULL && message_size > 0) {
        message[0] = '\0';
    }
    error[0] = '\0';

    if (exe_path == NULL || exe_path[0] == '\0') {
        install_set_error(message, message_size, "Native installer EXE path is required.");
        return -1;
    }

    if (os_pack_read_manifest(exe_path, &manifest, error, sizeof(error)) != 0) {
        install_copy(message, message_size, error);
        return -1;
    }

    raw_install_dir = (override_install_dir != NULL && override_install_dir[0] != '\0')
                          ? override_install_dir
                          : manifest.install_dir;
    if (install_expand_dir(raw_install_dir, install_dir, sizeof(install_dir)) != 0 ||
        install_delete_target_is_dangerous(install_dir)) {
        install_set_error(message, message_size, "Install directory is invalid.");
        return -1;
    }

    install_dir_existed = install_path_exists(install_dir);
    if (os_rollback_begin(&rollback, error, sizeof(error)) != 0) {
        install_copy(message, message_size, error);
        return -1;
    }

    progress.completed_files = 0;
    progress.total_files = manifest.file_count + os_online_selected_count(&manifest, online_component_mask) + 1;
    install_report_progress(OS_INSTALL_PROGRESS_PREPARE,
                            "Opening embedded payload archive",
                            exe_path,
                            install_dir,
                            0,
                            progress.total_files,
                            8);

    if (os_pack_extract_payload_with_rollback(exe_path, install_dir, &rollback, progress.total_files, error, sizeof(error)) != 0) {
        char primary_error[OS_MAX_MESSAGE_LEN];

        install_copy(primary_error, sizeof(primary_error), error);
        install_report_progress(OS_INSTALL_PROGRESS_ROLLBACK,
                                "Rolling back extracted files",
                                NULL,
                                install_dir,
                                progress.completed_files,
                                progress.total_files,
                                90);
        os_rollback_revert(&rollback, error, sizeof(error));
        install_rollback_fresh_dir(install_dir, install_dir_existed);
        install_copy(message, message_size, primary_error);
        return -1;
    }
    progress.completed_files = manifest.file_count;

    if (os_online_install_components(&manifest,
                                     install_dir,
                                     online_component_mask,
                                     &rollback,
                                     &progress,
                                     error,
                                     sizeof(error)) != 0) {
        char primary_error[OS_MAX_MESSAGE_LEN];

        install_copy(primary_error, sizeof(primary_error), error);
        install_report_progress(OS_INSTALL_PROGRESS_ROLLBACK,
                                "Rolling back online component failure",
                                NULL,
                                install_dir,
                                progress.completed_files,
                                progress.total_files,
                                90);
        os_rollback_revert(&rollback, error, sizeof(error));
        install_rollback_fresh_dir(install_dir, install_dir_existed);
        install_copy(message, message_size, primary_error);
        return -1;
    }

#if defined(_WIN32)
    if (install_join(uninstaller_path, sizeof(uninstaller_path), install_dir, "uninstaller.exe") != 0)
#else
    if (install_join(uninstaller_path, sizeof(uninstaller_path), install_dir, "uninstaller") != 0)
#endif
    {
        install_set_error(message, message_size, "Uninstaller storage path is too long.");
        return -1;
    }

    install_report_progress(OS_INSTALL_PROGRESS_REGISTER,
                            "Installing native uninstaller",
                            exe_path,
                            uninstaller_path,
                            progress.completed_files,
                            progress.total_files,
                            86);

    if (os_rollback_capture_file(&rollback, uninstaller_path, error, sizeof(error)) != 0 ||
        install_mkdirs(install_dir) != 0 ||
        os_pack_extract_uninstaller(exe_path, uninstaller_path, error, sizeof(error)) != 0) {
        char primary_error[OS_MAX_MESSAGE_LEN];

        install_copy(primary_error, sizeof(primary_error), error[0] != '\0' ? error : "Cannot write the native uninstaller.");
        install_report_progress(OS_INSTALL_PROGRESS_ROLLBACK,
                                "Rolling back uninstaller extraction failure",
                                NULL,
                                install_dir,
                                progress.completed_files,
                                progress.total_files,
                                90);
        os_rollback_revert(&rollback, error, sizeof(error));
        install_rollback_fresh_dir(install_dir, install_dir_existed);
        install_copy(message, message_size, primary_error);
        return -1;
    }

#if !defined(_WIN32)
    chmod(uninstaller_path, 0755);
#endif
    progress.completed_files++;

    install_report_progress(OS_INSTALL_PROGRESS_REGISTER,
                            "Writing uninstall registration",
                            install_dir,
                            install_dir,
                            progress.completed_files,
                            progress.total_files,
                            92);

    if (install_register_package(install_dir, &manifest, install_dir, error, sizeof(error)) != 0) {
        char registration_error[OS_MAX_MESSAGE_LEN];

        install_copy(registration_error, sizeof(registration_error), error);
        install_report_progress(OS_INSTALL_PROGRESS_ROLLBACK,
                                "Rolling back registration failure",
                                NULL,
                                install_dir,
                                progress.completed_files,
                                progress.total_files,
                                94);
        os_rollback_revert(&rollback, error, sizeof(error));
        install_rollback_fresh_dir(install_dir, install_dir_existed);
        install_copy(message, message_size, registration_error);
        return -1;
    }

    os_rollback_commit(&rollback);
    install_report_progress(OS_INSTALL_PROGRESS_COMPLETE,
                            "Installation complete",
                            NULL,
                            install_dir,
                            progress.completed_files,
                            progress.total_files,
                            100);
    install_set_error(message,
                      message_size,
                      "Installed %s to %s. Uninstaller registered in the system.",
                      manifest.app_name,
                      install_dir);
    return 0;
}

int os_uninstall_embedded_package(const char *exe_path,
                                  const char *override_install_dir,
                                  char *message,
                                  size_t message_size)
{
    OsPackageManifest manifest;
    char error[OS_MAX_MESSAGE_LEN];
    char install_dir[OS_MAX_PATH_LEN];
    char uninstaller_dir[OS_MAX_PATH_LEN];
    const char *raw_install_dir;

    if (message != NULL && message_size > 0) {
        message[0] = '\0';
    }
    error[0] = '\0';

    if (exe_path == NULL || exe_path[0] == '\0') {
        install_set_error(message, message_size, "Native uninstaller EXE path is required.");
        return -1;
    }

    if (os_pack_read_manifest(exe_path, &manifest, error, sizeof(error)) != 0) {
        install_copy(message, message_size, error);
        return -1;
    }

    raw_install_dir = (override_install_dir != NULL && override_install_dir[0] != '\0')
                          ? override_install_dir
                          : manifest.install_dir;
    if (install_expand_dir(raw_install_dir, install_dir, sizeof(install_dir)) != 0 ||
        install_delete_target_is_dangerous(install_dir)) {
        install_set_error(message, message_size, "Install directory is invalid.");
        return -1;
    }

    install_report_progress(OS_INSTALL_PROGRESS_PREPARE,
                            "Preparing removal plan",
                            NULL,
                            install_dir,
                            0,
                            manifest.file_count,
                            10);

    if (install_remove_tree(install_dir, error, sizeof(error)) != 0) {
        install_copy(message, message_size, error);
        return -1;
    }

    install_report_progress(OS_INSTALL_PROGRESS_UNREGISTER,
                            "Removing uninstall registration",
                            NULL,
                            install_dir,
                            manifest.file_count,
                            manifest.file_count,
                            82);

    if (install_unregister_package(&manifest, error, sizeof(error)) != 0) {
        install_copy(message, message_size, error);
        return -1;
    }

    if (install_uninstaller_store_dir(&manifest, uninstaller_dir, sizeof(uninstaller_dir)) == 0) {
#if defined(_WIN32)
        MoveFileExA(exe_path, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExA(uninstaller_dir, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
#else
        unlink(exe_path);
        rmdir(uninstaller_dir);
#endif
    }

    install_report_progress(OS_INSTALL_PROGRESS_COMPLETE,
                            "Removal complete",
                            NULL,
                            install_dir,
                            manifest.file_count,
                            manifest.file_count,
                            100);
    install_set_error(message,
                      message_size,
                      "Uninstalled %s from %s and removed system uninstall metadata.",
                      manifest.app_name,
                      install_dir);
    return 0;
}
