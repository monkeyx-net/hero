/* test_unit: checks that need no game data (they run in CI).
 *  - tape decoder on a synthetic CBM ROM-loader tape
 *  - 6502 core: binary/decimal ADC, SBC, JSR/RTS, branches
 *  - PNG CRC32, VIC-II text rendering + sprite collision, lookalike font
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/font.h"
#include "../src/png.h"
#include "../src/tape.h"
#include "../src/vic.h"
#include "../tools/cpu6502.h"

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
                                              printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- synthetic tape ------------------------------------------------------- */

typedef struct { uint8_t *p; size_t n, cap; } buf_t;
static void put(buf_t *b, uint8_t v)
{
    if (b->n == b->cap) { b->cap = b->cap ? b->cap * 2 : 4096; b->p = realloc(b->p, b->cap); }
    b->p[b->n++] = v;
}
enum { S = 0x30, MP = 0x42, L = 0x56 };
static void tape_byte(buf_t *b, uint8_t v)
{
    int ones = 0;
    put(b, L); put(b, MP);                                     /* byte marker */
    for (int i = 0; i < 8; i++) {
        int bit = v >> i & 1;
        ones += bit;
        if (bit) { put(b, MP); put(b, S); } else { put(b, S); put(b, MP); }
    }
    if (ones & 1) { put(b, S); put(b, MP); } else { put(b, MP); put(b, S); }   /* odd parity */
}
static void tape_block(buf_t *b, const uint8_t *data, size_t n, int repeat)
{
    for (int i = 0; i < 200; i++) put(b, S);                   /* leader */
    uint8_t x = 0;
    for (int k = 0; k < 9; k++) tape_byte(b, (uint8_t)((repeat ? 0x09 : 0x89) - k));
    for (size_t i = 0; i < n; i++) { tape_byte(b, data[i]); x ^= data[i]; }
    tape_byte(b, x);
    put(b, L); put(b, S);                                      /* end of data */
}

typedef struct { int blocks, first, repeat; uint8_t got[300]; size_t len; } seen_t;
static void on_block(void *user, const uint8_t *p, size_t n, int repeat)
{
    seen_t *s = user;
    s->blocks++;
    if (repeat) s->repeat++;
    else { s->first++; if (n <= sizeof s->got) { memcpy(s->got, p, n); s->len = n; } }
}

static void test_tape(void)
{
    buf_t b = {0};
    const char hdr[] = "C64-TAPE-RAW";
    for (size_t i = 0; i < 12; i++) put(&b, (uint8_t)hdr[i]);
    put(&b, 1); put(&b, 0); put(&b, 0); put(&b, 0);
    for (int i = 0; i < 4; i++) put(&b, 0);                    /* length (unchecked) */
    uint8_t data[257];
    for (int i = 0; i < 257; i++) data[i] = (uint8_t)(i * 37 + 11);
    tape_block(&b, data, sizeof data, 0);
    tape_block(&b, data, sizeof data, 1);
    seen_t s = {0};
    int n = tape_decode(b.p, b.n, on_block, &s);
    CHECK(n == 2, "tape_decode found %d blocks, want 2", n);
    CHECK(s.first == 1 && s.repeat == 1, "first %d repeat %d", s.first, s.repeat);
    CHECK(s.len == sizeof data && !memcmp(s.got, data, sizeof data), "payload differs");

    b.p[b.n / 3] = 0xFF;                                       /* damage: still one good copy */
    memset(&s, 0, sizeof s);
    tape_decode(b.p, b.n, on_block, &s);
    CHECK(s.first + s.repeat >= 1, "damaged tape: no good copy left");

    static uint8_t img[0x10000];
    char err[200];
    CHECK(tape_load_mem(b.p, b.n, img, err, sizeof err) != 0, "a tape without the game loaded");
    CHECK(tape_decode((const uint8_t *)"not a tape at all...", 20, on_block, &s) < 0, "garbage accepted");
    free(b.p);
}

/* ---- 6502 core ----------------------------------------------------------------- */

static uint8_t ram[0x10000];
static uint8_t rd(cpu6502_t *c, uint16_t a) { (void)c; return ram[a]; }
static void wr(cpu6502_t *c, uint16_t a, uint8_t v) { (void)c; ram[a] = v; }

static void run(cpu6502_t *c, const uint8_t *prog, size_t n)
{
    memset(ram, 0, sizeof ram);
    memcpy(ram + 0x0200, prog, n);
    memset(c, 0, sizeof *c);
    c->read = rd; c->write = wr; c->s = 0xFF; c->p = F_U; c->pc = 0x0200;
    for (int i = 0; i < 1000 && !c->jammed && c->pc != 0x0000; i++) cpu6502_step(c);
}

static void test_cpu(void)
{
    cpu6502_t c;
    /* CLC; LDA #$7F; ADC #$01 -> $80, V set; BRK-free end via JMP $0000 */
    static const uint8_t p1[] = { 0x18, 0xA9, 0x7F, 0x69, 0x01, 0x4C, 0x00, 0x00 };
    run(&c, p1, sizeof p1);
    CHECK(c.a == 0x80 && (c.p & F_V) && (c.p & F_N) && !(c.p & F_C), "ADC binary A=%02X P=%02X", c.a, c.p);
    /* SED; CLC; LDA #$19; ADC #$28 -> $47; ADC #$58 -> $05 + carry (decimal) */
    static const uint8_t p2[] = { 0xF8, 0x18, 0xA9, 0x19, 0x69, 0x28, 0x85, 0x10, 0x69, 0x58, 0x4C, 0x00, 0x00 };
    run(&c, p2, sizeof p2);
    CHECK(ram[0x10] == 0x47, "BCD 19+28 = %02X", ram[0x10]);
    CHECK(c.a == 0x05 && (c.p & F_C), "BCD 47+58 = %02X C=%d", c.a, c.p & F_C);
    /* SEC; LDA #$10; SBC #$20 -> $F0, C clear; JSR sub (INX); X == 1 */
    static const uint8_t p3[] = { 0x38, 0xA9, 0x10, 0xE9, 0x20, 0x20, 0x0B, 0x02, 0x4C, 0x00, 0x00, 0xE8, 0x60 };
    run(&c, p3, sizeof p3);
    CHECK(c.a == 0xF0 && !(c.p & F_C) && c.x == 1 && c.s == 0xFF, "SBC/JSR A=%02X X=%02X S=%02X", c.a, c.x, c.s);
    /* LDX #5; loop: DEX; BNE loop -> X == 0 */
    static const uint8_t p4[] = { 0xA2, 0x05, 0xCA, 0xD0, 0xFD, 0x4C, 0x00, 0x00 };
    run(&c, p4, sizeof p4);
    CHECK(c.x == 0, "DEX/BNE X=%02X", c.x);
}

/* ---- PNG, VIC, font ------------------------------------------------------------------ */

static void test_misc(void)
{
    CHECK(png_crc32((const uint8_t *)"123456789", 9, 0) == 0xCBF43926u, "crc32");

    static uint8_t mem[0x10000], color[0x400], reg[0x40];
    memset(mem, 0, sizeof mem);
    mem[0x0400] = 1;                     /* screen code 1 at row 0, column 0 */
    memset(mem + 0x2008, 0xFF, 8);       /* char 1 solid */
    color[0] = 7;
    reg[0x11] = 0x1B; reg[0x16] = 0x08; reg[0x18] = 0x18; reg[0x20] = 14; reg[0x21] = 6;
    vic_mem_t vm = { mem, color, NULL, 0 };
    uint8_t line[VIC_FB_W], ss = 0, sb = 0;
    vic_line(reg, &vm, 0x33 + 1, line, &ss, &sb);   /* yscroll 3: row 0 starts at line $33 */
    CHECK(line[24 - VIC_X0] == 7 && line[24 + 8 - VIC_X0] == 6 && line[0] == 14,
          "text pixel %d bg %d border %d", line[24 - VIC_X0], line[24 + 8 - VIC_X0], line[0]);
    /* two overlapping sprites collide */
    reg[0x15] = 0x03; reg[0x00] = 100; reg[0x01] = 100; reg[0x02] = 104; reg[0x03] = 100;
    mem[0x07F8] = 0xC0; mem[0x07F9] = 0xC0;
    memset(mem + 0xC0 * 64, 0xFF, 63);
    ss = sb = 0;
    vic_line(reg, &vm, 105, line, &ss, &sb);
    CHECK(ss == 0x03, "sprite collision %02X", ss);

    const uint8_t *rom = font_chargen();
    int lit = 0;
    for (int code = 1; code <= 26; code++) lit += rom[code * 8] | rom[code * 8 + 3];
    CHECK(lit > 0 && rom[32 * 8] == 0, "font letters / space");
}

int main(void)
{
    test_tape();
    test_cpu();
    test_misc();
    printf("%s (%d failure%s)\n", failures ? "FAILED" : "ok", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
