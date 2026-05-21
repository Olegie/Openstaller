#include "win32_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(OPENSTALLER_WIN2000_COMPAT) && defined(CLEARTYPE_QUALITY)
#define RT_FONT_QUALITY CLEARTYPE_QUALITY
#elif defined(ANTIALIASED_QUALITY)
#define RT_FONT_QUALITY ANTIALIASED_QUALITY
#else
#define RT_FONT_QUALITY DEFAULT_QUALITY
#endif

int rt_is_sep(char ch)
{
    return ch == '/' || ch == '\\';
}

int rt_copy(char *dst, size_t dst_size, const char *src)
{
    size_t len = strlen(src);

    if (len >= dst_size) {
        return -1;
    }

    memcpy(dst, src, len + 1);
    return 0;
}

int rt_join(char *out, size_t out_size, const char *left, const char *right)
{
    size_t len;

    if (rt_copy(out, out_size, left) != 0) {
        return -1;
    }

    len = strlen(out);
    if (len > 0 && !rt_is_sep(out[len - 1])) {
        if (len + 2 >= out_size) {
            return -1;
        }
        out[len++] = '\\';
        out[len] = '\0';
    }

    if (len + strlen(right) >= out_size) {
        return -1;
    }

    strcat(out, right);
    return 0;
}

int rt_dirname(const char *path, char *out, size_t out_size)
{
    size_t len = strlen(path);

    while (len > 0 && !rt_is_sep(path[len - 1])) {
        --len;
    }

    while (len > 1 && rt_is_sep(path[len - 1])) {
        --len;
    }

    if (len == 0 || len >= out_size) {
        return -1;
    }

    memcpy(out, path, len);
    out[len] = '\0';
    return 0;
}

const char *rt_basename(const char *path)
{
    const char *base = path;

    while (*path != '\0') {
        if (rt_is_sep(*path)) {
            base = path + 1;
        }
        ++path;
    }

    return base;
}

int rt_ieq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;

        if (ca >= 'A' && ca <= 'Z') {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return 0;
        }

        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

static int rt_hex_digit(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

COLORREF rt_theme_color(const char *value, COLORREF fallback)
{
    int r1;
    int r2;
    int g1;
    int g2;
    int b1;
    int b2;

    if (value == NULL || value[0] == '\0') {
        return fallback;
    }
    if (value[0] == '#') {
        ++value;
    }
    if (strlen(value) != 6) {
        return fallback;
    }

    r1 = rt_hex_digit(value[0]);
    r2 = rt_hex_digit(value[1]);
    g1 = rt_hex_digit(value[2]);
    g2 = rt_hex_digit(value[3]);
    b1 = rt_hex_digit(value[4]);
    b2 = rt_hex_digit(value[5]);
    if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0) {
        return fallback;
    }

    return RGB((r1 << 4) | r2, (g1 << 4) | g2, (b1 << 4) | b2);
}

static int rt_font_height(int points)
{
    HDC dc = GetDC(NULL);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    return -MulDiv(points, dpi, 72);
}

HFONT rt_font(int points, int weight)
{
    const char *face = g_rt.info.ui_font[0] != '\0'
                           ? g_rt.info.ui_font
                           : g_rt.info.installer_style == OS_INSTALLER_STYLE_MODERN ? "Segoe UI" : "MS Shell Dlg";

    return CreateFontA(rt_font_height(points),
                       0,
                       0,
                       0,
                       weight,
                       FALSE,
                       FALSE,
                       FALSE,
                       DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS,
                       RT_FONT_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE,
                       face);
}

void rt_set_font(HWND hwnd, HFONT font)
{
    SendMessageA(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
}

void rt_show(HWND hwnd, int visible)
{
    ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
}

int rt_load_text_file(const char *path, HWND target)
{
    FILE *file = fopen(path, "rb");
    long size;
    char *data;

    if (file == NULL) {
        return -1;
    }

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0 || size > 65536) {
        fclose(file);
        return -1;
    }

    data = (char *)malloc((size_t)size + 1);
    if (data == NULL) {
        fclose(file);
        return -1;
    }

    fread(data, 1, (size_t)size, file);
    data[size] = '\0';
    fclose(file);
    SetWindowTextA(target, data);
    free(data);
    return 0;
}
