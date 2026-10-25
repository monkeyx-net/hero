/* VIC-II line renderer + sprite collision engine.
 *
 * Shared by the port (C64 mode) and the reference model in tools/, so both
 * produce identical pixels and identical $D01E/$D01F collision bits.
 * Line based: callers apply register changes between lines (raster splits).
 */
#ifndef VIC_H
#define VIC_H
#include <stdint.h>

#define VIC_LINES       312     /* PAL */
#define VIC_FB_W        384     /* X coordinates -8..375 */
#define VIC_FB_H        272     /* raster lines 16..287 */
#define VIC_FIRST_LINE  16
#define VIC_X0          (24 - 32)

typedef struct {
    const uint8_t *ram;      /* 64 KB as seen by the VIC */
    const uint8_t *color;    /* 1 KB colour RAM (low nibbles) */
    const uint8_t *chargen;  /* optional 4 KB char ROM for banks 0/2 at $1000 */
    unsigned bank;           /* 0..3, VIC bank (base = bank * $4000) */
} vic_mem_t;

extern const uint32_t vic_palette[16];   /* "Pepto" PAL, 0xRRGGBB */

/* Process raster line `line` with registers `reg`.
 * out: VIC_FB_W palette indices for this line, or NULL (collisions only).
 * Sprite-sprite / sprite-background collision bits are ORed into *ss / *sb. */
void vic_line(const uint8_t reg[0x40], const vic_mem_t *mem, unsigned line,
              uint8_t *out, uint8_t *ss, uint8_t *sb);

#endif
