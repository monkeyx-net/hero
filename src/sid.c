#include "sid.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CLOCK 985248.0          /* PAL */
#define FRAME_HZ (CLOCK / (63.0 * 312.0))

typedef struct {
    double acc;                 /* 24-bit phase accumulator */
    uint32_t lfsr;
    uint8_t prev_msb;
    double env;                 /* 0..255 */
    int state;                  /* 0 release, 1 attack, 2 decay/sustain */
    double exp_cnt;
} voice_t;

static uint8_t reg[0x20];
static voice_t v[3];
static double lp, bp;           /* filter state */
static double frac;             /* fractional samples per frame */

static const double attack_ms[16] = { 2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000 };

void sid_reset(void)
{
    memset(reg, 0, sizeof reg);
    memset(v, 0, sizeof v);
    for (int i = 0; i < 3; i++) v[i].lfsr = 0x7FFFF8;
    lp = bp = 0;
}

static void write_reg(uint8_t r, uint8_t val)
{
    if (r >= 0x19) return;
    int vi = r / 7, off = r % 7;
    if (r < 0x15 && off == 4) {
        uint8_t old = reg[r];
        if ((val & 1) && !(old & 1)) v[vi].state = 1;          /* gate on: attack */
        if (!(val & 1) && (old & 1)) v[vi].state = 0;          /* gate off: release */
        if (val & 8) v[vi].acc = 0;                            /* test bit */
    }
    reg[r] = val;
}

/* exponential decay/release step multiplier like the real chip's counter */
static double exp_period(double env)
{
    if (env > 93) return 1;
    if (env > 54) return 2;
    if (env > 26) return 4;
    if (env > 14) return 8;
    if (env > 6) return 16;
    if (env > 0) return 30;
    return 1;
}

static void step_env(voice_t *o, int vi, double dt)
{
    uint8_t ad = reg[vi * 7 + 5], sr = reg[vi * 7 + 6];
    double sustain = (sr >> 4) * 17.0;
    if (o->state == 1) {
        o->env += 255.0 * dt * 1000.0 / attack_ms[ad >> 4];
        if (o->env >= 255) { o->env = 255; o->state = 2; }
    } else if (o->state == 2) {
        if (o->env > sustain) {
            o->env -= 255.0 * dt * 1000.0 / (attack_ms[ad & 15] * 3.0) / exp_period(o->env) * 1.0;
            if (o->env < sustain) o->env = sustain;
        }
    } else if (o->env > 0) {
        o->env -= 255.0 * dt * 1000.0 / (attack_ms[sr & 15] * 3.0) / exp_period(o->env);
        if (o->env < 0) o->env = 0;
    }
}

static int wave(int vi)
{
    voice_t *o = &v[vi];
    uint8_t ctl = reg[vi * 7 + 4];
    uint32_t acc = (uint32_t)o->acc & 0xFFFFFF;
    int src = (vi + 2) % 3;
    uint32_t sacc = (uint32_t)v[src].acc & 0xFFFFFF;
    unsigned out = 0xFFF;
    int any = 0;
    if (ctl & 0x10) {           /* triangle (ring mod XORs the MSB with the source) */
        uint32_t msb = acc & 0x800000;
        if (ctl & 0x04) msb ^= sacc & 0x800000;
        unsigned t = (unsigned)((msb ? ~acc : acc) >> 11) & 0xFFF;
        out &= t; any = 1;
    }
    if (ctl & 0x20) { out &= (acc >> 12) & 0xFFF; any = 1; }
    if (ctl & 0x40) {
        unsigned pw = (unsigned)(reg[vi * 7 + 2] | (reg[vi * 7 + 3] & 0x0F) << 8);
        out &= ((acc >> 12) >= pw || (ctl & 8)) ? 0xFFF : 0; any = 1;
    }
    if (ctl & 0x80) {
        uint32_t l = o->lfsr;
        unsigned n = (unsigned)(((l >> 22) & 1) << 7 | ((l >> 20) & 1) << 6 | ((l >> 16) & 1) << 5 |
                                ((l >> 13) & 1) << 4 | ((l >> 11) & 1) << 3 | ((l >> 7) & 1) << 2 |
                                ((l >> 4) & 1) << 1 | ((l >> 2) & 1));
        out &= n << 4; any = 1;
    }
    return any ? (int)out - 0x800 : 0;
}

int sid_render_frame(const hw_sidw_t *w, unsigned nw, int16_t *out)
{
    frac += SID_RATE / FRAME_HZ;
    int n = (int)frac;
    frac -= n;
    if (n > 1024) n = 1024;
    const double cps = CLOCK / SID_RATE, dt = 1.0 / SID_RATE;
    unsigned wi = 0;
    for (int s = 0; s < n; s++) {
        unsigned line = (unsigned)s * 312u / (unsigned)n;
        while (wi < nw && (unsigned)(w[wi].line | w[wi].hi << 8) <= line) {
            write_reg(w[wi].reg, w[wi].val);
            wi++;
        }
        /* oscillators */
        for (int i = 0; i < 3; i++) {
            voice_t *o = &v[i];
            uint8_t ctl = reg[i * 7 + 4];
            o->prev_msb = (uint8_t)(((uint32_t)o->acc >> 23) & 1);
            if (ctl & 8) { o->acc = 0; continue; }
            double freq = reg[i * 7] | reg[i * 7 + 1] << 8;
            double before = o->acc;
            o->acc += freq * cps;
            /* noise LFSR clocks on each 0->1 of accumulator bit 19 */
            long edges = (long)floor((o->acc - 524288.0) / 1048576.0) -
                         (long)floor((before - 524288.0) / 1048576.0);
            for (long e = 0; e < edges; e++) {
                uint32_t l = o->lfsr;
                o->lfsr = ((l << 1) | (((l >> 22) ^ (l >> 17)) & 1)) & 0x7FFFFF;
            }
            while (o->acc >= 16777216.0) o->acc -= 16777216.0;
        }
        for (int i = 0; i < 3; i++) {                      /* hard sync */
            int src = (i + 2) % 3;
            if ((reg[i * 7 + 4] & 2) && !v[src].prev_msb && (((uint32_t)v[src].acc >> 23) & 1))
                v[i].acc = 0;
        }
        /* mix */
        double filt_in = 0, direct = 0;
        uint8_t route = reg[0x17], mode = reg[0x18];
        for (int i = 0; i < 3; i++) {
            step_env(&v[i], i, dt);
            double smp = wave(i) * v[i].env / 255.0;
            if (route >> i & 1) filt_in += smp;
            else if (!(i == 2 && (mode & 0x80))) direct += smp;
        }
        unsigned cutoff = (unsigned)((reg[0x15] & 7) | reg[0x16] << 3);
        double fc = 30.0 + cutoff * 5.8;
        if (fc > 16000) fc = 16000;
        double f = 2.0 * sin(M_PI * fc / SID_RATE);
        double q = 1.4 - (reg[0x17] >> 4) / 15.0 * 1.1;
        double hp = filt_in - lp - q * bp;
        bp += f * hp;
        lp += f * bp;
        double fout = 0;
        if (mode & 0x10) fout += lp;
        if (mode & 0x20) fout += bp;
        if (mode & 0x40) fout += hp;
        double mix = (direct + fout) * (mode & 15) / 15.0;
        mix *= 3.0;
        if (mix > 32767) mix = 32767;
        if (mix < -32768) mix = -32768;
        out[s] = (int16_t)mix;
    }
    while (wi < nw) { write_reg(w[wi].reg, w[wi].val); wi++; }
    return n;
}
