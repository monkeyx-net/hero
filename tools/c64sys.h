/* c64sys: a deliberately small C64 model used as a reference for the port.
 *
 * Enough to run H.E.R.O. from its decrypted RAM image: 6502, 64 KB RAM,
 * PAL raster timing + raster IRQ, CIA1 keyboard/joystick, KERNAL traps
 * (no ROM images needed), SID register capture, line-based VIC-II renderer.
 * Not cycle exact (no badlines / sprite DMA stalls).
 */
#ifndef C64SYS_H
#define C64SYS_H
#include <stdint.h>
#include "cpu6502.h"

#define C64_CYCLES_PER_LINE 63
#define C64_LINES           312
#define C64_FB_W            384     /* X coords -8..375 (sprite space -32..351 + 24) */
#define C64_FB_H            272     /* raster lines 16..287 */
#define C64_FB_FIRST_LINE   16
#define C64_FB_X0           (24 - 32)

enum {   /* joystick bits, active high in c64sys_t.joy */
    JOY_UP = 1, JOY_DOWN = 2, JOY_LEFT = 4, JOY_RIGHT = 8, JOY_FIRE = 16
};

typedef struct {
    cpu6502_t cpu;
    uint8_t ram[0x10000];
    uint8_t vic[0x40];
    uint8_t sid[0x20];
    uint8_t color[0x400];       /* colour RAM, low nibble */
    uint8_t cia1[0x10], cia2[0x10];

    unsigned line;              /* current raster line */
    unsigned line_cycle;        /* cycles into current line */
    uint64_t frame;
    uint64_t main_cycles;       /* cycles executed with I clear (main thread) */
    uint8_t irq_latch;          /* $D019 */
    int irq_pending;

    uint8_t joy[2];             /* port 1, port 2 (JOY_* bits) */
    uint8_t keymatrix[8];       /* [column] = row bits pressed (active high) */

    uint8_t fb[C64_FB_W * C64_FB_H];  /* palette indices 0..15 */
    const uint8_t *chargen;     /* optional 4 KB character ROM (VIC banks 0/2, $1000) */

    void (*sid_write)(void *user, uint8_t reg, uint8_t v, uint64_t cycle);
    void *sid_user;
    int log_kernal;
    uint8_t *coverage;          /* optional 64 KB map: bit0 = opcode executed here */
} c64sys_t;

/* Load a 64 KB RAM image and set CPU to start at pc with tape-load state. */
void c64sys_init(c64sys_t *m, const uint8_t ram[0x10000], uint16_t pc);
/* Run one full PAL frame (renders into fb). */
void c64sys_run_frame(c64sys_t *m);
/* Press/release a key in the 8x8 matrix (col = PA bit, row = PB bit). */
void c64sys_key(c64sys_t *m, int col, int row, int down);
void c64sys_restore(c64sys_t *m);   /* RESTORE key -> NMI */

#define c64_palette vic_palette
extern const uint32_t vic_palette[16];

#endif
