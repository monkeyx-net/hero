#include "c64sys.h"
#include "../src/vic.h"
#include <stdio.h>
#include <string.h>


static c64sys_t *cur;   /* the CPU callbacks need the machine */

static int hiram(const c64sys_t *m) { return (m->ram[1] & 2) != 0; }
static int io_on(const c64sys_t *m) { return (m->ram[1] & 3) != 0 && (m->ram[1] & 4); }

static unsigned raster_cmp(const c64sys_t *m)
{
    return m->vic[0x12] | ((unsigned)(m->vic[0x11] & 0x80) << 1);
}

static void update_irq(c64sys_t *m)
{
    m->irq_pending = (m->irq_latch & m->vic[0x1A] & 0x0F) != 0;
}

static uint8_t cia1_read(c64sys_t *m, uint8_t r)
{
    uint8_t pra = (uint8_t)(m->cia1[0] | ~m->cia1[2]);
    switch (r) {
    case 0x00:
        return (uint8_t)(pra & ~m->joy[1]);
    case 0x01: {
        uint8_t rows = 0xFF;
        for (int c = 0; c < 8; c++)
            if (!(pra & (1 << c))) rows &= (uint8_t)~m->keymatrix[c];
        return (uint8_t)(rows & ~m->joy[0]);
    }
    case 0x04: return (uint8_t)~m->cpu.cycles;
    case 0x05: return (uint8_t)(~m->cpu.cycles >> 8);
    case 0x0D: return 0;
    default: return m->cia1[r];
    }
}

static uint8_t rd(cpu6502_t *c, uint16_t a)
{
    c64sys_t *m = cur;
    (void)c;
    if (a >= 0xD000 && a < 0xE000 && io_on(m)) {
        if (a < 0xD400) {
            uint8_t r = a & 0x3F, v;
            switch (r) {
            case 0x11: return (uint8_t)((m->vic[0x11] & 0x7F) | ((m->line & 0x100) >> 1));
            case 0x12: return (uint8_t)m->line;
            case 0x19: return (uint8_t)(m->irq_latch | 0x70 | (m->irq_pending ? 0x80 : 0));
            case 0x1E: case 0x1F: v = m->vic[r]; m->vic[r] = 0; return v;
            default: return r < 0x2F ? m->vic[r] : 0xFF;
            }
        }
        if (a < 0xD800) {
            static uint32_t lfsr = 0x7FFFF8;
            switch (a & 0x1F) {
            case 0x19: case 0x1A: return 0xFF;
            case 0x1B:
                lfsr = (lfsr << 1) | (((lfsr >> 22) ^ (lfsr >> 17)) & 1);
                return (uint8_t)lfsr;
            case 0x1C: return 0;
            default: return 0;
            }
        }
        if (a < 0xDC00) return (uint8_t)(m->color[a & 0x3FF] | 0xF0);
        if (a < 0xDD00) return cia1_read(m, a & 0x0F);
        if (a < 0xDE00) {
            if ((a & 0x0F) == 0) return (uint8_t)(m->cia2[0] | 0xC0);
            return m->cia2[a & 0x0F];
        }
        return 0xFF;
    }
    return m->ram[a];
}

static void wr(cpu6502_t *c, uint16_t a, uint8_t v)
{
    c64sys_t *m = cur;
    (void)c;
    if (a >= 0xD000 && a < 0xE000 && io_on(m)) {
        if (a < 0xD400) {
            uint8_t r = a & 0x3F;
            if (r == 0x19) { m->irq_latch &= (uint8_t)~(v & 0x0F); update_irq(m); return; }
            if (r == 0x1E || r == 0x1F) return;
            if (r < 0x2F) m->vic[r] = v;
            if (r == 0x1A) update_irq(m);
            return;
        }
        if (a < 0xD800) {
            m->sid[a & 0x1F] = v;
            if (m->sid_write) m->sid_write(m->sid_user, a & 0x1F, v, m->cpu.cycles);
            return;
        }
        if (a < 0xDC00) { m->color[a & 0x3FF] = v & 0x0F; return; }
        if (a < 0xDD00) { m->cia1[a & 0x0F] = v; return; }
        if (a < 0xDE00) { m->cia2[a & 0x0F] = v; return; }
        return;
    }
    m->ram[a] = v;
}

void c64sys_init(c64sys_t *m, const uint8_t ram[0x10000], uint16_t pc)
{
    memset(m, 0, sizeof *m);
    memcpy(m->ram, ram, 0x10000);
    /* State the real tape load leaves behind; the loader protection checks it. */
    m->ram[1] = 0x26;          /* BASIC off, cassette PLAY pressed (bit 4 = 0) */
    m->ram[0xBA] = 1;          /* last device = cassette */
    m->ram[0x0314] = 0x31;     /* default IRQ vector $EA31 */
    m->ram[0x0315] = 0xEA;
    m->cia2[0] = 0x03;
    m->cpu.read = rd;
    m->cpu.write = wr;
    m->cpu.pc = pc;
    m->cpu.s = 0xFF;
    m->cpu.p = F_I | F_U;
}

static void push(c64sys_t *m, uint8_t v) { m->ram[0x100 | m->cpu.s] = v; m->cpu.s--; }
static uint8_t pull(c64sys_t *m) { m->cpu.s++; return m->ram[0x100 | m->cpu.s]; }

/* Emulate the KERNAL's IRQ/NMI entry and exit, and stub other calls. */
static int kernal_trap(c64sys_t *m)
{
    cpu6502_t *c = &m->cpu;
    if (c->pc < 0xE000 || !hiram(m)) return 0;
    switch (c->pc) {
    case 0xEA31: case 0xEA7E: case 0xEA81: case 0xFEBC:   /* pull Y,X,A; RTI */
        c->y = pull(m); c->x = pull(m); c->a = pull(m);
        c->p = (uint8_t)((pull(m) & ~F_B) | F_U);
        c->pc = (uint16_t)(pull(m) | pull(m) << 8);
        return 1;
    default:
        break;
    }
    if (m->log_kernal)
        fprintf(stderr, "KERNAL $%04X A=%02X X=%02X Y=%02X\n", c->pc, c->a, c->x, c->y);
    if (c->pc == 0xFFB7) {                 /* READST: nothing on the serial bus */
        m->ram[0x90] = 0x80;
        c->a = 0x80;
        c->p = (uint8_t)((c->p & ~F_Z) | F_N);
    }
    c->p &= (uint8_t)~F_C;
    c->pc = (uint16_t)((pull(m) | pull(m) << 8) + 1);   /* RTS */
    return 1;
}

static void take_irq(c64sys_t *m)
{
    cpu6502_t *c = &m->cpu;
    if (!hiram(m)) { cpu6502_irq(c); return; }
    push(m, (uint8_t)(c->pc >> 8));
    push(m, (uint8_t)c->pc);
    push(m, (uint8_t)(c->p | F_U));
    c->p |= F_I;
    push(m, c->a); push(m, c->x); push(m, c->y);       /* KERNAL $FF48 */
    c->pc = (uint16_t)(m->ram[0x314] | m->ram[0x315] << 8);
    c->cycles += 7 + 22;
}

void c64sys_restore(c64sys_t *m)
{
    cpu6502_t *c = &m->cpu;
    cur = m;
    if (!hiram(m)) { cpu6502_nmi(c); return; }
    push(m, (uint8_t)(c->pc >> 8));
    push(m, (uint8_t)c->pc);
    push(m, (uint8_t)(c->p | F_U));
    c->p |= F_I;
    c->pc = (uint16_t)(m->ram[0x318] | m->ram[0x319] << 8);   /* KERNAL $FE43 */
}

void c64sys_key(c64sys_t *m, int col, int row, int down)
{
    if (down) m->keymatrix[col] |= (uint8_t)(1 << row);
    else m->keymatrix[col] &= (uint8_t)~(1 << row);
}

/* ---- VIC-II: shared line renderer (src/vic.c) ----------------------------- */

static void render_line(c64sys_t *m, unsigned line)
{
    vic_mem_t mem = { m->ram, m->color, m->chargen, 3u - (m->cia2[0] & 3u) };
    uint8_t *out = NULL;
    if (line >= C64_FB_FIRST_LINE && line < C64_FB_FIRST_LINE + C64_FB_H)
        out = m->fb + (line - C64_FB_FIRST_LINE) * C64_FB_W;
    vic_line(m->vic, &mem, line, out, &m->vic[0x1E], &m->vic[0x1F]);
}

void c64sys_run_frame(c64sys_t *m)
{
    cur = m;
    cpu6502_t *c = &m->cpu;
    for (;;) {
        if (m->irq_pending && !(c->p & F_I)) take_irq(m);
        if (kernal_trap(m)) continue;
        if (m->coverage) m->coverage[c->pc] |= 1;
        int was_main = !(c->p & F_I);
        int cy = cpu6502_step(c);
        if (was_main) m->main_cycles += (unsigned)cy;
        if (c->jammed) cy = C64_CYCLES_PER_LINE;
        m->line_cycle += (unsigned)cy;
        while (m->line_cycle >= C64_CYCLES_PER_LINE) {
            m->line_cycle -= C64_CYCLES_PER_LINE;
            render_line(m, m->line);
            if (++m->line == C64_LINES) {
                m->line = 0;
                m->frame++;
                if (m->line == raster_cmp(m)) { m->irq_latch |= 1; update_irq(m); }
                return;
            }
            if (m->line == raster_cmp(m)) { m->irq_latch |= 1; update_irq(m); }
        }
    }
}
