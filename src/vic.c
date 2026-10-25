#include "vic.h"
#include <string.h>

const uint32_t vic_palette[16] = {
    0x000000, 0xFFFFFF, 0x68372B, 0x70A4B2, 0x6F3D86, 0x588D43, 0x352879, 0xB8C76F,
    0x6F4F25, 0x433900, 0x9A6759, 0x444444, 0x6C6C6C, 0x9AD284, 0x6C5EB5, 0x959595,
};

static uint8_t vram(const vic_mem_t *m, unsigned off)
{
    unsigned base = m->bank * 0x4000u;
    if (m->chargen && !(m->bank & 1u) && (off & 0x3000u) == 0x1000u)
        return m->chargen[off & 0xFFFu];
    return m->ram[(base + (off & 0x3FFFu)) & 0xFFFFu];
}

/* The 320 graphics pixels of one text/bitmap row line, a character cell (8
 * pixels) at a time: colour indices in g, foreground flags in f. */
static void graphics_row(const uint8_t *v, const vic_mem_t *m, int row, int rl, uint8_t *g, uint8_t *f)
{
    int ecm = v[0x11] & 0x40, bmm = v[0x11] & 0x20, mcm = v[0x16] & 0x10;
    unsigned scr = (v[0x18] >> 4) * 0x400u;
    unsigned chr = (v[0x18] & 0x0E) * 0x400u;
    unsigned bmp = (v[0x18] & 0x08) * 0x400u;
    uint8_t b0 = v[0x21] & 15;
    for (int c = 0; c < 40; c++, g += 8, f += 8) {
        unsigned cell = (unsigned)row * 40 + (unsigned)c;
        uint8_t code = vram(m, scr + cell), cram = m->color[cell] & 15;
        if (!bmm) {
            uint8_t bits = vram(m, chr + (ecm ? (code & 63u) : code) * 8u + (unsigned)rl);
            if (ecm && mcm) {                              /* invalid mode: black */
                memset(g, 0, 8); memset(f, 0, 8);
            } else if (ecm) {
                uint8_t bg = v[0x21 + (code >> 6)] & 15;
                for (int b = 0; b < 8; b++) {
                    int on = bits >> (7 - b) & 1;
                    g[b] = on ? cram : bg; f[b] = (uint8_t)on;
                }
            } else if (mcm && (cram & 8)) {
                uint8_t col4[4] = { b0, (uint8_t)(v[0x22] & 15), (uint8_t)(v[0x23] & 15), (uint8_t)(cram & 7) };
                for (int b = 0; b < 8; b += 2) {
                    unsigned pair = (bits >> (6 - b)) & 3u;
                    g[b] = g[b + 1] = col4[pair];
                    f[b] = f[b + 1] = pair >= 2;
                }
            } else {
                uint8_t fg = mcm ? (cram & 7) : cram;
                for (int b = 0; b < 8; b++) {
                    int on = bits >> (7 - b) & 1;
                    g[b] = on ? fg : b0; f[b] = (uint8_t)on;
                }
            }
        } else {
            uint8_t bits = vram(m, bmp + cell * 8u + (unsigned)rl);
            if (ecm) {
                memset(g, 0, 8); memset(f, 0, 8);
            } else if (mcm) {
                uint8_t col4[4] = { b0, (uint8_t)(code >> 4), (uint8_t)(code & 15), cram };
                for (int b = 0; b < 8; b += 2) {
                    unsigned pair = (bits >> (6 - b)) & 3u;
                    g[b] = g[b + 1] = col4[pair];
                    f[b] = f[b + 1] = pair >= 2;
                }
            } else {
                for (int b = 0; b < 8; b++) {
                    int on = bits >> (7 - b) & 1;
                    g[b] = on ? (uint8_t)(code >> 4) : (uint8_t)(code & 15); f[b] = (uint8_t)on;
                }
            }
        }
    }
}

void vic_line(const uint8_t reg[0x40], const vic_mem_t *m, unsigned line,
              uint8_t *out, uint8_t *ss, uint8_t *sb)
{
    const uint8_t *v = reg;

    /* sprites on this line (nothing to do at all for an undrawn line without any) */
    int sprite_on[8], any_sprite = 0;
    for (int i = 0; i < 8; i++) {
        int sy = v[1 + i * 2] + 1, yexp = v[0x17] >> i & 1;
        sprite_on[i] = (v[0x15] >> i & 1) && (int)line >= sy && (((int)line - sy) >> yexp) < 21;
        any_sprite |= sprite_on[i];
    }
    if (!out && !any_sprite) return;

    uint8_t fg[VIC_FB_W];            /* foreground mask for priority/collision */
    uint8_t scratch[VIC_FB_W];
    if (!out) out = scratch;

    uint8_t border = v[0x20] & 15;
    int rsel = v[0x11] & 8, csel = v[0x16] & 8, den = v[0x11] & 0x10;
    unsigned top = rsel ? 51 : 55, bot = rsel ? 251 : 247;
    int left = csel ? 24 : 31, right = csel ? 344 : 335;
    int in_v = den && line >= top && line < bot;

    memset(fg, 0, sizeof fg);
    if (!in_v) {
        memset(out, border, VIC_FB_W);
    } else {
        int lpx = left - VIC_X0, rpx = right - VIC_X0;           /* window in fb pixels */
        memset(out, border, (size_t)lpx);
        memset(out + rpx, border, (size_t)(VIC_FB_W - rpx));
        unsigned yscroll = v[0x11] & 7, xscroll = v[0x16] & 7;
        int dy = (int)line - (0x30 + (int)yscroll);
        int row = dy >= 0 ? dy / 8 : -1, rl = dy >= 0 ? dy & 7 : 0;
        uint8_t bg = v[0x21] & 15;
        if (row >= 0 && row < 25) {
            uint8_t g[320], f[320];
            graphics_row(v, m, row, rl, g, f);
            int gx0 = lpx + VIC_X0 - 24 - (int)xscroll;         /* graphics x of the first window pixel */
            for (int px = lpx; px < rpx; px++) {
                int gx = gx0 + (px - lpx);
                if (gx >= 0 && gx < 320) { out[px] = g[gx]; fg[px] = f[gx]; }
                else out[px] = bg;
            }
        } else {
            memset(out + lpx, bg, (size_t)(rpx - lpx));
        }
    }
    if (!any_sprite) return;

    /* Sprites: draw 7..0 so sprite 0 ends on top. */
    uint8_t owner[VIC_FB_W];
    memset(owner, 0, sizeof owner);
    unsigned scr = (v[0x18] >> 4) * 0x400u;
    for (int i = 7; i >= 0; i--) {
        if (!sprite_on[i]) continue;
        int yexp = v[0x17] >> i & 1, xexp = v[0x1D] >> i & 1;
        int sl = ((int)line - (v[1 + i * 2] + 1)) >> yexp;
        int sx = v[i * 2] | ((v[0x10] >> i & 1) << 8);
        unsigned ptr = vram(m, scr + 0x3F8u + (unsigned)i) * 64u + (unsigned)sl * 3u;
        uint32_t data = (uint32_t)vram(m, ptr) << 16 | (uint32_t)vram(m, ptr + 1) << 8 | vram(m, ptr + 2);
        if (!data) continue;
        int mc = v[0x1C] >> i & 1, behind = v[0x1B] >> i & 1;
        for (int p = 0; p < 24 << xexp; p++) {
            int q = p >> xexp;
            uint8_t col;
            if (mc) {
                unsigned pair = (data >> (22 - (q & ~1))) & 3u;
                if (!pair) continue;
                col = pair == 1 ? v[0x25] : pair == 2 ? v[0x27 + i] : v[0x26];
            } else {
                if (!(data >> (23 - q) & 1)) continue;
                col = v[0x27 + i];
            }
            int px = sx + p - VIC_X0;
            if (px < 0 || px >= VIC_FB_W) continue;
            if (owner[px]) *ss |= (uint8_t)(owner[px] | 1 << i);
            owner[px] |= (uint8_t)(1 << i);
            if (fg[px]) *sb |= (uint8_t)(1 << i);
            if (behind && fg[px]) continue;
            int x = px + VIC_X0;
            if (!in_v || x < left || x >= right) continue;   /* border covers sprites */
            out[px] = col & 15;
        }
    }
}
