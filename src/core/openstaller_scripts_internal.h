#ifndef OPENSTALLER_SCRIPTS_INTERNAL_H
#define OPENSTALLER_SCRIPTS_INTERNAL_H

#include "openstaller_scripts.h"

#include <stdio.h>
#include <stddef.h>

int scripts_join(char *out, size_t out_size, const char *left, const char *right);
void scripts_write_bat_value(FILE *file, const char *value);
void scripts_write_sh_single(FILE *file, const char *value);

#endif
