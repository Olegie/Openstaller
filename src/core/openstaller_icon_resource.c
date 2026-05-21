#include "openstaller_icon_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)

static void icon_resource_set_error(char *buffer, size_t size, const char *format, ...)
{
    va_list args;

    if (size == 0) {
        return;
    }

    va_start(args, format);
    vsnprintf(buffer, size, format, args);
    va_end(args);
    buffer[size - 1] = '\0';
}

static uint16_t icon_resource_read_u16(const unsigned char *data)
{
    return (uint16_t)(data[0] | (data[1] << 8));
}

static uint32_t icon_resource_read_u32(const unsigned char *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static void icon_resource_write_u16(unsigned char *data, uint16_t value)
{
    data[0] = (unsigned char)(value & 0xffu);
    data[1] = (unsigned char)((value >> 8) & 0xffu);
}

static void icon_resource_write_u32(unsigned char *data, uint32_t value)
{
    data[0] = (unsigned char)(value & 0xffu);
    data[1] = (unsigned char)((value >> 8) & 0xffu);
    data[2] = (unsigned char)((value >> 16) & 0xffu);
    data[3] = (unsigned char)((value >> 24) & 0xffu);
}

int icon_load_file(const char *path, unsigned char **data, size_t *size, char *error, size_t error_size)
{
    FILE *file;
    long length;
    unsigned char *buffer;

    file = fopen(path, "rb");
    if (file == NULL) {
        icon_resource_set_error(error, error_size, "Cannot open icon file: %s", path);
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0 ||
        (length = ftell(file)) <= 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        icon_resource_set_error(error, error_size, "Cannot read icon file: %s", path);
        return -1;
    }

    buffer = (unsigned char *)malloc((size_t)length);
    if (buffer == NULL) {
        fclose(file);
        icon_resource_set_error(error, error_size, "Not enough memory to read icon file.");
        return -1;
    }

    if (fread(buffer, 1, (size_t)length, file) != (size_t)length) {
        free(buffer);
        fclose(file);
        icon_resource_set_error(error, error_size, "Cannot read icon file: %s", path);
        return -1;
    }

    fclose(file);
    *data = buffer;
    *size = (size_t)length;
    return 0;
}

int icon_parse_ico(const unsigned char *ico,
                   size_t ico_size,
                   IconImage **images_out,
                   uint16_t *count_out,
                   char *error,
                   size_t error_size)
{
    IconImage *images;
    uint16_t count;
    uint16_t i;

    if (ico_size < 6 ||
        icon_resource_read_u16(ico) != 0 ||
        icon_resource_read_u16(ico + 2) != 1) {
        icon_resource_set_error(error, error_size, "Icon file is not a valid ICO image.");
        return -1;
    }

    count = icon_resource_read_u16(ico + 4);
    if (count == 0 || count > 32 || ico_size < 6u + (size_t)count * 16u) {
        icon_resource_set_error(error, error_size, "Icon file has an invalid image table.");
        return -1;
    }

    images = (IconImage *)calloc(count, sizeof(*images));
    if (images == NULL) {
        icon_resource_set_error(error, error_size, "Not enough memory to parse icon file.");
        return -1;
    }

    for (i = 0; i < count; ++i) {
        const unsigned char *entry = ico + 6u + (size_t)i * 16u;
        uint32_t bytes = icon_resource_read_u32(entry + 8);
        uint32_t offset = icon_resource_read_u32(entry + 12);

        if (bytes == 0 || offset > ico_size || bytes > ico_size - offset) {
            free(images);
            icon_resource_set_error(error, error_size, "Icon file contains an invalid image entry.");
            return -1;
        }

        images[i].width = entry[0];
        images[i].height = entry[1];
        images[i].color_count = entry[2];
        images[i].planes = icon_resource_read_u16(entry + 4);
        images[i].bit_count = icon_resource_read_u16(entry + 6);
        images[i].size = bytes;
        images[i].data = ico + offset;
        images[i].owned = NULL;
    }

    *images_out = images;
    *count_out = count;
    return 0;
}

void icon_free_images(IconImage *images, uint16_t count)
{
    uint16_t i;

    if (images == NULL) {
        return;
    }

    for (i = 0; i < count; ++i) {
        free(images[i].owned);
    }
    free(images);
}

int icon_update_resources(HANDLE update, const IconImage *images, uint16_t count, char *error, size_t error_size)
{
    WORD language = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    unsigned char *group;
    size_t group_size = 6u + (size_t)count * 14u;
    uint16_t i;

    group = (unsigned char *)calloc(1, group_size);
    if (group == NULL) {
        icon_resource_set_error(error, error_size, "Not enough memory to build icon resource.");
        return -1;
    }

    icon_resource_write_u16(group, 0);
    icon_resource_write_u16(group + 2, 1);
    icon_resource_write_u16(group + 4, count);

    for (i = 0; i < count; ++i) {
        unsigned char *entry = group + 6u + (size_t)i * 14u;
        uint16_t id = (uint16_t)(i + 1u);

        if (!UpdateResourceA(update,
                             RT_ICON,
                             MAKEINTRESOURCEA(id),
                             language,
                             (void *)images[i].data,
                             images[i].size)) {
            free(group);
            icon_resource_set_error(error, error_size, "Cannot write icon image resource.");
            return -1;
        }

        entry[0] = images[i].width;
        entry[1] = images[i].height;
        entry[2] = images[i].color_count;
        entry[3] = 0;
        icon_resource_write_u16(entry + 4, images[i].planes == 0 ? 1 : images[i].planes);
        icon_resource_write_u16(entry + 6, images[i].bit_count == 0 ? 32 : images[i].bit_count);
        icon_resource_write_u32(entry + 8, images[i].size);
        icon_resource_write_u16(entry + 12, id);
    }

    if (!UpdateResourceA(update,
                         RT_GROUP_ICON,
                         MAKEINTRESOURCEA(1),
                         language,
                         group,
                         (DWORD)group_size)) {
        free(group);
        icon_resource_set_error(error, error_size, "Cannot write icon group resource.");
        return -1;
    }

    free(group);
    return 0;
}

#endif
