#include "openstaller_icon.h"

#include "openstaller/openstaller.h"
#include "openstaller_icon_internal.h"

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

typedef struct IconCanvas {
    int w;
    int h;
    unsigned char *rgba;
} IconCanvas;

static void icon_set_error(char *buffer, size_t size, const char *format, ...)
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

static void icon_write_u16(unsigned char *data, uint16_t value)
{
    data[0] = (unsigned char)(value & 0xffu);
    data[1] = (unsigned char)((value >> 8) & 0xffu);
}

static void icon_write_u32(unsigned char *data, uint32_t value)
{
    data[0] = (unsigned char)(value & 0xffu);
    data[1] = (unsigned char)((value >> 8) & 0xffu);
    data[2] = (unsigned char)((value >> 16) & 0xffu);
    data[3] = (unsigned char)((value >> 24) & 0xffu);
}

static void icon_blend(IconCanvas *canvas, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    unsigned char *dst;
    unsigned int inv;
    unsigned int out_a;

    if (x < 0 || y < 0 || x >= canvas->w || y >= canvas->h || a == 0) {
        return;
    }

    dst = canvas->rgba + ((size_t)y * (size_t)canvas->w + (size_t)x) * 4u;
    inv = 255u - a;
    out_a = a + (unsigned int)dst[3] * inv / 255u;
    dst[0] = (unsigned char)(((unsigned int)r * a + (unsigned int)dst[0] * inv) / 255u);
    dst[1] = (unsigned char)(((unsigned int)g * a + (unsigned int)dst[1] * inv) / 255u);
    dst[2] = (unsigned char)(((unsigned int)b * a + (unsigned int)dst[2] * inv) / 255u);
    dst[3] = (unsigned char)(out_a > 255u ? 255u : out_a);
}

static int icon_point_in_poly(double x, double y, const double *points, int count)
{
    int inside = 0;
    int i;
    int j = count - 1;

    for (i = 0; i < count; ++i) {
        double xi = points[i * 2];
        double yi = points[i * 2 + 1];
        double xj = points[j * 2];
        double yj = points[j * 2 + 1];

        if (((yi > y) != (yj > y)) &&
            x < (xj - xi) * (y - yi) / (yj - yi + 0.000001) + xi) {
            inside = !inside;
        }
        j = i;
    }

    return inside;
}

static void icon_poly(IconCanvas *canvas,
                      const double *points,
                      int count,
                      unsigned char r,
                      unsigned char g,
                      unsigned char b,
                      unsigned char a)
{
    int x;
    int y;
    int sx;
    int sy;

    for (y = 0; y < canvas->h; ++y) {
        for (x = 0; x < canvas->w; ++x) {
            int cover = 0;
            for (sy = 0; sy < 2; ++sy) {
                for (sx = 0; sx < 2; ++sx) {
                    double nx = ((double)x + 0.25 + 0.5 * (double)sx) / (double)canvas->w;
                    double ny = ((double)y + 0.25 + 0.5 * (double)sy) / (double)canvas->h;
                    if (icon_point_in_poly(nx, ny, points, count)) {
                        ++cover;
                    }
                }
            }
            if (cover > 0) {
                icon_blend(canvas, x, y, r, g, b, (unsigned char)((int)a * cover / 4));
            }
        }
    }
}

static unsigned char icon_lerp_byte(unsigned char a, unsigned char b, double t)
{
    if (t < 0.0) {
        t = 0.0;
    } else if (t > 1.0) {
        t = 1.0;
    }
    return (unsigned char)((double)a + ((double)b - (double)a) * t);
}

static void icon_gradient_color(double y,
                                double top,
                                double bottom,
                                unsigned char r1,
                                unsigned char g1,
                                unsigned char b1,
                                unsigned char r2,
                                unsigned char g2,
                                unsigned char b2,
                                unsigned char *r,
                                unsigned char *g,
                                unsigned char *b)
{
    double t = (y - top) / (bottom - top + 0.000001);

    *r = icon_lerp_byte(r1, r2, t);
    *g = icon_lerp_byte(g1, g2, t);
    *b = icon_lerp_byte(b1, b2, t);
}

static void icon_poly_gradient(IconCanvas *canvas,
                               const double *points,
                               int count,
                               unsigned char r1,
                               unsigned char g1,
                               unsigned char b1,
                               unsigned char r2,
                               unsigned char g2,
                               unsigned char b2,
                               unsigned char a)
{
    int x;
    int y;
    int sx;
    int sy;

    for (y = 0; y < canvas->h; ++y) {
        for (x = 0; x < canvas->w; ++x) {
            int cover = 0;
            double ny_mid = ((double)y + 0.5) / (double)canvas->h;
            for (sy = 0; sy < 2; ++sy) {
                for (sx = 0; sx < 2; ++sx) {
                    double nx = ((double)x + 0.25 + 0.5 * (double)sx) / (double)canvas->w;
                    double ny = ((double)y + 0.25 + 0.5 * (double)sy) / (double)canvas->h;
                    if (icon_point_in_poly(nx, ny, points, count)) {
                        ++cover;
                    }
                }
            }
            if (cover > 0) {
                unsigned char r;
                unsigned char g;
                unsigned char b;
                icon_gradient_color(ny_mid, 0.0, 1.0, r1, g1, b1, r2, g2, b2, &r, &g, &b);
                icon_blend(canvas, x, y, r, g, b, (unsigned char)((int)a * cover / 4));
            }
        }
    }
}

static void icon_ellipse_gradient(IconCanvas *canvas,
                                  double x0,
                                  double y0,
                                  double x1,
                                  double y1,
                                  unsigned char r1,
                                  unsigned char g1,
                                  unsigned char b1,
                                  unsigned char r2,
                                  unsigned char g2,
                                  unsigned char b2,
                                  unsigned char a)
{
    int x;
    int y;
    int sx;
    int sy;
    double cx = (x0 + x1) * 0.5;
    double cy = (y0 + y1) * 0.5;
    double rx = (x1 - x0) * 0.5;
    double ry = (y1 - y0) * 0.5;

    for (y = 0; y < canvas->h; ++y) {
        for (x = 0; x < canvas->w; ++x) {
            int cover = 0;
            double ny_mid = ((double)y + 0.5) / (double)canvas->h;
            for (sy = 0; sy < 2; ++sy) {
                for (sx = 0; sx < 2; ++sx) {
                    double nx = ((double)x + 0.25 + 0.5 * (double)sx) / (double)canvas->w;
                    double ny = ((double)y + 0.25 + 0.5 * (double)sy) / (double)canvas->h;
                    double dx = (nx - cx) / rx;
                    double dy = (ny - cy) / ry;
                    if (dx * dx + dy * dy <= 1.0) {
                        ++cover;
                    }
                }
            }
            if (cover > 0) {
                unsigned char r;
                unsigned char g;
                unsigned char b;
                icon_gradient_color(ny_mid, y0, y1, r1, g1, b1, r2, g2, b2, &r, &g, &b);
                icon_blend(canvas, x, y, r, g, b, (unsigned char)((int)a * cover / 4));
            }
        }
    }
}

static int icon_in_round_rect(double x, double y, double x0, double y0, double x1, double y1, double radius)
{
    double cx;
    double cy;
    double dx;
    double dy;

    if (x < x0 || y < y0 || x > x1 || y > y1) {
        return 0;
    }

    cx = x;
    cy = y;
    if (cx < x0 + radius) {
        cx = x0 + radius;
    } else if (cx > x1 - radius) {
        cx = x1 - radius;
    }
    if (cy < y0 + radius) {
        cy = y0 + radius;
    } else if (cy > y1 - radius) {
        cy = y1 - radius;
    }

    dx = x - cx;
    dy = y - cy;
    return dx * dx + dy * dy <= radius * radius;
}

static void icon_round_rect_gradient(IconCanvas *canvas,
                                     double x0,
                                     double y0,
                                     double x1,
                                     double y1,
                                     double radius,
                                     unsigned char r1,
                                     unsigned char g1,
                                     unsigned char b1,
                                     unsigned char r2,
                                     unsigned char g2,
                                     unsigned char b2,
                                     unsigned char a)
{
    int x;
    int y;
    int sx;
    int sy;

    for (y = 0; y < canvas->h; ++y) {
        for (x = 0; x < canvas->w; ++x) {
            int cover = 0;
            double ny_mid = ((double)y + 0.5) / (double)canvas->h;
            for (sy = 0; sy < 2; ++sy) {
                for (sx = 0; sx < 2; ++sx) {
                    double nx = ((double)x + 0.25 + 0.5 * (double)sx) / (double)canvas->w;
                    double ny = ((double)y + 0.25 + 0.5 * (double)sy) / (double)canvas->h;
                    if (icon_in_round_rect(nx, ny, x0, y0, x1, y1, radius)) {
                        ++cover;
                    }
                }
            }
            if (cover > 0) {
                unsigned char r;
                unsigned char g;
                unsigned char b;
                icon_gradient_color(ny_mid, y0, y1, r1, g1, b1, r2, g2, b2, &r, &g, &b);
                icon_blend(canvas, x, y, r, g, b, (unsigned char)((int)a * cover / 4));
            }
        }
    }
}

static double icon_segment_distance_sq(double px, double py, double ax, double ay, double bx, double by)
{
    double dx = bx - ax;
    double dy = by - ay;
    double len_sq = dx * dx + dy * dy;
    double t;
    double cx;
    double cy;

    if (len_sq <= 0.000001) {
        dx = px - ax;
        dy = py - ay;
        return dx * dx + dy * dy;
    }

    t = ((px - ax) * dx + (py - ay) * dy) / len_sq;
    if (t < 0.0) {
        t = 0.0;
    } else if (t > 1.0) {
        t = 1.0;
    }

    cx = ax + t * dx;
    cy = ay + t * dy;
    dx = px - cx;
    dy = py - cy;
    return dx * dx + dy * dy;
}

static void icon_line(IconCanvas *canvas,
                      double ax,
                      double ay,
                      double bx,
                      double by,
                      double radius,
                      unsigned char r,
                      unsigned char g,
                      unsigned char b,
                      unsigned char a)
{
    int x;
    int y;
    int sx;
    int sy;
    double radius_sq = radius * radius;
    double feather_sq = (radius + 0.012) * (radius + 0.012);

    for (y = 0; y < canvas->h; ++y) {
        for (x = 0; x < canvas->w; ++x) {
            int cover = 0;
            for (sy = 0; sy < 2; ++sy) {
                for (sx = 0; sx < 2; ++sx) {
                    double nx = ((double)x + 0.25 + 0.5 * (double)sx) / (double)canvas->w;
                    double ny = ((double)y + 0.25 + 0.5 * (double)sy) / (double)canvas->h;
                    double dist_sq = icon_segment_distance_sq(nx, ny, ax, ay, bx, by);
                    if (dist_sq <= feather_sq) {
                        cover += dist_sq <= radius_sq ? 2 : 1;
                    }
                }
            }
            if (cover > 0) {
                if (cover > 4) {
                    cover = 4;
                }
                icon_blend(canvas, x, y, r, g, b, (unsigned char)((int)a * cover / 4));
            }
        }
    }
}

static void icon_draw_box(IconCanvas *canvas)
{
    const double cube_top[] = {0.33, 0.36, 0.49, 0.27, 0.65, 0.36, 0.49, 0.45};
    const double cube_left[] = {0.33, 0.37, 0.49, 0.46, 0.49, 0.64, 0.33, 0.55};
    const double cube_right[] = {0.49, 0.46, 0.65, 0.37, 0.65, 0.55, 0.49, 0.64};
    const double gloss[] = {0.20, 0.16, 0.72, 0.16, 0.72, 0.36, 0.62, 0.42, 0.24, 0.34};

    icon_ellipse_gradient(canvas, 0.20, 0.72, 0.82, 0.91, 0, 0, 0, 0, 0, 0, 36);
    icon_round_rect_gradient(canvas, 0.12, 0.09, 0.80, 0.80, 0.15, 13, 44, 104, 19, 76, 157, 255);
    icon_round_rect_gradient(canvas, 0.16, 0.13, 0.76, 0.76, 0.12, 93, 163, 240, 40, 94, 190, 255);
    icon_poly_gradient(canvas, gloss, 5, 255, 255, 255, 255, 255, 255, 42);

    icon_line(canvas, 0.28, 0.70, 0.68, 0.70, 0.010, 17, 54, 124, 130);
    icon_poly(canvas, cube_top, 4, 24, 66, 148, 210);
    icon_poly_gradient(canvas, cube_top, 4, 240, 248, 255, 178, 211, 255, 210);
    icon_poly(canvas, cube_left, 4, 24, 66, 148, 200);
    icon_poly_gradient(canvas, cube_left, 4, 198, 226, 255, 126, 178, 243, 215);
    icon_poly(canvas, cube_right, 4, 24, 66, 148, 200);
    icon_poly_gradient(canvas, cube_right, 4, 170, 207, 255, 72, 132, 224, 230);
    icon_line(canvas, 0.49, 0.46, 0.49, 0.64, 0.006, 255, 255, 255, 85);
}

static void icon_draw_arrow(IconCanvas *canvas)
{
    const double arrow_shadow[] = {0.66, 0.57, 0.75, 0.57, 0.75, 0.68, 0.81, 0.68, 0.705, 0.80, 0.60, 0.68, 0.66, 0.68};
    const double arrow[] = {0.67, 0.55, 0.74, 0.55, 0.74, 0.66, 0.80, 0.66, 0.705, 0.78, 0.61, 0.66, 0.67, 0.66};

    icon_ellipse_gradient(canvas, 0.52, 0.49, 0.90, 0.87, 15, 91, 33, 10, 121, 50, 255);
    icon_ellipse_gradient(canvas, 0.56, 0.53, 0.86, 0.83, 113, 224, 92, 42, 168, 64, 255);
    icon_ellipse_gradient(canvas, 0.59, 0.55, 0.83, 0.68, 255, 255, 255, 255, 255, 255, 45);
    icon_poly(canvas, arrow_shadow, 7, 18, 91, 35, 95);
    icon_poly(canvas, arrow, 7, 255, 255, 255, 245);
}

static void icon_draw_cross(IconCanvas *canvas)
{
    icon_ellipse_gradient(canvas, 0.52, 0.49, 0.90, 0.87, 142, 20, 14, 110, 9, 7, 255);
    icon_ellipse_gradient(canvas, 0.56, 0.53, 0.86, 0.83, 251, 80, 66, 214, 38, 31, 255);
    icon_ellipse_gradient(canvas, 0.59, 0.55, 0.83, 0.68, 255, 255, 255, 255, 255, 255, 34);
    icon_line(canvas, 0.62, 0.61, 0.80, 0.79, 0.050, 120, 16, 13, 125);
    icon_line(canvas, 0.80, 0.61, 0.62, 0.79, 0.050, 120, 16, 13, 125);
    icon_line(canvas, 0.63, 0.62, 0.79, 0.78, 0.031, 255, 255, 255, 245);
    icon_line(canvas, 0.79, 0.62, 0.63, 0.78, 0.031, 255, 255, 255, 245);
}

static int icon_downsample_canvas(IconCanvas *dst, const IconCanvas *src, int dst_size)
{
    int scale = src->w / dst_size;
    int x;
    int y;

    if (scale < 1 || src->w != src->h || src->w % dst_size != 0) {
        return -1;
    }

    dst->w = dst_size;
    dst->h = dst_size;
    dst->rgba = (unsigned char *)calloc((size_t)dst_size * (size_t)dst_size, 4u);
    if (dst->rgba == NULL) {
        return -1;
    }

    for (y = 0; y < dst_size; ++y) {
        for (x = 0; x < dst_size; ++x) {
            unsigned int sum_a = 0;
            unsigned int sum_r = 0;
            unsigned int sum_g = 0;
            unsigned int sum_b = 0;
            int sx;
            int sy;
            unsigned char *out = dst->rgba + ((size_t)y * (size_t)dst_size + (size_t)x) * 4u;

            for (sy = 0; sy < scale; ++sy) {
                for (sx = 0; sx < scale; ++sx) {
                    const unsigned char *in = src->rgba +
                        ((size_t)(y * scale + sy) * (size_t)src->w + (size_t)(x * scale + sx)) * 4u;
                    unsigned int a = in[3];

                    sum_a += a;
                    sum_r += (unsigned int)in[0] * a;
                    sum_g += (unsigned int)in[1] * a;
                    sum_b += (unsigned int)in[2] * a;
                }
            }

            out[3] = (unsigned char)(sum_a / (unsigned int)(scale * scale));
            if (sum_a > 0) {
                out[0] = (unsigned char)(sum_r / sum_a);
                out[1] = (unsigned char)(sum_g / sum_a);
                out[2] = (unsigned char)(sum_b / sum_a);
            }
        }
    }

    return 0;
}

static int icon_render_default_canvas(IconCanvas *canvas, int size, int uninstall_icon)
{
    IconCanvas hi;
    int scale = size <= 64 ? 4 : size <= 128 ? 3 : 2;

    memset(canvas, 0, sizeof(*canvas));
    memset(&hi, 0, sizeof(hi));
    hi.w = size * scale;
    hi.h = size * scale;
    hi.rgba = (unsigned char *)calloc((size_t)hi.w * (size_t)hi.h, 4u);
    if (hi.rgba == NULL) {
        return -1;
    }

    icon_draw_box(&hi);
    if (uninstall_icon) {
        icon_draw_cross(&hi);
    } else {
        icon_draw_arrow(&hi);
    }

    if (icon_downsample_canvas(canvas, &hi, size) != 0) {
        free(hi.rgba);
        return -1;
    }

    free(hi.rgba);
    return 0;
}

static int icon_dib_from_canvas(const IconCanvas *canvas, unsigned char **data_out, uint32_t *size_out)
{
    uint32_t w = (uint32_t)canvas->w;
    uint32_t h = (uint32_t)canvas->h;
    uint32_t pixel_bytes = w * h * 4u;
    uint32_t mask_stride = ((w + 31u) / 32u) * 4u;
    uint32_t mask_bytes = mask_stride * h;
    uint32_t total = 40u + pixel_bytes + mask_bytes;
    unsigned char *data = (unsigned char *)calloc(1, total);
    uint32_t x;
    uint32_t y;

    if (data == NULL) {
        return -1;
    }

    icon_write_u32(data, 40u);
    icon_write_u32(data + 4, w);
    icon_write_u32(data + 8, h * 2u);
    icon_write_u16(data + 12, 1u);
    icon_write_u16(data + 14, 32u);
    icon_write_u32(data + 16, 0u);
    icon_write_u32(data + 20, pixel_bytes + mask_bytes);

    for (y = 0; y < h; ++y) {
        const unsigned char *src = canvas->rgba + ((size_t)(h - 1u - y) * w) * 4u;
        unsigned char *dst = data + 40u + (size_t)y * w * 4u;
        for (x = 0; x < w; ++x) {
            dst[x * 4u + 0u] = src[x * 4u + 2u];
            dst[x * 4u + 1u] = src[x * 4u + 1u];
            dst[x * 4u + 2u] = src[x * 4u + 0u];
            dst[x * 4u + 3u] = src[x * 4u + 3u];
        }
    }

    *data_out = data;
    *size_out = total;
    return 0;
}

static int icon_make_default_images(int uninstall_icon, IconImage **images_out, uint16_t *count_out)
{
    static const int sizes[] = {16, 24, 32, 48, 64, 128, 256};
    const uint16_t count = (uint16_t)(sizeof(sizes) / sizeof(sizes[0]));
    IconImage *images = (IconImage *)calloc(count, sizeof(*images));
    uint16_t i;

    if (images == NULL) {
        return -1;
    }

    for (i = 0; i < count; ++i) {
        IconCanvas canvas;
        uint32_t dib_size;
        unsigned char *dib;

        if (icon_render_default_canvas(&canvas, sizes[i], uninstall_icon) != 0) {
            free(images);
            return -1;
        }

        if (icon_dib_from_canvas(&canvas, &dib, &dib_size) != 0) {
            free(canvas.rgba);
            free(images);
            return -1;
        }

        free(canvas.rgba);
        images[i].width = sizes[i] == 256 ? 0 : (unsigned char)sizes[i];
        images[i].height = sizes[i] == 256 ? 0 : (unsigned char)sizes[i];
        images[i].color_count = 0;
        images[i].planes = 1;
        images[i].bit_count = 32;
        images[i].size = dib_size;
        images[i].data = dib;
        images[i].owned = dib;
    }

    *images_out = images;
    *count_out = count;
    return 0;
}

HICON os_icon_create_default_hicon(int size, int uninstall_icon)
{
    IconCanvas canvas;
    BITMAPV5HEADER header;
    ICONINFO info;
    HBITMAP color = NULL;
    HBITMAP mask = NULL;
    HDC dc = NULL;
    unsigned char *bits = NULL;
    unsigned char *mask_bits = NULL;
    HICON icon = NULL;
    int x;
    int y;

    if (size < 16) {
        size = 16;
    } else if (size > 256) {
        size = 256;
    }

    if (icon_render_default_canvas(&canvas, size, uninstall_icon) != 0) {
        return NULL;
    }

    memset(&header, 0, sizeof(header));
    header.bV5Size = sizeof(header);
    header.bV5Width = size;
    header.bV5Height = -size;
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00ff0000;
    header.bV5GreenMask = 0x0000ff00;
    header.bV5BlueMask = 0x000000ff;
    header.bV5AlphaMask = 0xff000000;

    dc = GetDC(NULL);
    if (dc == NULL) {
        free(canvas.rgba);
        return NULL;
    }

    color = CreateDIBSection(dc, (BITMAPINFO *)&header, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    ReleaseDC(NULL, dc);
    if (color == NULL || bits == NULL) {
        DeleteObject(color);
        free(canvas.rgba);
        return NULL;
    }

    for (y = 0; y < size; ++y) {
        for (x = 0; x < size; ++x) {
            const unsigned char *src = canvas.rgba + ((size_t)y * (size_t)size + (size_t)x) * 4u;
            unsigned char *dst = bits + ((size_t)y * (size_t)size + (size_t)x) * 4u;
            unsigned int alpha = src[3];

            dst[0] = (unsigned char)((unsigned int)src[2] * alpha / 255u);
            dst[1] = (unsigned char)((unsigned int)src[1] * alpha / 255u);
            dst[2] = (unsigned char)((unsigned int)src[0] * alpha / 255u);
            dst[3] = src[3];
        }
    }

    mask_bits = (unsigned char *)calloc(((size + 31) / 32) * 4, (size_t)size);
    if (mask_bits == NULL) {
        DeleteObject(color);
        free(canvas.rgba);
        return NULL;
    }

    mask = CreateBitmap(size, size, 1, 1, mask_bits);
    free(mask_bits);
    if (mask == NULL) {
        DeleteObject(color);
        free(canvas.rgba);
        return NULL;
    }

    memset(&info, 0, sizeof(info));
    info.fIcon = TRUE;
    info.hbmColor = color;
    info.hbmMask = mask;
    icon = CreateIconIndirect(&info);

    DeleteObject(mask);
    DeleteObject(color);
    free(canvas.rgba);
    return icon;
}

int os_icon_apply_to_exe(const char *exe_path,
                         const char *ico_path,
                         int uninstall_icon,
                         char *error,
                         size_t error_size)
{
    unsigned char *ico_data = NULL;
    size_t ico_size = 0;
    IconImage *images = NULL;
    uint16_t count = 0;
    HANDLE update;
    int ok;

    if (error != NULL && error_size > 0) {
        error[0] = '\0';
    }

    if (exe_path == NULL || exe_path[0] == '\0') {
        icon_set_error(error, error_size, "EXE path is required for icon embedding.");
        return -1;
    }

    if (ico_path != NULL && ico_path[0] != '\0') {
        if (icon_load_file(ico_path, &ico_data, &ico_size, error, error_size) != 0 ||
            icon_parse_ico(ico_data, ico_size, &images, &count, error, error_size) != 0) {
            free(ico_data);
            return -1;
        }
    } else if (icon_make_default_images(uninstall_icon, &images, &count) != 0) {
        icon_set_error(error, error_size, "Cannot build default Openstaller icon.");
        return -1;
    }

    update = BeginUpdateResourceA(exe_path, FALSE);
    if (update == NULL) {
        icon_free_images(images, count);
        free(ico_data);
        icon_set_error(error, error_size, "Cannot open EXE resources for icon update: %s", exe_path);
        return -1;
    }

    ok = icon_update_resources(update, images, count, error, error_size) == 0;
    if (!EndUpdateResourceA(update, ok ? FALSE : TRUE)) {
        icon_free_images(images, count);
        free(ico_data);
        icon_set_error(error, error_size, "Cannot save EXE icon resources: %s", exe_path);
        return -1;
    }

    icon_free_images(images, count);
    free(ico_data);
    return ok ? 0 : -1;
}

#else

int os_icon_apply_to_exe(const char *exe_path,
                         const char *ico_path,
                         int uninstall_icon,
                         char *error,
                         size_t error_size)
{
    (void)exe_path;
    (void)ico_path;
    (void)uninstall_icon;
    if (error != NULL && error_size > 0) {
        error[0] = '\0';
    }
    return 0;
}

#endif
