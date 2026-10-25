/* c64run: run a 64 KB C64 RAM image on the 6502 core with crude I/O stubs.
 * Used to let the game's own depacker run, then dump the final memory.
 *
 * usage: c64run <mem.bin> <start> <out.bin> [stop_lo stop_hi]
 *   Runs from hex <start>; stops when PC first leaves [stop_lo, stop_hi]
 *   (default $7E00-$7FFF) or after 200M cycles. Writes RAM to <out.bin>
 *   and logs I/O register writes and KERNAL calls seen.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "cpu6502.h"

static uint8_t ram[0x10000];
static uint8_t io[0x1000];
static unsigned raster;
static int nlog;

static int io_visible(void) { return (ram[1] & 3) != 0 && (ram[1] & 4); }

static uint8_t rd(cpu6502_t *c, uint16_t a)
{
    (void)c;
    if (a >= 0xD000 && a < 0xE000 && io_visible()) {
        if (a == 0xD012) return (uint8_t)(raster++ );
        if (a == 0xD011) return (uint8_t)((io[0x11] & 0x7F) | ((raster >> 1) & 0x80));
        return io[a - 0xD000];
    }
    if (a >= 0xE000 && (ram[1] & 2) && nlog < 40) {
        fprintf(stderr, "KERNAL read $%04X from PC~$%04X\n", a, c->pc);
        nlog++;
    }
    return ram[a];
}

static void wr(cpu6502_t *c, uint16_t a, uint8_t v)
{
    (void)c;
    if (a >= 0xD000 && a < 0xE000 && io_visible()) {
        io[a - 0xD000] = v;
        return;
    }
    ram[a] = v;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s <mem.bin> <start> <out.bin> [stop_lo stop_hi]\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f || fread(ram, 1, sizeof ram, f) != sizeof ram) { perror(argv[1]); return 1; }
    fclose(f);
    unsigned lo = 0x7E00, hi = 0x7FFF;
    if (argc >= 6) { lo = (unsigned)strtoul(argv[4], NULL, 16); hi = (unsigned)strtoul(argv[5], NULL, 16); }
    /* State the real tape load leaves behind; the protection checks it. */
    ram[1] = 0x26;     /* BASIC off, cassette PLAY pressed (bit 4 = 0) */
    ram[0xBA] = 1;     /* last device = cassette */

    cpu6502_t c = {0};
    c.read = rd;
    c.write = wr;
    c.pc = (uint16_t)strtoul(argv[2], NULL, 16);
    c.s = 0xFF;
    c.p = F_I | F_U;
    static uint16_t trace[64];
    unsigned tn = 0;
    while (c.cycles < 200000000ull) {
        trace[tn++ & 63] = c.pc;
        cpu6502_step(&c);
        if (c.jammed) { fprintf(stderr, "JAM at $%04X\n", c.pc); break; }
        if (c.pc >= 0xE000 && (ram[1] & 2)) {   /* KERNAL call: log + RTS */
            if (c.pc == 0xFFD2) putchar(c.a >= 0x20 && c.a < 0x7F ? c.a : 0x0A);
            else fprintf(stderr, "KERNAL $%04X A=%02X X=%02X Y=%02X\n", c.pc, c.a, c.x, c.y);
            /* Behave like a tape-only C64: no device on the serial bus. */
            if (c.pc == 0xFFB7) {                  /* READST */
                ram[0x90] = 0x80;
                c.a = 0x80;
                c.p = (uint8_t)((c.p & ~(F_Z | F_N)) | F_N);
            }
            c.p &= (uint8_t)~F_C;                  /* calls "succeed" */
            c.s++; uint16_t ret = ram[0x100 + c.s]; c.s++; ret |= (uint16_t)(ram[0x100 + c.s] << 8);
            c.pc = (uint16_t)(ret + 1);
            continue;
        }
        if (c.pc < lo || c.pc > hi) break;
    }
    printf("stopped at PC=$%04X A=%02X X=%02X Y=%02X S=%02X P=%02X after %llu cycles, $01=%02X\n",
           c.pc, c.a, c.x, c.y, c.s, c.p, (unsigned long long)c.cycles, ram[1]);
    if (getenv("TRACE"))
        for (unsigned k = tn - 64; k != tn; k++)
            printf("  $%04X\n", trace[k & 63]);
    f = fopen(argv[3], "wb");
    fwrite(ram, 1, sizeof ram, f);
    fclose(f);
    return 0;
}
