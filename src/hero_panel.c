/* Status panel: power bar, lives, dynamite, score, level, bottom text row. */
#include "hero.h"

static void col_wr(uint16_t a, uint8_t v) { hw_wr(a, v); }

/* $85A1: draw POWER label and empty bar, colours, score, bottom row */
void s85A1(void)
{
    ry = 0x0A;
    rx = 0x25;
    ra = 0x46;
    do {
        M[0x06D1 + rx] = ra;
        col_wr(0xDAD1 + rx, ry);
        rx--;
    } while (rx != 5);
    do {
        ra = (uint8_t)(rx + 0x40);
        M[0x06D1 + rx] = ra;
        col_wr(0xDAD1 + rx, 9);
        rx--;
    } while (!NEG(rx));
    for (rx = 0x0B; !NEG(rx); rx--) col_wr(0xDB88 + rx, 9);
    s86ED();
    s9207();
}

/* $85D5: (unreferenced) alternate bottom row */
void s85D5(void)
{
    for (ry = 0x27; !NEG(ry); ry--) col_wr(0xDBC8 + ry, 9);
    ra = 0x56;
    for (ry = 9; !NEG(ry); ry--) { ra--; M[0x07DB + ry] = ra; }
    for (ry = 4; !NEG(ry); ry--) M[0x07D7 + ry] = ra;
}

/* $85F5: draw lives ($98 / 2 hero icons) */
void s85F5(void)
{
    for (rx = 0x0A; !NEG(rx); rx -= 2) {
        M[0x06FC + rx] = 0;
        M[0x06FD + rx] = 0;
        M[0x0725 + rx] = 0;
    }
    ra = 0;
    rx = M[Z_LIVES2];
    if (!rx) return;
    rx = 0;
    do {
        col_wr(0xDAFC + rx, 0x0E);
        M[0x06FC + rx] = 0x8D;
        rx++;
        col_wr(0xDAFC + rx, 0x0E);
        M[0x06FC + rx] = 0x8E;
        col_wr(0xDB24 + rx, 0x09);
        M[0x0724 + rx] = 0x8F;
        rx++;
    } while (rx != M[Z_LIVES2]);
}

/* $8631: clear dynamite icons, then $863F */
void s8631(void)
{
    for (rx = 0x0B; !NEG(rx); rx -= 2) {
        M[0x0710 + rx] = 0;
        M[0x0738 + rx] = 0;
    }
    s863F();
}

/* $863F: draw dynamite icons ($99 / 2) */
void s863F(void)
{
    rx = M[Z_DYN2];
    if (!rx) return;
    rx--;
    do {
        col_wr(0xDB10 + rx, 0x0A);
        col_wr(0xDB38 + rx, 0x0A);
        M[0x0710 + rx] = 0x90;
        M[0x0738 + rx] = 0x91;
        ra = 0x90;
        rx -= 2;
    } while (!NEG(rx));
}

/* $865E: power -1; when it runs out lose a life and restart the level */
void s865E(void)
{
    if (--M[Z_POWER] == 0 && !NEG(M[0x7F])) {
        ra = M[Z_LIVES2];
        if (ra) {
            M[Z_LIVES2] = (uint8_t)(ra - 2);
            s85F5();
        }
        s867B();
        s9B61();
        s8515();
        return;
    }
    s867B();
}

/* $867B: draw the power bar (32 chars, 4 steps per char) */
void s867B(void)
{
    ry = (uint8_t)(M[Z_POWER] >> 2);
    rx = 0;
    for (;;) {
        ry--;
        if (NEG(ry)) break;
        M[0x06D7 + rx] = 0x4A;
        if (++rx == 0x20) return;
    }
    ra = (uint8_t)((M[Z_POWER] & 3) + 0x46);
    M[0x06D7 + rx] = ra;
    if (++rx == 0x20) return;
    do {
        M[0x06D7 + rx] = 0x46;
    } while (++rx != 0x20);
}

/* decimal-mode ADC (NMOS, valid BCD operands) */
static void adc_bcd(uint8_t v)
{
    int al = (ra & 0x0F) + (v & 0x0F) + rc;
    if (al >= 0x0A) al = ((al + 0x06) & 0x0F) + 0x10;
    int a = (ra & 0xF0) + (v & 0xF0) + al;
    if (a >= 0xA0) a += 0x60;
    rc = a >= 0x100;
    ra = (uint8_t)a;
}

/* $86AB: add BCD (X:A) to the score; extra life every 20000; redraw */
void s86AB(void)
{
    if (NEG(M[Z_DEMO]) || NEG(M[0xA5])) return;
    rc = 0;
    adc_bcd(M[0x55]);
    M[0x55] = ra;
    ra = rx;
    adc_bcd(M[0x54]);
    M[0x54] = ra;
    if (rc) {
        ra = 0;
        adc_bcd(M[0x53]);
        M[0x53] = ra;
        if (rc) {                         /* score overflow: freeze display */
            M[0xA5] = 0xFF;
            for (rx = 0x0B; !NEG(rx); rx--) M[0x0788 + rx] = 0x4B;
            ra = 0x4B;
            return;
        }
        if (!(M[0x53] & 1) && M[Z_LIVES2] != 0x0C) {
            M[Z_LIVES2] += 2;
            s85F5();
        }
    }
    s86ED();
}

/* $86ED: draw the 6-digit score (2 chars per digit, leading zeros blank) */
void s86ED(void)
{
    ry = 0;
    rx = 0;
    M[0x56] = 0xFF;
    do {
        ra = (uint8_t)(M[0x53 + rx] >> 3);
        s8709();
        ra = (uint8_t)(M[0x53 + rx] << 1);
        s8709();
        rx++;
    } while (rx != 3);
}

/* $8709: one digit (A & $1E = digit * 2) */
void s8709(void)
{
    ra &= 0x1E;
    if (ra == 0 && NEG(M[0x56]) && ry != 0x0A) {
        M[0x0788 + ry] = 0;
        ry++;
        M[0x0788 + ry] = 0;
    } else {
        ra = (uint8_t)(ra + 0x6D);
        M[0x0788 + ry] = ra;
        ry++;
        ra = (uint8_t)(ra + 1);
        M[0x0788 + ry] = ra;
        ra = 0;
        M[0x56] = 0;
    }
    ry++;
}

/* $8734: "LEVEL:" + number (or PRO) */
void s8734(void)
{
    for (rx = 0x0E; !NEG(rx); rx--) col_wr(0xDB74 + rx, 0x0F);
    s873E();
}

/* $873E */
void s873E(void)
{
    rx = 0;
    ra = 0x85;
    do {
        M[0x0774 + rx] = ra;
        ra++;
        rx++;
    } while (rx != 8);
    ra = 0x14;
    if (!NEG(M[0xA6]) && !NEG(M[0xA7])) {
        ra = M[Z_LEVEL];
        if (NEG(M[Z_DEMO])) ra = M[0xB7];
    }
    ry = (uint8_t)(ra << 1);
    do {
        ra = M[0xA476 + ry];
        M[0x0774 + rx] = ra;
        rx++;
        M[0x0774 + rx] = (uint8_t)(ra + 1);
        ry++;
        rx++;
    } while (rx != 0x0C);
}

/* $91DD: (IRQ) attract mode: cycle the bottom text row every $B4 frames */
void s91DD(void)
{
    if (!NEG(M[0xBC])) {
        if (!NEG(M[Z_DEMO])) return;
        if (M[0x4E]) return;
        M[0x4E] = 0xB4;
        M[Z_BOTTOMPAGE]++;
    }
    M[0xBC] = 0;
    M[Z_BOTTOMPAGE] &= 3;
    if (M[Z_BOTTOMPAGE]) {
        for (rx = 0x27; !NEG(rx); rx--) col_wr(0xDBC0 + rx, 1);
        goto draw;
    }
    for (rx = 0x27; !NEG(rx); rx--) col_wr(0xDBC0 + rx, 9);
    col_wr(0xDBD8, 1);
draw:
    rx = M[0x922D + M[Z_BOTTOMPAGE]];
    for (ry = 0x27; !NEG(ry); ry--, rx--) M[0x07C0 + ry] = M[0xA930 + rx];
}

/* $9207: bottom row back to the logo */
void s9207(void)
{
    M[Z_BOTTOMPAGE] = 0;
    for (rx = 0x27; !NEG(rx); rx--) col_wr(0xDBC0 + rx, 9);
    col_wr(0xDBD8, 1);
    rx = M[0x922D];
    for (ry = 0x27; !NEG(ry); ry--, rx--) M[0x07C0 + ry] = M[0xA930 + rx];
}
