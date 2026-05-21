#ifndef OPENSTALLER_VERSION_H
#define OPENSTALLER_VERSION_H

#include "openstaller/openstaller.h"

#include <stddef.h>

int os_version_apply_to_exe(const char *exe_path,
                            const OsProjectConfig *config,
                            int uninstall_exe,
                            char *error,
                            size_t error_size);

#endif
