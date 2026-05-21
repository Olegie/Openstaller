#ifndef OPENSTALLER_PACK_H
#define OPENSTALLER_PACK_H

#include "openstaller/openstaller.h"
#include "openstaller_manifest.h"
#include "openstaller_rollback.h"

#include <stddef.h>

int os_pack_append_archive(const OsProjectConfig *config,
                           const char *package_dir,
                           const char *exe_path,
                           int include_payload,
                           int include_license,
                           const char *uninstaller_path,
                           char *error,
                           size_t error_size);
int os_pack_read_manifest(const char *exe_path,
                          OsPackageManifest *manifest,
                          char *error,
                          size_t error_size);
int os_pack_read_info(const char *exe_path,
                      OsPackageInfo *info,
                      char *message,
                      size_t message_size);
int os_pack_extract_payload(const char *exe_path,
                            const char *dst_dir,
                            char *message,
                            size_t message_size);
int os_pack_extract_payload_with_rollback(const char *exe_path,
                                          const char *dst_dir,
                                          OsRollback *rollback,
                                          size_t total_files,
                                          char *message,
                                          size_t message_size);
int os_pack_extract_license(const char *exe_path,
                            const char *dst_path,
                            char *message,
                            size_t message_size);
int os_pack_extract_wizard_image(const char *exe_path,
                                 const char *dst_path,
                                 char *message,
                                 size_t message_size);
int os_pack_extract_background_image(const char *exe_path,
                                     const char *dst_path,
                                     char *message,
                                     size_t message_size);
int os_pack_extract_uninstaller(const char *exe_path,
                                const char *dst_path,
                                char *message,
                                size_t message_size);

#endif
