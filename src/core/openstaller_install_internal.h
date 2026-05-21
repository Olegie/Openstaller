#ifndef OPENSTALLER_INSTALL_INTERNAL_H
#define OPENSTALLER_INSTALL_INTERNAL_H

#include "openstaller/openstaller.h"
#include "openstaller_manifest.h"
#include "openstaller_rollback.h"

#include <stddef.h>

typedef struct OsInstallProgressCounter {
    size_t completed_files;
    size_t total_files;
} OsInstallProgressCounter;

void install_set_error(char *buffer, size_t size, const char *format, ...);
int install_copy(char *dst, size_t dst_size, const char *src);
int install_append(char *dst, size_t dst_size, const char *src);
int install_join(char *out, size_t out_size, const char *left, const char *right);
int install_mkdirs(const char *path);
int install_path_exists(const char *path);
int install_copy_plain_file(const char *src, const char *dst);
int install_expand_dir(const char *input, char *out, size_t out_size);
int install_delete_target_is_dangerous(const char *path);
int install_copy_tree_plain(const char *src_dir, const char *dst_dir, char *error, size_t error_size);
int install_copy_tree_with_rollback(const char *src_dir,
                                    const char *dst_dir,
                                    OsRollback *rollback,
                                    OsInstallProgressCounter *progress,
                                    char *error,
                                    size_t error_size);
int install_remove_tree(const char *path, char *error, size_t error_size);
int install_progress_percent(size_t completed, size_t total, int start, int end);
void install_report_progress(OsInstallProgressStage stage,
                             const char *action,
                             const char *source_path,
                             const char *target_path,
                             size_t completed_files,
                             size_t total_files,
                             int percent);
int install_uninstaller_store_dir(const OsPackageManifest *manifest, char *out, size_t out_size);
int install_register_package(const char *package_dir,
                             const OsPackageManifest *manifest,
                             const char *install_dir,
                             char *error,
                             size_t error_size);
int install_unregister_package(const OsPackageManifest *manifest, char *error, size_t error_size);

#if !defined(_WIN32)
int install_file_exists(const char *path);
#endif

#endif
