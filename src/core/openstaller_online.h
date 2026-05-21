#ifndef OPENSTALLER_ONLINE_H
#define OPENSTALLER_ONLINE_H

#include "openstaller_install_internal.h"

#include <stdint.h>

uint64_t os_online_default_mask(const OsPackageManifest *manifest);
size_t os_online_selected_count(const OsPackageManifest *manifest, uint64_t selected_mask);
int os_online_install_components(const OsPackageManifest *manifest,
                                 const char *install_dir,
                                 uint64_t selected_mask,
                                 OsRollback *rollback,
                                 OsInstallProgressCounter *progress,
                                 char *error,
                                 size_t error_size);

#endif
