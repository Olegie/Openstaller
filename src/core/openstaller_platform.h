#ifndef OPENSTALLER_PLATFORM_H
#define OPENSTALLER_PLATFORM_H

#include "openstaller/openstaller.h"

#include <stddef.h>

const char *os_platform_family(void);
int os_platform_registration_root(char *out, size_t out_size);
int os_platform_uninstaller_root(char *out, size_t out_size);
int os_platform_applications_dir(char *out, size_t out_size);

#endif
