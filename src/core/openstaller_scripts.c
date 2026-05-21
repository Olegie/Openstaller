#include "openstaller_scripts_internal.h"

#include <string.h>

#if defined(_WIN32)
#define OS_PATH_SEP '\\'
#else
#define OS_PATH_SEP '/'
#endif

static int scripts_copy(char *dst, size_t dst_size, const char *src)
{
    size_t len = strlen(src);

    if (len >= dst_size) {
        return -1;
    }

    memcpy(dst, src, len + 1);
    return 0;
}

static int scripts_append(char *dst, size_t dst_size, const char *src)
{
    size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);

    if (dst_len + src_len >= dst_size) {
        return -1;
    }

    memcpy(dst + dst_len, src, src_len + 1);
    return 0;
}

static int scripts_is_sep(char ch)
{
    return ch == '/' || ch == '\\';
}

int scripts_join(char *out, size_t out_size, const char *left, const char *right)
{
    size_t len;

    if (scripts_copy(out, out_size, left) != 0) {
        return -1;
    }

    len = strlen(out);
    if (len > 0 && !scripts_is_sep(out[len - 1])) {
        char sep[2] = {OS_PATH_SEP, '\0'};
        if (scripts_append(out, out_size, sep) != 0) {
            return -1;
        }
    }

    return scripts_append(out, out_size, right);
}

void scripts_write_bat_value(FILE *file, const char *value)
{
    while (*value != '\0') {
        fputc(*value == '"' ? '\'' : *value, file);
        ++value;
    }
}

void scripts_write_sh_single(FILE *file, const char *value)
{
    fputc('\'', file);
    while (*value != '\0') {
        if (*value == '\'') {
            fputs("'\\''", file);
        } else {
            fputc(*value, file);
        }
        ++value;
    }
    fputc('\'', file);
}
