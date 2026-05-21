#ifndef OPENSTALLER_ROLLBACK_H
#define OPENSTALLER_ROLLBACK_H

#include "openstaller/openstaller.h"

#include <stddef.h>

typedef struct OsRollbackEntry {
    char target[OS_MAX_PATH_LEN];
    char backup[OS_MAX_PATH_LEN];
    int existed;
} OsRollbackEntry;

typedef struct OsRollback {
    OsRollbackEntry *entries;
    size_t count;
    size_t capacity;
    char staging_dir[OS_MAX_PATH_LEN];
} OsRollback;

void os_rollback_init(OsRollback *rollback);
int os_rollback_begin(OsRollback *rollback, char *error, size_t error_size);
int os_rollback_capture_file(OsRollback *rollback, const char *target, char *error, size_t error_size);
void os_rollback_commit(OsRollback *rollback);
void os_rollback_revert(OsRollback *rollback, char *error, size_t error_size);

#endif
