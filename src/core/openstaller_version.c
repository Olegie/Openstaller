#include "openstaller_version.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(_WIN32)

typedef struct VersionBuffer {
    unsigned char *data;
    size_t size;
    size_t capacity;
} VersionBuffer;

static void version_set_error(char *buffer, size_t size, const char *format, ...)
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

static int version_reserve(VersionBuffer *buffer, size_t extra)
{
    unsigned char *next;
    size_t capacity;

    if (extra > (size_t)-1 - buffer->size) {
        return -1;
    }

    if (buffer->size + extra <= buffer->capacity) {
        return 0;
    }

    capacity = buffer->capacity == 0 ? 1024u : buffer->capacity;
    while (capacity < buffer->size + extra) {
        if (capacity > (size_t)-1 / 2u) {
            return -1;
        }
        capacity *= 2u;
    }

    next = (unsigned char *)realloc(buffer->data, capacity);
    if (next == NULL) {
        return -1;
    }

    buffer->data = next;
    buffer->capacity = capacity;
    return 0;
}

static int version_write(VersionBuffer *buffer, const void *data, size_t size)
{
    if (version_reserve(buffer, size) != 0) {
        return -1;
    }
    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
    return 0;
}

static int version_write_u16(VersionBuffer *buffer, uint16_t value)
{
    return version_write(buffer, &value, sizeof(value));
}

static int version_write_wstr(VersionBuffer *buffer, const WCHAR *value)
{
    size_t chars = (size_t)lstrlenW(value) + 1u;
    return version_write(buffer, value, chars * sizeof(WCHAR));
}

static int version_align4(VersionBuffer *buffer)
{
    static const unsigned char zero[4] = {0, 0, 0, 0};
    size_t pad = (4u - (buffer->size & 3u)) & 3u;
    return pad == 0 ? 0 : version_write(buffer, zero, pad);
}

static void version_patch_u16(VersionBuffer *buffer, size_t offset, uint16_t value)
{
    memcpy(buffer->data + offset, &value, sizeof(value));
}

static WCHAR *version_to_wide(const char *text)
{
    WCHAR *wide;
    int needed;

    if (text == NULL) {
        text = "";
    }

    needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (needed <= 0) {
        needed = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
    }
    if (needed <= 0) {
        needed = 1;
    }

    wide = (WCHAR *)calloc((size_t)needed, sizeof(WCHAR));
    if (wide == NULL) {
        return NULL;
    }

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, needed) <= 0 &&
        MultiByteToWideChar(CP_ACP, 0, text, -1, wide, needed) <= 0) {
        wide[0] = L'\0';
    }

    return wide;
}

static int version_parse_numbers(const char *version, WORD parts[4])
{
    int index = 0;
    const char *cursor = version;

    parts[0] = 0;
    parts[1] = 0;
    parts[2] = 0;
    parts[3] = 0;

    while (cursor != NULL && *cursor != '\0' && index < 4) {
        unsigned long value = 0;
        int saw_digit = 0;

        while (*cursor != '\0' && (*cursor < '0' || *cursor > '9')) {
            ++cursor;
        }
        while (*cursor >= '0' && *cursor <= '9') {
            saw_digit = 1;
            value = value * 10u + (unsigned long)(*cursor - '0');
            if (value > 65535u) {
                value = 65535u;
            }
            ++cursor;
        }
        if (!saw_digit) {
            break;
        }
        parts[index++] = (WORD)value;
    }

    return index > 0 ? 0 : -1;
}

static int version_write_string(VersionBuffer *buffer, const WCHAR *key, const WCHAR *value)
{
    size_t start = buffer->size;
    WORD value_len = (WORD)((size_t)lstrlenW(value) + 1u);

    if (version_write_u16(buffer, 0) != 0 ||
        version_write_u16(buffer, value_len) != 0 ||
        version_write_u16(buffer, 1) != 0 ||
        version_write_wstr(buffer, key) != 0 ||
        version_align4(buffer) != 0 ||
        version_write_wstr(buffer, value) != 0 ||
        version_align4(buffer) != 0) {
        return -1;
    }

    version_patch_u16(buffer, start, (uint16_t)(buffer->size - start));
    return 0;
}

static int version_begin_block(VersionBuffer *buffer, const WCHAR *key, WORD value_len, WORD type, size_t *start)
{
    *start = buffer->size;
    return version_write_u16(buffer, 0) == 0 &&
           version_write_u16(buffer, value_len) == 0 &&
           version_write_u16(buffer, type) == 0 &&
           version_write_wstr(buffer, key) == 0 &&
           version_align4(buffer) == 0
               ? 0
               : -1;
}

static void version_end_block(VersionBuffer *buffer, size_t start)
{
    version_align4(buffer);
    version_patch_u16(buffer, start, (uint16_t)(buffer->size - start));
}

static int version_build_description(WCHAR *out,
                                     size_t out_chars,
                                     const WCHAR *app_name,
                                     int uninstall_exe)
{
    const WCHAR *suffix = uninstall_exe ? L" Uninstaller" : L" Setup";
    int needed = lstrlenW(app_name) + lstrlenW(suffix) + 1;

    if ((size_t)needed > out_chars) {
        return -1;
    }

    lstrcpyW(out, app_name);
    lstrcatW(out, suffix);
    return 0;
}

static int version_build_resource(const OsProjectConfig *config,
                                  int uninstall_exe,
                                  VersionBuffer *buffer,
                                  char *error,
                                  size_t error_size)
{
    VS_FIXEDFILEINFO fixed;
    WORD parts[4];
    WCHAR *app_name = NULL;
    WCHAR *company = NULL;
    WCHAR *version = NULL;
    WCHAR description[OS_MAX_NAME_LEN + 32];
    WCHAR filename[32];
    size_t root;
    size_t string_file_info;
    size_t table;
    size_t var_file_info;
    size_t translation;
    DWORD translation_value;
    int result = -1;

    app_name = version_to_wide(config->app_name[0] != '\0' ? config->app_name : "Application");
    company = version_to_wide(config->company_name[0] != '\0' ? config->company_name : config->app_name);
    version = version_to_wide(config->app_version[0] != '\0' ? config->app_version : "0.0.0");
    if (app_name == NULL || company == NULL || version == NULL) {
        version_set_error(error, error_size, "Not enough memory to build Windows version metadata.");
        goto done;
    }

    if (version_build_description(description, sizeof(description) / sizeof(description[0]), app_name, uninstall_exe) != 0) {
        version_set_error(error, error_size, "Product name is too long for Windows version metadata.");
        goto done;
    }
    lstrcpyW(filename, uninstall_exe ? L"uninstaller.exe" : L"installer.exe");

    if (version_parse_numbers(config->app_version, parts) != 0) {
        parts[0] = 0;
        parts[1] = 0;
        parts[2] = 0;
        parts[3] = 0;
    }

    memset(&fixed, 0, sizeof(fixed));
    fixed.dwSignature = 0xFEEF04BD;
    fixed.dwStrucVersion = 0x00010000;
    fixed.dwFileVersionMS = ((DWORD)parts[0] << 16) | parts[1];
    fixed.dwFileVersionLS = ((DWORD)parts[2] << 16) | parts[3];
    fixed.dwProductVersionMS = fixed.dwFileVersionMS;
    fixed.dwProductVersionLS = fixed.dwFileVersionLS;
    fixed.dwFileFlagsMask = 0x3f;
    fixed.dwFileOS = VOS_NT_WINDOWS32;
    fixed.dwFileType = VFT_APP;

    if (version_begin_block(buffer, L"VS_VERSION_INFO", (WORD)sizeof(fixed), 0, &root) != 0 ||
        version_write(buffer, &fixed, sizeof(fixed)) != 0 ||
        version_align4(buffer) != 0 ||
        version_begin_block(buffer, L"StringFileInfo", 0, 1, &string_file_info) != 0 ||
        version_begin_block(buffer, L"040904b0", 0, 1, &table) != 0 ||
        version_write_string(buffer, L"CompanyName", company) != 0 ||
        version_write_string(buffer, L"FileDescription", description) != 0 ||
        version_write_string(buffer, L"FileVersion", version) != 0 ||
        version_write_string(buffer, L"InternalName", uninstall_exe ? L"openstaller-generated-uninstaller" : L"openstaller-generated-installer") != 0 ||
        version_write_string(buffer, L"LegalCopyright", company) != 0 ||
        version_write_string(buffer, L"OriginalFilename", filename) != 0 ||
        version_write_string(buffer, L"ProductName", app_name) != 0 ||
        version_write_string(buffer, L"ProductVersion", version) != 0) {
        version_set_error(error, error_size, "Cannot build Windows version metadata.");
        goto done;
    }

    version_end_block(buffer, table);
    version_end_block(buffer, string_file_info);

    if (version_begin_block(buffer, L"VarFileInfo", 0, 1, &var_file_info) != 0 ||
        version_begin_block(buffer, L"Translation", (WORD)sizeof(translation_value), 0, &translation) != 0) {
        version_set_error(error, error_size, "Cannot build Windows version metadata.");
        goto done;
    }
    translation_value = ((DWORD)1200u << 16) | 0x0409u;
    if (version_write(buffer, &translation_value, sizeof(translation_value)) != 0) {
        version_set_error(error, error_size, "Cannot build Windows version metadata.");
        goto done;
    }

    version_end_block(buffer, translation);
    version_end_block(buffer, var_file_info);
    version_end_block(buffer, root);
    result = 0;

done:
    free(app_name);
    free(company);
    free(version);
    return result;
}

int os_version_apply_to_exe(const char *exe_path,
                            const OsProjectConfig *config,
                            int uninstall_exe,
                            char *error,
                            size_t error_size)
{
    VersionBuffer buffer;
    HANDLE update;
    WORD language = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
    int ok;

    if (error != NULL && error_size > 0) {
        error[0] = '\0';
    }

    if (exe_path == NULL || exe_path[0] == '\0' || config == NULL) {
        version_set_error(error, error_size, "EXE path and project config are required for version metadata.");
        return -1;
    }

    memset(&buffer, 0, sizeof(buffer));
    if (version_build_resource(config, uninstall_exe, &buffer, error, error_size) != 0) {
        free(buffer.data);
        return -1;
    }

    update = BeginUpdateResourceA(exe_path, FALSE);
    if (update == NULL) {
        free(buffer.data);
        version_set_error(error, error_size, "Cannot open EXE resources for version update: %s", exe_path);
        return -1;
    }

    ok = UpdateResourceA(update,
                         RT_VERSION,
                         MAKEINTRESOURCEA(VS_VERSION_INFO),
                         language,
                         buffer.data,
                         (DWORD)buffer.size) != 0;
    if (!EndUpdateResourceA(update, ok ? FALSE : TRUE)) {
        free(buffer.data);
        version_set_error(error, error_size, "Cannot save EXE version resources: %s", exe_path);
        return -1;
    }

    free(buffer.data);
    if (!ok) {
        version_set_error(error, error_size, "Cannot write EXE version resources: %s", exe_path);
        return -1;
    }

    return 0;
}

#else

int os_version_apply_to_exe(const char *exe_path,
                            const OsProjectConfig *config,
                            int uninstall_exe,
                            char *error,
                            size_t error_size)
{
    (void)exe_path;
    (void)config;
    (void)uninstall_exe;
    if (error != NULL && error_size > 0) {
        error[0] = '\0';
    }
    return 0;
}

#endif
