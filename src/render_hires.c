#include "render_hires.h"
#include <stddef.h>

/* the display window (320x200) inside the frame: X 24..343, raster lines 51..250 */
#define SCREEN_X (24 - VIC_X0)
#define SCREEN_Y (51 - VIC_FIRST_LINE)

static void crop_rect(int crop, int *x, int *y, int *w, int *h)
{
    if (crop == CROP_BORDER) { *x = 0; *y = 0; *w = VIC_FB_W; *h = VIC_FB_H; }
    else { *x = SCREEN_X; *y = SCREEN_Y; *w = 320; *h = 200; }
}

void render_crop_size(int crop, int *w, int *h)
{
    int x, y;
    crop_rect(crop, &x, &y, w, h);
}

int render_c64_argb(const uint8_t *fb, int crop, uint32_t *out)
{
    int x0, y0, w, h;
    crop_rect(crop, &x0, &y0, &w, &h);
    for (int y = 0; y < h; y++) {
        const uint8_t *s = fb + (y0 + y) * VIC_FB_W + x0;
        for (int x = 0; x < w; x++) *out++ = 0xFF000000u | vic_palette[s[x] & 15];
    }
    return 1;
}

/* Scale2x (EPX) on palette indices, straight to ARGB. Neighbours outside the
 * crop come from the frame itself, so the edges look the same either way. */
int render_hires_argb(const uint8_t *fb, int crop, uint32_t *out)
{
    int x0, y0, w, h;
    crop_rect(crop, &x0, &y0, &w, &h);
    uint32_t pal[16];
    for (int i = 0; i < 16; i++) pal[i] = 0xFF000000u | vic_palette[i];
    int ow = w * 2;
    for (int y = 0; y < h; y++) {
        int fy = y0 + y;
        const uint8_t *row = fb + fy * VIC_FB_W;
        const uint8_t *up = fy > 0 ? row - VIC_FB_W : row;
        const uint8_t *dn = fy < VIC_FB_H - 1 ? row + VIC_FB_W : row;
        uint32_t *d0 = out + (size_t)(2 * y) * (size_t)ow, *d1 = d0 + ow;
        for (int x = 0; x < w; x++) {
            int fx = x0 + x;
            uint8_t e = row[fx], b = up[fx], hh = dn[fx];
            uint8_t dd = fx > 0 ? row[fx - 1] : e, f = fx < VIC_FB_W - 1 ? row[fx + 1] : e;
            uint32_t pe = pal[e & 15];
            if (b != hh && dd != f) {
                d0[2 * x] = dd == b ? pal[dd & 15] : pe;
                d0[2 * x + 1] = b == f ? pal[f & 15] : pe;
                d1[2 * x] = dd == hh ? pal[dd & 15] : pe;
                d1[2 * x + 1] = hh == f ? pal[f & 15] : pe;
            } else {
                d0[2 * x] = d0[2 * x + 1] = d1[2 * x] = d1[2 * x + 1] = pe;
            }
        }
    }
    return 2;
}
