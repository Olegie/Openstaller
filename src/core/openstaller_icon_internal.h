#ifndef OPENSTALLER_ICON_INTERNAL_H
#define OPENSTALLER_ICON_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct IconImage {
    unsigned char width;
    unsigned char height;
    unsigned char color_count;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t size;
    const unsigned char *data;
    unsigned char *owned;
} IconImage;

int icon_load_file(const char *path, unsigned char **data, size_t *size, char *error, size_t error_size);
int icon_parse_ico(const unsigned char *ico,
                   size_t ico_size,
                   IconImage **images_out,
                   uint16_t *count_out,
                   char *error,
                   size_t error_size);
void icon_free_images(IconImage *images, uint16_t count);
int icon_update_resources(HANDLE update,
                          const IconImage *images,
                          uint16_t count,
                          char *error,
                          size_t error_size);

#endif

#endif
