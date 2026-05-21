#include "openstaller_pack.h"
#include "openstaller_pack_compress.h"
#include "openstaller_install_internal.h"

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <windows.h>
#define OS_PATH_SEP '\\'
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define OS_PATH_SEP '/'
#endif

#define OS_PACK_COPY_BUFFER_SIZE 65536u
#define OS_PACK_VERSION_CURRENT 2u
#define OS_PACK_ENTRY_FLAG_DEFLATE 0x00000001u

enum {
    OS_PACK_ENTRY_MANIFEST = 1,
    OS_PACK_ENTRY_PAYLOAD = 2,
    OS_PACK_ENTRY_LICENSE = 3,
    OS_PACK_ENTRY_UNINSTALLER = 4,
    OS_PACK_ENTRY_WIZARD_IMAGE = 5,
    OS_PACK_ENTRY_BACKGROUND_IMAGE = 6
};

typedef struct OsPackHeader {
    char magic[16];
    uint32_t version;
    uint32_t entry_count;
} OsPackHeader;

typedef struct OsPackEntryHeader {
    uint32_t type;
    uint32_t path_len;
    uint64_t size;
    uint32_t flags;
    uint32_t reserved;
    uint64_t original_size;
} OsPackEntryHeader;

typedef struct OsPackEntryHeaderV1 {
    uint32_t type;
    uint32_t path_len;
    uint64_t size;
} OsPackEntryHeaderV1;

typedef struct OsPackFooter {
    char magic[16];
    uint64_t archive_offset;
    uint64_t archive_size;
    uint32_t entry_count;
    uint32_t reserved;
} OsPackFooter;

static const char OS_PACK_MAGIC[16] = {
    'O', 'S', 'T', 'A', 'L', 'L', 'E', 'R', 'P', 'K', 'G', '1', 0, 0, 0, 0
};

static void pack_set_error(char *buffer, size_t size, const char *format, ...)
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

static int pack_is_sep(char ch)
{
    return ch == '/' || ch == '\\';
}

static int pack_copy_string(char *dst, size_t dst_size, const char *src)
{
    size_t len;

    if (dst_size == 0) {
        return -1;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return 0;
    }

    len = strlen(src);
    if (len >= dst_size) {
        dst[0] = '\0';
        return -1;
    }

    memcpy(dst, src, len + 1);
    return 0;
}

static int pack_append_string(char *dst, size_t dst_size, const char *src)
{
    size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);

    if (dst_len + src_len >= dst_size) {
        return -1;
    }

    memcpy(dst + dst_len, src, src_len + 1);
    return 0;
}

static int pack_path_join(char *out, size_t out_size, const char *left, const char *right)
{
    size_t len;

    if (pack_copy_string(out, out_size, left) != 0) {
        return -1;
    }

    len = strlen(out);
    if (len > 0 && !pack_is_sep(out[len - 1])) {
        char sep[2] = {OS_PATH_SEP, '\0'};
        if (pack_append_string(out, out_size, sep) != 0) {
            return -1;
        }
    }

    return pack_append_string(out, out_size, right);
}

static int pack_parent_dir(char *out, size_t out_size, const char *path)
{
    size_t len;

    if (pack_copy_string(out, out_size, path) != 0) {
        return -1;
    }

    len = strlen(out);
    while (len > 0 && !pack_is_sep(out[len - 1])) {
        --len;
    }

    if (len == 0) {
        out[0] = '\0';
        return 0;
    }

    while (len > 1 && pack_is_sep(out[len - 1])) {
        --len;
    }
    out[len] = '\0';
    return 0;
}

static int pack_mkdir_one(const char *path)
{
#if defined(_WIN32)
    if (_mkdir(path) == 0 || errno == EEXIST) {
        return 0;
    }
#else
    if (mkdir(path, 0755) == 0 || errno == EEXIST) {
        return 0;
    }
#endif
    return -1;
}

static int pack_mkdirs(const char *path)
{
    char tmp[OS_MAX_PATH_LEN];
    char *cursor;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    if (pack_copy_string(tmp, sizeof(tmp), path) != 0) {
        return -1;
    }

    for (cursor = tmp; *cursor != '\0'; ++cursor) {
        if (!pack_is_sep(*cursor)) {
            continue;
        }
        if (cursor == tmp) {
            continue;
        }
#if defined(_WIN32)
        if (cursor == tmp + 2 && tmp[1] == ':') {
            continue;
        }
#endif
        *cursor = '\0';
        if (tmp[0] != '\0' && pack_mkdir_one(tmp) != 0) {
            *cursor = OS_PATH_SEP;
            return -1;
        }
        *cursor = OS_PATH_SEP;
    }

    return pack_mkdir_one(tmp);
}

static void pack_normalize_path(char *path)
{
    char *cursor;

    for (cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
}

static int pack_relative_path_is_safe(const char *path)
{
    const char *segment = path;

    if (path == NULL || path[0] == '\0' || pack_is_sep(path[0]) || strchr(path, ':') != NULL) {
        return 0;
    }

    while (*segment != '\0') {
        const char *end = segment;
        size_t len;

        while (*end != '\0' && !pack_is_sep(*end)) {
            ++end;
        }

        len = (size_t)(end - segment);
        if (len == 0 ||
            (len == 1 && segment[0] == '.') ||
            (len == 2 && segment[0] == '.' && segment[1] == '.')) {
            return 0;
        }

        segment = *end == '\0' ? end : end + 1;
    }

    return 1;
}

static int pack_join_relative_path(char *out, size_t out_size, const char *root, const char *relative)
{
    char rel[OS_MAX_PATH_LEN];
    char *cursor;

    if (!pack_relative_path_is_safe(relative) || pack_copy_string(rel, sizeof(rel), relative) != 0) {
        return -1;
    }

    for (cursor = rel; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') {
            *cursor = OS_PATH_SEP;
        }
    }

    return pack_path_join(out, out_size, root, rel);
}

static int pack_file_size(const char *path, uint64_t *size_out)
{
    FILE *file = fopen(path, "rb");
    long size;

    if (file == NULL) {
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }

    size = ftell(file);
    fclose(file);
    if (size < 0) {
        return -1;
    }

    *size_out = (uint64_t)size;
    return 0;
}

static int pack_seek64(FILE *file, uint64_t offset, int origin)
{
#if defined(_WIN32)
    return _fseeki64(file, (__int64)offset, origin);
#else
    return fseeko(file, (off_t)offset, origin);
#endif
}

static int pack_tell64(FILE *file, uint64_t *offset)
{
#if defined(_WIN32)
    __int64 pos = _ftelli64(file);
    if (pos < 0) {
        return -1;
    }
    *offset = (uint64_t)pos;
    return 0;
#else
    off_t pos = ftello(file);
    if (pos < 0) {
        return -1;
    }
    *offset = (uint64_t)pos;
    return 0;
#endif
}

static int pack_write_exact(FILE *file, const void *data, size_t size)
{
    return fwrite(data, 1, size, file) == size ? 0 : -1;
}

static int pack_read_exact(FILE *file, void *data, size_t size)
{
    return fread(data, 1, size, file) == size ? 0 : -1;
}

static int pack_copy_stream(FILE *in, FILE *out, uint64_t size)
{
    unsigned char buffer[OS_PACK_COPY_BUFFER_SIZE];

    while (size > 0) {
        size_t chunk = size > sizeof(buffer) ? sizeof(buffer) : (size_t)size;
        if (fread(buffer, 1, chunk, in) != chunk ||
            fwrite(buffer, 1, chunk, out) != chunk) {
            return -1;
        }
        size -= chunk;
    }

    return 0;
}

static int pack_skip_stream(FILE *file, uint64_t size)
{
    uint64_t pos;

    if (pack_tell64(file, &pos) != 0) {
        return -1;
    }

    return pack_seek64(file, pos + size, SEEK_SET);
}

static int pack_count_payload_entries(const char *payload_dir,
                                      uint32_t *count,
                                      char *error,
                                      size_t error_size)
{
#if defined(_WIN32)
    WIN32_FIND_DATAA data;
    HANDLE handle;
    char pattern[OS_MAX_PATH_LEN];

    if (pack_path_join(pattern, sizeof(pattern), payload_dir, "*") != 0) {
        pack_set_error(error, error_size, "Payload path is too long.");
        return -1;
    }

    handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        pack_set_error(error, error_size, "Cannot read payload directory: %s", payload_dir);
        return -1;
    }

    do {
        char child[OS_MAX_PATH_LEN];

        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
            continue;
        }

        if (pack_path_join(child, sizeof(child), payload_dir, data.cFileName) != 0) {
            FindClose(handle);
            pack_set_error(error, error_size, "Payload path is too long.");
            return -1;
        }

        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (pack_count_payload_entries(child, count, error, error_size) != 0) {
                FindClose(handle);
                return -1;
            }
        } else {
            ++(*count);
        }
    } while (FindNextFileA(handle, &data) != 0);

    FindClose(handle);
    return 0;
#else
    DIR *dir;
    struct dirent *entry;

    dir = opendir(payload_dir);
    if (dir == NULL) {
        pack_set_error(error, error_size, "Cannot read payload directory: %s", payload_dir);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char child[OS_MAX_PATH_LEN];
        struct stat st;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (pack_path_join(child, sizeof(child), payload_dir, entry->d_name) != 0) {
            closedir(dir);
            pack_set_error(error, error_size, "Payload path is too long.");
            return -1;
        }

        if (stat(child, &st) != 0) {
            closedir(dir);
            pack_set_error(error, error_size, "Cannot stat payload path: %s", child);
            return -1;
        }

        if (S_ISDIR(st.st_mode)) {
            if (pack_count_payload_entries(child, count, error, error_size) != 0) {
                closedir(dir);
                return -1;
            }
        } else if (S_ISREG(st.st_mode)) {
            ++(*count);
        }
    }

    closedir(dir);
    return 0;
#endif
}

static int pack_write_file_entry(FILE *out,
                                 uint32_t type,
                                 const char *entry_path,
                                 const char *src_path,
                                 char *error,
                                 size_t error_size)
{
    OsPackEntryHeader entry;
    char normalized[OS_MAX_PATH_LEN];
    FILE *in;
#if defined(OPENSTALLER_HAVE_ZLIB)
    FILE *compressed;
#endif

    if (pack_copy_string(normalized, sizeof(normalized), entry_path) != 0) {
        pack_set_error(error, error_size, "Archive entry path is too long: %s", entry_path);
        return -1;
    }
    pack_normalize_path(normalized);

    memset(&entry, 0, sizeof(entry));
    entry.type = type;
    entry.path_len = (uint32_t)strlen(normalized);
    if (pack_file_size(src_path, &entry.original_size) != 0) {
        pack_set_error(error, error_size, "Cannot read file for EXE archive: %s", src_path);
        return -1;
    }
    entry.size = entry.original_size;

    in = fopen(src_path, "rb");
    if (in == NULL) {
        pack_set_error(error, error_size, "Cannot open file for EXE archive: %s", src_path);
        return -1;
    }

#if defined(OPENSTALLER_HAVE_ZLIB)
    if (type == OS_PACK_ENTRY_PAYLOAD) {
        entry.flags = OS_PACK_ENTRY_FLAG_DEFLATE;
        entry.size = 0;
        compressed = tmpfile();
        if (compressed == NULL) {
            fclose(in);
            pack_set_error(error, error_size, "Cannot create temporary compressed payload stream.");
            return -1;
        }
        if (os_pack_deflate_stream(in, compressed, &entry.size) != 0 ||
            pack_seek64(compressed, 0, SEEK_SET) != 0 ||
            pack_write_exact(out, &entry, sizeof(entry)) != 0 ||
            pack_write_exact(out, normalized, entry.path_len) != 0 ||
            pack_copy_stream(compressed, out, entry.size) != 0) {
            fclose(compressed);
            fclose(in);
            pack_set_error(error, error_size, "Cannot append compressed payload to EXE archive: %s", src_path);
            return -1;
        }

        fclose(compressed);
        fclose(in);
        return 0;
    }
#endif

    if (pack_write_exact(out, &entry, sizeof(entry)) != 0 ||
        pack_write_exact(out, normalized, entry.path_len) != 0 ||
        pack_copy_stream(in, out, entry.size) != 0) {
        fclose(in);
        pack_set_error(error, error_size, "Cannot append file to EXE archive: %s", src_path);
        return -1;
    }

    fclose(in);
    return 0;
}

static int pack_write_payload_entries(FILE *out,
                                      const char *payload_dir,
                                      const char *rel_dir,
                                      char *error,
                                      size_t error_size)
{
#if defined(_WIN32)
    WIN32_FIND_DATAA data;
    HANDLE handle;
    char pattern[OS_MAX_PATH_LEN];

    if (pack_path_join(pattern, sizeof(pattern), payload_dir, "*") != 0) {
        pack_set_error(error, error_size, "Payload path is too long.");
        return -1;
    }

    handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        pack_set_error(error, error_size, "Cannot read payload directory: %s", payload_dir);
        return -1;
    }

    do {
        char child[OS_MAX_PATH_LEN];
        char rel_child[OS_MAX_PATH_LEN];

        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
            continue;
        }

        if (pack_path_join(child, sizeof(child), payload_dir, data.cFileName) != 0) {
            FindClose(handle);
            pack_set_error(error, error_size, "Payload path is too long.");
            return -1;
        }

        if (rel_dir[0] == '\0') {
            if (pack_copy_string(rel_child, sizeof(rel_child), data.cFileName) != 0) {
                FindClose(handle);
                pack_set_error(error, error_size, "Archive path is too long.");
                return -1;
            }
        } else if (pack_path_join(rel_child, sizeof(rel_child), rel_dir, data.cFileName) != 0) {
            FindClose(handle);
            pack_set_error(error, error_size, "Archive path is too long.");
            return -1;
        }

        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (pack_write_payload_entries(out, child, rel_child, error, error_size) != 0) {
                FindClose(handle);
                return -1;
            }
        } else if (pack_write_file_entry(out, OS_PACK_ENTRY_PAYLOAD, rel_child, child, error, error_size) != 0) {
            FindClose(handle);
            return -1;
        }
    } while (FindNextFileA(handle, &data) != 0);

    FindClose(handle);
    return 0;
#else
    DIR *dir;
    struct dirent *entry;

    dir = opendir(payload_dir);
    if (dir == NULL) {
        pack_set_error(error, error_size, "Cannot read payload directory: %s", payload_dir);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char child[OS_MAX_PATH_LEN];
        char rel_child[OS_MAX_PATH_LEN];
        struct stat st;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (pack_path_join(child, sizeof(child), payload_dir, entry->d_name) != 0) {
            closedir(dir);
            pack_set_error(error, error_size, "Payload path is too long.");
            return -1;
        }

        if (rel_dir[0] == '\0') {
            if (pack_copy_string(rel_child, sizeof(rel_child), entry->d_name) != 0) {
                closedir(dir);
                pack_set_error(error, error_size, "Archive path is too long.");
                return -1;
            }
        } else if (pack_path_join(rel_child, sizeof(rel_child), rel_dir, entry->d_name) != 0) {
            closedir(dir);
            pack_set_error(error, error_size, "Archive path is too long.");
            return -1;
        }

        if (stat(child, &st) != 0) {
            closedir(dir);
            pack_set_error(error, error_size, "Cannot stat payload path: %s", child);
            return -1;
        }

        if (S_ISDIR(st.st_mode)) {
            if (pack_write_payload_entries(out, child, rel_child, error, error_size) != 0) {
                closedir(dir);
                return -1;
            }
        } else if (S_ISREG(st.st_mode) &&
                   pack_write_file_entry(out, OS_PACK_ENTRY_PAYLOAD, rel_child, child, error, error_size) != 0) {
            closedir(dir);
            return -1;
        }
    }

    closedir(dir);
    return 0;
#endif
}

int os_pack_append_archive(const OsProjectConfig *config,
                           const char *package_dir,
                           const char *exe_path,
                           int include_payload,
                           int include_license,
                           const char *uninstaller_path,
                           char *error,
                           size_t error_size)
{
    OsPackHeader header;
    OsPackFooter footer;
    FILE *out;
    uint64_t archive_start;
    uint64_t archive_end;
    uint32_t payload_count = 0;
    uint32_t entry_count = 1;
    char manifest_path[OS_MAX_PATH_LEN];
    char payload_dir[OS_MAX_PATH_LEN];
    char license_path[OS_MAX_PATH_LEN];

    if (error != NULL && error_size > 0) {
        error[0] = '\0';
    }

    if (pack_path_join(manifest_path, sizeof(manifest_path), package_dir, "manifest.openstaller") != 0 ||
        pack_path_join(payload_dir, sizeof(payload_dir), package_dir, "payload") != 0 ||
        pack_path_join(license_path, sizeof(license_path), package_dir, "LICENSE.txt") != 0) {
        pack_set_error(error, error_size, "Archive package path is too long.");
        return -1;
    }

    if (include_payload && pack_count_payload_entries(payload_dir, &payload_count, error, error_size) != 0) {
        return -1;
    }

    entry_count += payload_count;
    if (include_license) {
        entry_count++;
    }
    if (config->wizard_image_file[0] != '\0') {
        entry_count++;
    }
    if (config->background_image_file[0] != '\0') {
        entry_count++;
    }
    if (uninstaller_path != NULL && uninstaller_path[0] != '\0') {
        entry_count++;
    }

    out = fopen(exe_path, "ab+");
    if (out == NULL) {
        pack_set_error(error, error_size, "Cannot open native EXE for packaging: %s", exe_path);
        return -1;
    }

    if (pack_seek64(out, 0, SEEK_END) != 0 || pack_tell64(out, &archive_start) != 0) {
        fclose(out);
        pack_set_error(error, error_size, "Cannot seek native EXE for packaging: %s", exe_path);
        return -1;
    }

    memset(&header, 0, sizeof(header));
    memcpy(header.magic, OS_PACK_MAGIC, sizeof(header.magic));
    header.version = OS_PACK_VERSION_CURRENT;
    header.entry_count = entry_count;

    if (pack_write_exact(out, &header, sizeof(header)) != 0 ||
        pack_write_file_entry(out, OS_PACK_ENTRY_MANIFEST, "manifest.openstaller", manifest_path, error, error_size) != 0) {
        fclose(out);
        return -1;
    }

    if (include_payload && pack_write_payload_entries(out, payload_dir, "", error, error_size) != 0) {
        fclose(out);
        return -1;
    }

    if (include_license &&
        pack_write_file_entry(out, OS_PACK_ENTRY_LICENSE, "LICENSE.txt", license_path, error, error_size) != 0) {
        fclose(out);
        return -1;
    }

    if (config->wizard_image_file[0] != '\0' &&
        pack_write_file_entry(out,
                              OS_PACK_ENTRY_WIZARD_IMAGE,
                              "wizard-image.bmp",
                              config->wizard_image_file,
                              error,
                              error_size) != 0) {
        fclose(out);
        return -1;
    }

    if (config->background_image_file[0] != '\0' &&
        pack_write_file_entry(out,
                              OS_PACK_ENTRY_BACKGROUND_IMAGE,
                              "background-image.bmp",
                              config->background_image_file,
                              error,
                              error_size) != 0) {
        fclose(out);
        return -1;
    }

    if (uninstaller_path != NULL && uninstaller_path[0] != '\0' &&
        pack_write_file_entry(out,
                              OS_PACK_ENTRY_UNINSTALLER,
#if defined(_WIN32)
                              "uninstaller.exe",
#else
                              "uninstaller",
#endif
                              uninstaller_path,
                              error,
                              error_size) != 0) {
        fclose(out);
        return -1;
    }

    if (pack_tell64(out, &archive_end) != 0) {
        fclose(out);
        pack_set_error(error, error_size, "Cannot finish native EXE archive: %s", exe_path);
        return -1;
    }

    memset(&footer, 0, sizeof(footer));
    memcpy(footer.magic, OS_PACK_MAGIC, sizeof(footer.magic));
    footer.archive_offset = archive_start;
    footer.archive_size = archive_end - archive_start;
    footer.entry_count = entry_count;

    if (pack_write_exact(out, &footer, sizeof(footer)) != 0 || fclose(out) != 0) {
        pack_set_error(error, error_size, "Cannot write native EXE archive footer: %s", exe_path);
        return -1;
    }

    return 0;
}

static int pack_read_footer(FILE *file, OsPackFooter *footer, char *error, size_t error_size)
{
    uint64_t file_size;
    uint64_t footer_offset;

    if (pack_seek64(file, 0, SEEK_END) != 0 || pack_tell64(file, &file_size) != 0) {
        pack_set_error(error, error_size, "Cannot read native EXE size.");
        return -1;
    }

    if (file_size < sizeof(*footer)) {
        pack_set_error(error, error_size, "This EXE does not contain an Openstaller package archive.");
        return -1;
    }

    footer_offset = file_size - sizeof(*footer);
    if (pack_seek64(file, footer_offset, SEEK_SET) != 0 ||
        pack_read_exact(file, footer, sizeof(*footer)) != 0 ||
        memcmp(footer->magic, OS_PACK_MAGIC, sizeof(footer->magic)) != 0) {
        pack_set_error(error, error_size, "This EXE does not contain an Openstaller package archive.");
        return -1;
    }

    if (footer->archive_offset >= footer_offset ||
        footer->archive_size == 0 ||
        footer->archive_offset + footer->archive_size != footer_offset ||
        footer->entry_count == 0) {
        pack_set_error(error, error_size, "The embedded package archive is damaged.");
        return -1;
    }

    return 0;
}

static int pack_open(const char *exe_path, FILE **file_out, OsPackHeader *header, char *error, size_t error_size)
{
    FILE *file;
    OsPackFooter footer;

    file = fopen(exe_path, "rb");
    if (file == NULL) {
        pack_set_error(error, error_size, "Cannot open native EXE: %s", exe_path);
        return -1;
    }

    if (pack_read_footer(file, &footer, error, error_size) != 0 ||
        pack_seek64(file, footer.archive_offset, SEEK_SET) != 0 ||
        pack_read_exact(file, header, sizeof(*header)) != 0 ||
        memcmp(header->magic, OS_PACK_MAGIC, sizeof(header->magic)) != 0 ||
        (header->version != 1 && header->version != OS_PACK_VERSION_CURRENT) ||
        header->entry_count != footer.entry_count) {
        fclose(file);
        if (error != NULL && error_size > 0 && error[0] == '\0') {
            pack_set_error(error, error_size, "The embedded package archive is damaged.");
        }
        return -1;
    }

    *file_out = file;
    return 0;
}

static int pack_read_entry(FILE *file,
                           uint32_t archive_version,
                           OsPackEntryHeader *entry,
                           char *path,
                           size_t path_size,
                           char *error,
                           size_t error_size)
{
    memset(entry, 0, sizeof(*entry));
    if (archive_version == 1) {
        OsPackEntryHeaderV1 entry_v1;
        if (pack_read_exact(file, &entry_v1, sizeof(entry_v1)) != 0) {
            pack_set_error(error, error_size, "Cannot read embedded package entry.");
            return -1;
        }
        entry->type = entry_v1.type;
        entry->path_len = entry_v1.path_len;
        entry->size = entry_v1.size;
        entry->original_size = entry_v1.size;
    } else if (pack_read_exact(file, entry, sizeof(*entry)) != 0) {
        pack_set_error(error, error_size, "Cannot read embedded package entry.");
        return -1;
    }

    if ((entry->flags & ~OS_PACK_ENTRY_FLAG_DEFLATE) != 0) {
        pack_set_error(error, error_size, "Embedded package entry uses unsupported compression flags.");
        return -1;
    }

    if (entry->path_len == 0 || entry->path_len >= path_size) {
        pack_set_error(error, error_size, "Embedded package entry path is invalid.");
        return -1;
    }

    if (pack_read_exact(file, path, entry->path_len) != 0) {
        pack_set_error(error, error_size, "Cannot read embedded package entry path.");
        return -1;
    }
    path[entry->path_len] = '\0';
    return 0;
}

int os_pack_read_manifest(const char *exe_path, OsPackageManifest *manifest, char *error, size_t error_size)
{
    FILE *file;
    OsPackHeader header;
    uint32_t i;
    int found_manifest = 0;

    if (error != NULL && error_size > 0) {
        error[0] = '\0';
    }

    if (pack_open(exe_path, &file, &header, error, error_size) != 0) {
        return -1;
    }

    os_manifest_init_defaults(manifest);
    for (i = 0; i < header.entry_count; ++i) {
        OsPackEntryHeader entry;
        char path[OS_MAX_PATH_LEN];

        if (pack_read_entry(file, header.version, &entry, path, sizeof(path), error, error_size) != 0) {
            fclose(file);
            return -1;
        }

        if (entry.type == OS_PACK_ENTRY_MANIFEST) {
            char *data;

            if (entry.size > 1024 * 1024) {
                fclose(file);
                pack_set_error(error, error_size, "Embedded manifest is too large.");
                return -1;
            }

            data = (char *)malloc((size_t)entry.size + 1);
            if (data == NULL) {
                fclose(file);
                pack_set_error(error, error_size, "Not enough memory to read embedded manifest.");
                return -1;
            }

            if (pack_read_exact(file, data, (size_t)entry.size) != 0) {
                free(data);
                fclose(file);
                pack_set_error(error, error_size, "Cannot read embedded manifest.");
                return -1;
            }

            if (os_read_manifest_buffer(data, (size_t)entry.size, manifest, error, error_size) != 0) {
                free(data);
                fclose(file);
                return -1;
            }

            free(data);
            found_manifest = 1;
        } else {
            if (entry.type == OS_PACK_ENTRY_LICENSE) {
                manifest->has_license = 1;
            } else if (entry.type == OS_PACK_ENTRY_WIZARD_IMAGE) {
                manifest->has_wizard_image = 1;
            } else if (entry.type == OS_PACK_ENTRY_BACKGROUND_IMAGE) {
                manifest->has_background_image = 1;
            }

            if (pack_skip_stream(file, entry.size) != 0) {
                fclose(file);
                pack_set_error(error, error_size, "Cannot read embedded package archive.");
                return -1;
            }
        }
    }

    fclose(file);
    if (!found_manifest) {
        pack_set_error(error, error_size, "Embedded package archive has no manifest.");
        return -1;
    }

    return 0;
}

int os_pack_read_info(const char *exe_path, OsPackageInfo *info, char *message, size_t message_size)
{
    OsPackageManifest manifest;
    char error[OS_MAX_MESSAGE_LEN];

    if (info == NULL) {
        pack_set_error(message, message_size, "Package info output is required.");
        return -1;
    }

    if (os_pack_read_manifest(exe_path, &manifest, error, sizeof(error)) != 0) {
        pack_copy_string(message, message_size, error);
        return -1;
    }

    os_manifest_to_info(&manifest, info);
    pack_set_error(message, message_size, "Loaded embedded package information.");
    return 0;
}

static int pack_extract_current_file(FILE *file,
                                     const OsPackEntryHeader *entry,
                                     const char *dst_path,
                                     char *error,
                                     size_t error_size)
{
    FILE *out;
    char parent[OS_MAX_PATH_LEN];

    if (pack_parent_dir(parent, sizeof(parent), dst_path) != 0 ||
        (parent[0] != '\0' && pack_mkdirs(parent) != 0)) {
        pack_set_error(error, error_size, "Cannot create destination folder for: %s", dst_path);
        return -1;
    }

    out = fopen(dst_path, "wb");
    if (out == NULL) {
        pack_set_error(error, error_size, "Cannot write extracted file: %s", dst_path);
        return -1;
    }

    if ((entry->flags & OS_PACK_ENTRY_FLAG_DEFLATE) != 0) {
#if defined(OPENSTALLER_HAVE_ZLIB)
        if (os_pack_inflate_stream(file, out, entry->size) != 0 || fclose(out) != 0) {
            pack_set_error(error, error_size, "Cannot decompress extracted file: %s", dst_path);
            return -1;
        }
#else
        fclose(out);
        pack_set_error(error, error_size, "This embedded payload requires zlib/deflate support.");
        return -1;
#endif
    } else if (pack_copy_stream(file, out, entry->size) != 0 || fclose(out) != 0) {
        pack_set_error(error, error_size, "Cannot write extracted file: %s", dst_path);
        return -1;
    }

    return 0;
}

static int pack_extract_entry_type(const char *exe_path,
                                   uint32_t type,
                                   const char *dst_path,
                                   char *message,
                                   size_t message_size)
{
    FILE *file;
    OsPackHeader header;
    char error[OS_MAX_MESSAGE_LEN];
    uint32_t i;

    error[0] = '\0';
    if (pack_open(exe_path, &file, &header, error, sizeof(error)) != 0) {
        pack_copy_string(message, message_size, error);
        return -1;
    }

    for (i = 0; i < header.entry_count; ++i) {
        OsPackEntryHeader entry;
        char path[OS_MAX_PATH_LEN];

        if (pack_read_entry(file, header.version, &entry, path, sizeof(path), error, sizeof(error)) != 0) {
            fclose(file);
            pack_copy_string(message, message_size, error);
            return -1;
        }

        if (entry.type == type) {
            if (pack_extract_current_file(file, &entry, dst_path, error, sizeof(error)) != 0) {
                fclose(file);
                pack_copy_string(message, message_size, error);
                return -1;
            }

            fclose(file);
            pack_set_error(message, message_size, "Extracted embedded resource.");
            return 0;
        }

        if (pack_skip_stream(file, entry.size) != 0) {
            fclose(file);
            pack_set_error(message, message_size, "Cannot read embedded package archive.");
            return -1;
        }
    }

    fclose(file);
    pack_set_error(message, message_size, "Embedded resource was not found.");
    return -1;
}

int os_pack_extract_payload_with_rollback(const char *exe_path,
                                          const char *dst_dir,
                                          OsRollback *rollback,
                                          size_t total_files,
                                          char *message,
                                          size_t message_size)
{
    FILE *file;
    OsPackHeader header;
    char error[OS_MAX_MESSAGE_LEN];
    uint32_t i;
    size_t extracted = 0;

    error[0] = '\0';
    if (pack_open(exe_path, &file, &header, error, sizeof(error)) != 0) {
        pack_copy_string(message, message_size, error);
        return -1;
    }

    if (pack_mkdirs(dst_dir) != 0) {
        fclose(file);
        pack_set_error(message, message_size, "Cannot create install directory: %s", dst_dir);
        return -1;
    }

    for (i = 0; i < header.entry_count; ++i) {
        OsPackEntryHeader entry;
        char path[OS_MAX_PATH_LEN];

        if (pack_read_entry(file, header.version, &entry, path, sizeof(path), error, sizeof(error)) != 0) {
            fclose(file);
            pack_copy_string(message, message_size, error);
            return -1;
        }

        if (entry.type == OS_PACK_ENTRY_PAYLOAD) {
            char dst_path[OS_MAX_PATH_LEN];

            if (pack_join_relative_path(dst_path, sizeof(dst_path), dst_dir, path) != 0) {
                fclose(file);
                pack_set_error(message, message_size, "Embedded payload path is unsafe: %s", path);
                return -1;
            }

            install_report_progress(OS_INSTALL_PROGRESS_FILE,
                                    "Extracting file",
                                    path,
                                    dst_path,
                                    extracted,
                                    total_files,
                                    install_progress_percent(extracted, total_files, 12, 82));

            if (os_rollback_capture_file(rollback, dst_path, error, sizeof(error)) != 0) {
                fclose(file);
                pack_copy_string(message, message_size, error);
                return -1;
            }

            if (pack_extract_current_file(file, &entry, dst_path, error, sizeof(error)) != 0) {
                fclose(file);
                pack_copy_string(message, message_size, error);
                return -1;
            }
            extracted++;
            install_report_progress(OS_INSTALL_PROGRESS_FILE,
                                    "Installed file",
                                    path,
                                    dst_path,
                                    extracted,
                                    total_files,
                                    install_progress_percent(extracted, total_files, 12, 82));
        } else if (pack_skip_stream(file, entry.size) != 0) {
            fclose(file);
            pack_set_error(message, message_size, "Cannot read embedded package archive.");
            return -1;
        }
    }

    fclose(file);
    if (extracted == 0) {
        pack_set_error(message, message_size, "Embedded package has no payload files.");
        return -1;
    }

    pack_set_error(message, message_size, "Extracted %llu embedded file(s).", (unsigned long long)extracted);
    return 0;
}

int os_pack_extract_payload(const char *exe_path, const char *dst_dir, char *message, size_t message_size)
{
    return os_pack_extract_payload_with_rollback(exe_path, dst_dir, NULL, 0, message, message_size);
}

int os_pack_extract_license(const char *exe_path, const char *dst_path, char *message, size_t message_size)
{
    return pack_extract_entry_type(exe_path, OS_PACK_ENTRY_LICENSE, dst_path, message, message_size);
}

int os_pack_extract_wizard_image(const char *exe_path, const char *dst_path, char *message, size_t message_size)
{
    return pack_extract_entry_type(exe_path, OS_PACK_ENTRY_WIZARD_IMAGE, dst_path, message, message_size);
}

int os_pack_extract_background_image(const char *exe_path, const char *dst_path, char *message, size_t message_size)
{
    return pack_extract_entry_type(exe_path, OS_PACK_ENTRY_BACKGROUND_IMAGE, dst_path, message, message_size);
}

int os_pack_extract_uninstaller(const char *exe_path, const char *dst_path, char *message, size_t message_size)
{
    return pack_extract_entry_type(exe_path, OS_PACK_ENTRY_UNINSTALLER, dst_path, message, message_size);
}
