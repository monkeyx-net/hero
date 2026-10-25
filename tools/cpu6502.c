#include "cpu6502.h"
#include "op6502.h"

#define RD(a) c->read(c, (uint16_t)(a))
#define WR(a, v) c->write(c, (uint16_t)(a), (uint8_t)(v))

static const uint8_t cyc[256] = {
    7,6,2,8,3,3,5,5,3,2,2,2,4,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
    6,6,2,8,3,3,5,5,4,2,2,2,4,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
    6,6,2,8,3,3,5,5,3,2,2,2,3,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
    6,6,2,8,3,3,5,5,4,2,2,2,5,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
    2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4, 2,6,2,6,4,4,4,4,2,5,2,5,5,5,5,5,
    2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4, 2,5,2,5,4,4,4,4,2,4,2,4,4,4,4,4,
    2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
    2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
};

static void push(cpu6502_t *c, uint8_t v)
{
    if (c->stack) c->stack[c->s] = v;
    else WR(0x100 | c->s, v);
    c->s--;
}
static uint8_t pull(cpu6502_t *c)
{
    c->s++;
    return c->stack ? c->stack[c->s] : RD(0x100 | c->s);
}
static uint8_t nz(cpu6502_t *c, uint8_t v)
{
    c->p = (uint8_t)((c->p & ~(F_N | F_Z)) | (v & F_N) | (v ? 0 : F_Z));
    return v;
}
static uint16_t rd16(cpu6502_t *c, uint16_t a) { return (uint16_t)(RD(a) | RD((uint16_t)(a + 1)) << 8); }
static uint16_t rd16zp(cpu6502_t *c, uint8_t a) { return (uint16_t)(RD(a) | RD((uint8_t)(a + 1)) << 8); }

void cpu6502_reset(cpu6502_t *c)
{
    c->a = c->x = c->y = 0;
    c->s = 0xFD;
    c->p = F_I | F_U;
    c->jammed = 0;
    c->pc = rd16(c, 0xFFFC);
}

static void interrupt(cpu6502_t *c, uint16_t vec, int brk)
{
    push(c, (uint8_t)(c->pc >> 8));
    push(c, (uint8_t)c->pc);
    push(c, (uint8_t)(c->p | F_U | (brk ? F_B : 0)));
    c->p |= F_I;
    c->pc = rd16(c, vec);
    c->cycles += 7;
}

void cpu6502_irq(cpu6502_t *c) { if (!(c->p & F_I)) interrupt(c, 0xFFFE, 0); }
void cpu6502_nmi(cpu6502_t *c) { interrupt(c, 0xFFFA, 0); }

static void adc(cpu6502_t *c, uint8_t v)
{
    unsigned carry = c->p & F_C;
    if (c->p & F_D) {
        unsigned lo = (c->a & 0x0F) + (v & 0x0F) + carry;
        unsigned hi = (c->a & 0xF0) + (v & 0xF0);
        if (lo > 9) { lo += 6; }
        if (lo > 0x0F) hi += 0x10;
        uint8_t bin = (uint8_t)(c->a + v + carry);
        c->p = (uint8_t)((c->p & ~(F_Z | F_N | F_V | F_C)) | (bin ? 0 : F_Z) | (hi & 0x80));
        if (~(c->a ^ v) & (c->a ^ hi) & 0x80) c->p |= F_V;
        if (hi > 0x90) hi += 0x60;
        if (hi > 0xFF) c->p |= F_C;
        c->a = (uint8_t)((hi & 0xF0) | (lo & 0x0F));
        return;
    }
    unsigned r = c->a + v + carry;
    c->p &= (uint8_t)~(F_C | F_V);
    if (r > 0xFF) c->p |= F_C;
    if (~(c->a ^ v) & (c->a ^ r) & 0x80) c->p |= F_V;
    c->a = nz(c, (uint8_t)r);
}

static void sbc(cpu6502_t *c, uint8_t v)
{
    if (c->p & F_D) {
        unsigned borrow = (c->p & F_C) ? 0 : 1;
        int lo = (c->a & 0x0F) - (v & 0x0F) - (int)borrow;
        int hi = (c->a & 0xF0) - (v & 0xF0);
        unsigned bin = (unsigned)c->a - v - borrow;
        if (lo < 0) { lo -= 6; hi -= 0x10; }
        if (hi < 0) hi -= 0x60;
        c->p &= (uint8_t)~(F_C | F_V);
        if (bin < 0x100) c->p |= F_C;
        if ((c->a ^ v) & (c->a ^ bin) & 0x80) c->p |= F_V;
        nz(c, (uint8_t)bin);
        c->a = (uint8_t)((hi & 0xF0) | (lo & 0x0F));
        return;
    }
    unsigned r = (unsigned)c->a - v - ((c->p & F_C) ? 0 : 1);
    c->p &= (uint8_t)~(F_C | F_V);
    if (r < 0x100) c->p |= F_C;
    if ((c->a ^ v) & (c->a ^ r) & 0x80) c->p |= F_V;
    c->a = nz(c, (uint8_t)r);
}

static void cmp(cpu6502_t *c, uint8_t r, uint8_t v)
{
    c->p = (uint8_t)((c->p & ~F_C) | (r >= v ? F_C : 0));
    nz(c, (uint8_t)(r - v));
}

static uint8_t asl(cpu6502_t *c, uint8_t v) { c->p = (uint8_t)((c->p & ~F_C) | (v >> 7)); return nz(c, (uint8_t)(v << 1)); }
static uint8_t lsr(cpu6502_t *c, uint8_t v) { c->p = (uint8_t)((c->p & ~F_C) | (v & 1)); return nz(c, v >> 1); }
static uint8_t rol(cpu6502_t *c, uint8_t v) { uint8_t r = (uint8_t)(v << 1 | (c->p & F_C)); c->p = (uint8_t)((c->p & ~F_C) | (v >> 7)); return nz(c, r); }
static uint8_t ror(cpu6502_t *c, uint8_t v) { uint8_t r = (uint8_t)(v >> 1 | (c->p & F_C) << 7); c->p = (uint8_t)((c->p & ~F_C) | (v & 1)); return nz(c, r); }

int cpu6502_step(cpu6502_t *c)
{
    if (c->jammed) return 0;
    uint16_t pc0 = c->pc;
    uint8_t opc = RD(c->pc++);
    const op6502_t *o = &op6502[opc];
    uint16_t ea = 0;
    int extra = 0;
    uint8_t lo;

    switch (o->am) {
    case AM_IMP: case AM_ACC: break;
    case AM_IMM: ea = c->pc++; break;
    case AM_ZP:  ea = RD(c->pc++); break;
    case AM_ZPX: ea = (uint8_t)(RD(c->pc++) + c->x); break;
    case AM_ZPY: ea = (uint8_t)(RD(c->pc++) + c->y); break;
    case AM_ABS: ea = rd16(c, c->pc); c->pc += 2; break;
    case AM_ABX: ea = rd16(c, c->pc); c->pc += 2; extra = (ea & 0xFF) + c->x > 0xFF; ea = (uint16_t)(ea + c->x); break;
    case AM_ABY: ea = rd16(c, c->pc); c->pc += 2; extra = (ea & 0xFF) + c->y > 0xFF; ea = (uint16_t)(ea + c->y); break;
    case AM_IND: { uint16_t p = rd16(c, c->pc); c->pc += 2;
                   ea = (uint16_t)(RD(p) | RD((p & 0xFF00) | ((p + 1) & 0xFF)) << 8); break; }
    case AM_IZX: ea = rd16zp(c, (uint8_t)(RD(c->pc++) + c->x)); break;
    case AM_IZY: ea = rd16zp(c, RD(c->pc++)); extra = (ea & 0xFF) + c->y > 0xFF; ea = (uint16_t)(ea + c->y); break;
    case AM_REL: lo = RD(c->pc++); ea = (uint16_t)(c->pc + (int8_t)lo); break;
    }

    int cy = cyc[opc];
    uint8_t v, t;
#define BR(cond) do { if (cond) { cy += 1 + ((ea & 0xFF00) != (c->pc & 0xFF00)); c->pc = ea; } } while (0)
#define RMW(expr) do { v = RD(ea); WR(ea, v); v = (expr); WR(ea, v); } while (0)
    switch (opc) {
    /* loads / stores */
    case 0xA9: case 0xA5: case 0xB5: case 0xAD: case 0xBD: case 0xB9: case 0xA1: case 0xB1:
        c->a = nz(c, RD(ea)); cy += extra; break;
    case 0xA2: case 0xA6: case 0xB6: case 0xAE: case 0xBE: c->x = nz(c, RD(ea)); cy += extra; break;
    case 0xA0: case 0xA4: case 0xB4: case 0xAC: case 0xBC: c->y = nz(c, RD(ea)); cy += extra; break;
    case 0x85: case 0x95: case 0x8D: case 0x9D: case 0x99: case 0x81: case 0x91: WR(ea, c->a); break;
    case 0x86: case 0x96: case 0x8E: WR(ea, c->x); break;
    case 0x84: case 0x94: case 0x8C: WR(ea, c->y); break;
    /* transfers */
    case 0xAA: c->x = nz(c, c->a); break;
    case 0xA8: c->y = nz(c, c->a); break;
    case 0x8A: c->a = nz(c, c->x); break;
    case 0x98: c->a = nz(c, c->y); break;
    case 0xBA: c->x = nz(c, c->s); break;
    case 0x9A: c->s = c->x; break;
    /* stack */
    case 0x48: push(c, c->a); break;
    case 0x68: c->a = nz(c, pull(c)); break;
    case 0x08: push(c, (uint8_t)(c->p | F_B | F_U)); break;
    case 0x28: c->p = (uint8_t)((pull(c) & ~F_B) | F_U); break;
    /* logic / arithmetic */
    case 0x29: case 0x25: case 0x35: case 0x2D: case 0x3D: case 0x39: case 0x21: case 0x31:
        c->a = nz(c, c->a & RD(ea)); cy += extra; break;
    case 0x09: case 0x05: case 0x15: case 0x0D: case 0x1D: case 0x19: case 0x01: case 0x11:
        c->a = nz(c, c->a | RD(ea)); cy += extra; break;
    case 0x49: case 0x45: case 0x55: case 0x4D: case 0x5D: case 0x59: case 0x41: case 0x51:
        c->a = nz(c, c->a ^ RD(ea)); cy += extra; break;
    case 0x69: case 0x65: case 0x75: case 0x6D: case 0x7D: case 0x79: case 0x61: case 0x71:
        adc(c, RD(ea)); cy += extra; break;
    case 0xE9: case 0xEB: case 0xE5: case 0xF5: case 0xED: case 0xFD: case 0xF9: case 0xE1: case 0xF1:
        sbc(c, RD(ea)); cy += extra; break;
    case 0xC9: case 0xC5: case 0xD5: case 0xCD: case 0xDD: case 0xD9: case 0xC1: case 0xD1:
        cmp(c, c->a, RD(ea)); cy += extra; break;
    case 0xE0: case 0xE4: case 0xEC: cmp(c, c->x, RD(ea)); break;
    case 0xC0: case 0xC4: case 0xCC: cmp(c, c->y, RD(ea)); break;
    case 0x24: case 0x2C:
        v = RD(ea);
        c->p = (uint8_t)((c->p & ~(F_N | F_V | F_Z)) | (v & (F_N | F_V)) | ((c->a & v) ? 0 : F_Z));
        break;
    /* inc / dec */
    case 0xE6: case 0xF6: case 0xEE: case 0xFE: RMW(nz(c, (uint8_t)(v + 1))); break;
    case 0xC6: case 0xD6: case 0xCE: case 0xDE: RMW(nz(c, (uint8_t)(v - 1))); break;
    case 0xE8: c->x = nz(c, (uint8_t)(c->x + 1)); break;
    case 0xCA: c->x = nz(c, (uint8_t)(c->x - 1)); break;
    case 0xC8: c->y = nz(c, (uint8_t)(c->y + 1)); break;
    case 0x88: c->y = nz(c, (uint8_t)(c->y - 1)); break;
    /* shifts */
    case 0x0A: c->a = asl(c, c->a); break;
    case 0x4A: c->a = lsr(c, c->a); break;
    case 0x2A: c->a = rol(c, c->a); break;
    case 0x6A: c->a = ror(c, c->a); break;
    case 0x06: case 0x16: case 0x0E: case 0x1E: RMW(asl(c, v)); break;
    case 0x46: case 0x56: case 0x4E: case 0x5E: RMW(lsr(c, v)); break;
    case 0x26: case 0x36: case 0x2E: case 0x3E: RMW(rol(c, v)); break;
    case 0x66: case 0x76: case 0x6E: case 0x7E: RMW(ror(c, v)); break;
    /* flow */
    case 0x4C: case 0x6C: c->pc = ea; break;
    case 0x20: c->pc--; push(c, (uint8_t)(c->pc >> 8)); push(c, (uint8_t)c->pc); c->pc = ea; break;
    case 0x60: c->pc = (uint16_t)(pull(c) | pull(c) << 8); c->pc++; break;
    case 0x40: c->p = (uint8_t)((pull(c) & ~F_B) | F_U); c->pc = (uint16_t)(pull(c) | pull(c) << 8); break;
    case 0x00: c->pc++; interrupt(c, 0xFFFE, 1); c->cycles -= 7; break;
    case 0x10: BR(!(c->p & F_N)); break;
    case 0x30: BR(c->p & F_N); break;
    case 0x50: BR(!(c->p & F_V)); break;
    case 0x70: BR(c->p & F_V); break;
    case 0x90: BR(!(c->p & F_C)); break;
    case 0xB0: BR(c->p & F_C); break;
    case 0xD0: BR(!(c->p & F_Z)); break;
    case 0xF0: BR(c->p & F_Z); break;
    /* flags */
    case 0x18: c->p &= (uint8_t)~F_C; break;
    case 0x38: c->p |= F_C; break;
    case 0x58: c->p &= (uint8_t)~F_I; break;
    case 0x78: c->p |= F_I; break;
    case 0xB8: c->p &= (uint8_t)~F_V; break;
    case 0xD8: c->p &= (uint8_t)~F_D; break;
    case 0xF8: c->p |= F_D; break;
    case 0xEA: break;
    /* undocumented */
    case 0xA7: case 0xB7: case 0xAF: case 0xBF: case 0xA3: case 0xB3:
        c->a = c->x = nz(c, RD(ea)); cy += extra; break;
    case 0x87: case 0x97: case 0x8F: case 0x83: WR(ea, c->a & c->x); break;
    case 0xC7: case 0xD7: case 0xCF: case 0xDF: case 0xDB: case 0xC3: case 0xD3:
        RMW((uint8_t)(v - 1)); cmp(c, c->a, v); break;
    case 0xE7: case 0xF7: case 0xEF: case 0xFF: case 0xFB: case 0xE3: case 0xF3:
        RMW((uint8_t)(v + 1)); sbc(c, v); break;
    case 0x07: case 0x17: case 0x0F: case 0x1F: case 0x1B: case 0x03: case 0x13:
        RMW(asl(c, v)); c->a = nz(c, c->a | v); break;
    case 0x27: case 0x37: case 0x2F: case 0x3F: case 0x3B: case 0x23: case 0x33:
        RMW(rol(c, v)); c->a = nz(c, c->a & v); break;
    case 0x47: case 0x57: case 0x4F: case 0x5F: case 0x5B: case 0x43: case 0x53:
        RMW(lsr(c, v)); c->a = nz(c, c->a ^ v); break;
    case 0x67: case 0x77: case 0x6F: case 0x7F: case 0x7B: case 0x63: case 0x73:
        RMW(ror(c, v)); adc(c, v); break;
    case 0x0B: case 0x2B: c->a = nz(c, c->a & RD(ea)); c->p = (uint8_t)((c->p & ~F_C) | (c->a >> 7)); break;
    case 0x4B: c->a = lsr(c, c->a & RD(ea)); break;
    case 0x6B:
        t = c->a & RD(ea);
        c->a = nz(c, (uint8_t)(t >> 1 | (c->p & F_C) << 7));
        c->p = (uint8_t)((c->p & ~(F_C | F_V)) | ((c->a >> 6) & 1) | (((c->a >> 6) ^ (c->a >> 5)) & 1) << 6);
        break;
    case 0xCB: t = c->a & c->x; v = RD(ea); c->p = (uint8_t)((c->p & ~F_C) | (t >= v)); c->x = nz(c, (uint8_t)(t - v)); break;
    case 0xAB: c->a = c->x = nz(c, (uint8_t)((c->a | 0xEE) & RD(ea))); break;
    case 0x8B: c->a = nz(c, (uint8_t)((c->a | 0xEE) & c->x & RD(ea))); break;
    case 0xBB: c->a = c->x = c->s = nz(c, RD(ea) & c->s); cy += extra; break;
    case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA: break;
    case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2:
    case 0x04: case 0x44: case 0x64: case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4:
    case 0x0C: case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC:
        (void)RD(ea); cy += extra; break;
    case 0x9C: case 0x9E: case 0x93: case 0x9F: case 0x9B: {
        uint8_t h = (uint8_t)((ea >> 8) + 1);
        uint8_t val = opc == 0x9C ? (uint8_t)(c->y & h) : opc == 0x9E ? (uint8_t)(c->x & h) : (uint8_t)(c->a & c->x & h);
        if (opc == 0x9B) c->s = c->a & c->x;
        WR(ea, val);
        break;
    }
    default: /* JAM */
        c->jammed = 1;
        c->pc = pc0;
        return 0;
    }
    c->cycles += (uint64_t)cy;
    return cy;
}
