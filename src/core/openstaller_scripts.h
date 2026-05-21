#ifndef OPENSTALLER_SCRIPTS_H
#define OPENSTALLER_SCRIPTS_H

#include "openstaller/openstaller.h"

#include <stdint.h>

int os_scripts_write_windows(const OsProjectConfig *config,
                             const char *package_dir,
                             const char *safe_name,
                             uint64_t installer_id);
int os_scripts_write_unix(const OsProjectConfig *config,
                          const char *package_dir,
                          const char *safe_name,
                          uint64_t installer_id);

#endif
