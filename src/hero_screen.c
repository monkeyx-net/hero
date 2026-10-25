/* Screen construction: rock bands, walls, decorations, colours, per-screen
 * objects. Mostly main-thread code; CYC() approximates the original timing so
 * a redraw takes a similar number of frames (it is visible on screen). */
#include "hero.h"

/* $877A: (main) build the current screen if a redraw was requested */
void s877A(void)
{
    if (!NEG(M[Z_REDRAW])) return;
    VIC_WR(0xD021, 0);
    M[0x834] = M[0x835] = M[0x838] = M[0x839] = M[0x83C] = M[0x83D] = 0;
    M[0xA9] = M[0xAA] = 0;
    SID_WR(0xD412, 0);
    for (rx = 0x27; !NEG(rx); rx--) hw_wr(0xDBC0 + rx, 0);
    M[0xBC] = 0xFF;
    CYC(400);
    s9408();
    s8D23();
    s8B91();
    M[0x6F] = 0;
    M[0x31] = 0;
    setptr(0x22, 0x0428); rx = 2; s886C();
    setptr(0x22, 0x04F0); rx = 1; s886C();
    setptr(0x22, 0x05B8); rx = 0; s886C();
    s8B59();
    s891A();
    M[0x6D] = 0;
    M[0x80] = 0xFF;
    if (!NEG(M[0x9B])) s8987();
    M[0x9B] = 0;
    s8DD2();
    M[0x5E] = M[0x5B];
    M[0x5F] = M[0x5C];
    ra = M[0x59];
    rx = M[0x5A];
    s8EE5();
    s8E9A();
    s9C93();
    s9DD1();
    if (NEG(M[0x74])) s8DD2();
    ra = 0xFF;
    s8E45();
    s8BE3();
    s8F37();
    M[0x82] = 0;
    s8A14();
    M[0x73] = M[0x72];
    s89BD();
    ry = 0;
    rx = M[Z_LEVEL];
    ra = M[Z_SCREEN];
    if (ra == M[0xA3D0 + rx] || !NEG(cmp(ra, 0x0A))) {
        rx &= 3;
        ry = M[0x859D + rx];
    }
    M[0x79] = ry;
    ra = 0;
    if (ry) ra = 5;
    M[0x0824] = M[0x0825] = ra;
    s8AFB();
    s967A();
    (void)VIC_RD(0xD01E);
    s9BDC();
    M[Z_REDRAW] = 0;
}

/* $886C: draw one rock band (5 char rows) at ($22); X = band 0..2 */
void s886C(void)
{
    ry = 0;
    M[0x2C] = rx;
    if (rx == 1 && !NEG(M[0x69])) s8DF6();
    else s8DFA();
    ry = 4;
    M[0x2B] = 4;
    M[0x6F] = 0;
    rx = M[0x2C];
    ra = ZPX(0x63, rx);
    s8DDE();
    rx = M[0x2C];
    M[0x2D] = ZPX(0x66, rx);
    s8906();
    rx = M[0x2C];
    if (rx == 1 && NEG(M[0x6A])) {
        ra = (uint8_t)(M[0x2B] + 8);
        M[0x2B] = ra;
        ry = ra;
        if (M[0x69] & 0x40) s8DFA();
        else s8DF6();
        M[0x2B] = (uint8_t)(M[0x2B] - 4);
        rx = M[0x2C];
        ra = ZPX(0x63, rx);
    } else {
        ra = ZPX(0x66, rx);
    }
    M[0x2D] = ra;
    s8906();
    rx = M[0x2C];
    if (rx == 1 && NEG(M[0x6A])) ra = ZPX(0x66, rx);
    else ra = ZPX(0x63, rx);
    M[0x2D] = ra;
    s8906();
    ra = (uint8_t)(M[0x2B] + 8);
    M[0x2B] = ra;
    ry = ra;
    if (M[0x2C] == 1) {
        if (NEG(M[0x6A])) return;
        if (!(M[0x69] & 0x40)) { s8DF6(); return; }
    }
    s8DFA();
}

/* $8906: next 8 columns from byte $2D, alternating mirror */
void s8906(void)
{
    ra = (uint8_t)(M[0x2B] + 8);
    M[0x2B] = ra;
    ry = ra;
    M[0x6F] ^= 0x80;
    ra = M[0x2D];
    s8DDE();
}

/* $891A: top row details on the first screen */
void s891A(void)
{
    if (M[Z_SCREEN]) {
        if (M[Z_SCREEN] == 1) s8B86();
        return;
    }
    ry = 4;
    setptr(0x22, 0x0428);
    ra = 0x81;
    s8DDE();
    ra = 0;
    for (rx = 5; !NEG(rx); rx--) hw_wr(0xD805 + rx, 0);
}

/* $8940: remember entry state when moving to screen A ($2A = old screen) */
void s8940(void)
{
    uint8_t a = ra;
    if (NEG(cmp(a, M[0x9D]))) {
        M[0x9A] = 3;
        if (M[0x2A] != M[0x9D]) return;
        rx = 1;
    } else if (a == M[0x9D]) {
        M[0x9A] = 2;
        if (!NEG(cmp(M[0x9D], M[0x2A]))) return;
        rx = 0;
    } else if (a == M[0x9C]) {
        M[0x9A] = 1;
        rx = 1;
    } else {
        M[0x9A] = 0;
        rx = 1;
    }
    ZPX(0x9C, rx) = M[0x2A];
    ZPX(0xA0, rx) = M[0x80];
    ZPX(0x9E, rx) = M[0x6E];
    ZPX(0xA2, rx) = M[0x6D];
}

/* $8987: restore the saved state of a revisited screen */
void s8987(void)
{
    ra = M[0x9A];
    if (!ra) return;
    if (ra == 3) {
        M[0x6E] = 0;
        M[0x74] = 0;
        M[0x60] = 0xC0;
        M[0x61] = 0xC0;
        return;
    }
    rx = (uint8_t)(ra - 1);
    M[0x6E] = ZPX(0x9E, rx);
    M[0x6D] = ZPX(0xA2, rx);
    ra = ZPX(0xA0, rx);
    M[0x80] = ra;
    if (!(ra & 0x20)) M[0x61] = 0xC0;
    ra = ZPX(0xA0, rx);
    if (!(ra & 0x10)) M[0x60] = 0xC0;
}

/* $89BD: miner (sprite 7) on the level's last screen */
void s89BD(void)
{
    VIC_WR(0xD015, VIC_RD(0xD015) & 0x7F);
    rx = M[Z_LEVEL];
    if (M[0xA3D0 + rx] != M[Z_SCREEN]) return;
    setptr(0x22, 0xA39F);
    ra = 7;
    s818B();
    if (M[0x6E] == 2) {
        M[0x28] = 0x25;
        rx = 0x0E;
        s9EF9();
        M[0x07FF] += 2;
        M[0x056E] = 0;
        M[0x056F] = 0;
    } else {
        M[0x0588] = 0;
        M[0x0589] = 0;
    }
    VIC_WR(0xD015, VIC_RD(0xD015) | 0x80);
    M[0x083C] = M[0x083D] = 3;
}

/* $8A14: raft / moving platform (sprite 6) on some deep screens */
void s8A14(void)
{
    M[0x082C] = 0;
    if (M[Z_SCREEN] < 0x0B) return;
    if (M[Z_LEVEL] < 9) return;
    if (M[0x66]) return;
    if (M[0x63] == 0xC0) {
        setptr(0x22, 0xA396);
    } else {
        if (M[Z_LEVEL] < 0x10) return;
        if (M[0x63] != 0xE0) return;
        M[0x082C] = M[0x082D] = 3;
        setptr(0x22, 0xA38D);
    }
    ra = 6;
    s818B();
    rx = 2;
    s9EDB();
    if (!NEG(cmp(ra, 0x50))) return;
    rx = 0x0C;
    M[0x28] = 0x28;
    s9EF9();
}

/* $8A6E: (timer) sprite 6 follows the hero horizontally (tentacle/raft) */
void s8A6E(void)
{
    rx = 2;
    s9EDB();
    M[0x2A] = M[0x28];
    rx = 0x0C;
    s9EDB();
    uint8_t d = cmp(ra, M[0x2A]);
    if (NEG(d)) {
        ra = (uint8_t)(ra + 1);
        if (!NEG(cmp(ra, 0x88))) return;
    } else {
        if (d == 0) return;
        ra = (uint8_t)(ra - 1);
        if (NEG(cmp(ra, 0x28))) return;
    }
    M[0x28] = ra;
    rx = 0x0C;
    s9EF9();
}

/* $8A9A: (timer) raft carrying the hero */
void s8A9A(void)
{
    if (NEG(cmp(M[0x3F], 0x95))) {
        M[0xB5] = 0;
        M[0x0830] = 0;
        M[0x0831] = 0;
        M[0x80] |= 0x40;
        return;
    }
    rx = 0x0C;
    s9EDB();
    ra = (uint8_t)(ra + M[0x73]);
    if (NEG(cmp(ra, 0x88)) && !NEG(cmp(ra, 0x28))) {
        /* in range */
    } else {
        M[0x73] = (uint8_t)((M[0x73] ^ 0xFF) + 1);
        ra = (uint8_t)(M[0x28] + M[0x73] + M[0x73]);
    }
    M[0x28] = ra;
    M[0x2A] = ra;
    rx = 0x0C;
    s9EF9();
    rx = 2;
    M[0x2A]++;
    M[0x28] = M[0x2A];
    M[0x29] = 0x95;
    s9EF5();
    rx = 4;
    M[0x28] = M[0x2A];
    s9EF9();
    if (!NEG(cmp(5, M[0x95]))) M[0x95] = 5;
}

/* $8AFB: fill the band row ($0680) with animated chars $38-$3F, colour it */
void s8AFB(void)
{
    rx = 0x27;
    ry = 3;
    for (;;) {
        do {
            ra = ry & 7;
            ry = ra;
            M[0x0680 + rx] = (uint8_t)(ra + 0x38);
            rx--;
            if (NEG(rx)) goto colours;
            ry--;
        } while (!NEG(ry));
        ry = 0;
        do {
            ra = ry & 7;
            ry = ra;
            M[0x0680 + rx] = (uint8_t)(ra + 0x38);
            rx--;
            if (NEG(rx)) goto colours;
            ry++;
        } while (ry != 8);
        ry = 7;
    }
colours:
    for (rx = 0x27; !NEG(rx); rx--) {
        ry = M[0x0658 + rx] & 0x3F;
        ra = M[0x5A];
        if (NEG(cmp(ry, 0x11)) || !NEG(cmp(ry, 0x37))) ra = M[0x79];
        hw_wr(0xDA80 + rx, ra);
    }
    CYC(2500);
}

/* $8B45: (timer) animate the band row */
void s8B45(void)
{
    for (rx = 0x27; !NEG(rx); rx--)
        M[0x0680 + rx] = (uint8_t)(((M[0x0680 + rx] - 0x37) & 7) + 0x38);
    CYC(1300);
}

/* $8B59: row 0 edge decoration above solid rock in row 1 */
void s8B59(void)
{
    rx = 0x27;
    ra = 0x22;
    do {
        ry = M[0x0428 + rx] & 0x3F;
        if (!NEG(cmp(ry, 0x11)) && NEG(cmp(ry, 0x38))) M[0x0400 + rx] = ra;
        else M[0x0400 + rx] = 0;
        ra--;
        if (ra == 0x1F) ra = 0x22;
        rx--;
    } while (!NEG(rx));
    CYC(1200);
}

/* $8B86: colour row 0 with $59 */
void s8B86(void)
{
    for (rx = 0x27; !NEG(rx); rx--) hw_wr(0xD800 + rx, M[0x59]);
    ra = M[0x59];
}

/* $8B91: levels 14+: flashing wall flag */
void s8B91(void)
{
    rx = 0;
    M[0x75] = 0;
    if (M[Z_LEVEL] >= 0x0D && M[0x67] == 0x7F && M[0x64] == 0xFF) {
        M[0x76] = 1;
        M[0x67] = 0x0F;
        rx = 0xFF;
    }
    M[0x74] = rx;
}

/* $8BB4: (IRQ) animate the flashing wall */
void s8BB4(void)
{
    if (!M[0x74]) return;
    if (--M[0x76]) return;
    M[0x76] = 8;
    M[0x75] = (M[0x75] + 1) & 7;
    rx = M[0x75];
    ra = M[0x8BDB + rx];
    s8BCD();
}

/* $8BCD: draw the flashing wall pattern A at $04F0 */
void s8BCD(void)
{
    M[0x22] = 0xF0;
    M[0x23] = 0x04;
    ry = 0x10;
    s8DDE();
}

/* $8BE3: enemies (sprites 4, 5) and lantern (sprite 6) for this screen */
void s8BE3(void)
{
    ra = M[0x60]; s8C3D(); ra = 4; M[0x22] = rx; M[0x23] = ry; s818B();
    ra = M[0x61]; s8C3D(); ra = 5; M[0x22] = rx; M[0x23] = ry; s818B();
    if (!NEG(cmp(M[Z_LEVEL], 0x0A))) {
        M[0x0810] >>= 1;
        M[0x0814] >>= 1;
    }
    setptr(0x22, 0xA36A);
    ra = 6;
    s818B();
    ry = 2;
    rx = 6;
    do {
        M[0x28] = (uint8_t)((M[0x60 + ry] & 0xFC) + 0x0D);
        uint8_t n = rx;
        rx = (uint8_t)(n << 1);
        push(n);
        M[0x29] = M[0x8C3A + ry];
        s9EF5();
        rx = pull();
        rx--;
        ry--;
    } while (!NEG(ry));
}

/* $8C3D: enemy record pointer for type A & 3 -> X/Y */
void s8C3D(void)
{
    switch (ra & 3) {
    case 0: rx = 0x46; break;
    case 1: rx = 0x4F; break;
    case 2: rx = 0x58; break;
    default: rx = 0x61; break;
    }
    ry = 0xA3;
}

/* $8C5F: (IRQ) move enemies every other frame */
void s8C5F(void)
{
    M[0x82]++;
    if (!(M[0x82] & 1)) return;
    ra = M[0x7B];
    if (ra == 0) ra = 0xFF;
    else if (ra == 0x20) ra = 2;
    else if (ra == 0x10) ra = 0;
    else ra = 0xFF;
    M[0x2D] = ra;
    M[0x2C] = (uint8_t)((M[0x82] & 0x3C) >> 2);
    if (VIC_RD(0xD015) & 0x10) { rx = 0; ra = M[0x0813]; ry = M[0x2C]; s8CAA(); }
    if (VIC_RD(0xD015) & 0x20) { rx = 2; ra = M[0x0817]; ry = M[0x2C]; s8CAA(); }
}

/* $8CAA: move enemy X (0/2) by frame A, phase Y */
void s8CAA(void)
{
    if (rx == M[0x2D]) return;
    if (ra == 0xC3) {                         /* spider: bob up/down */
        M[0x45 + rx] = (uint8_t)(M[0x45 + rx] + M[0xA3FE + ry]);
        return;
    }
    if (ra != 0xC7) return;
    M[0x45 + rx] = (uint8_t)(M[0x45 + rx] + M[0xA43E + ry]);   /* bat/moth */
    ra = M[Z_LEVEL];
    if (ra >= 0x10) s8CF5();
    if (ra >= 0x08) s8CF5();
    M[0x2A] = M[0xA40E + ry];
    ra = (uint8_t)(rx + 8);
    M[0x2B] = ra;
    rx = ra;
    s9EDB();
    M[0x28] = (uint8_t)(M[0x2A] + M[0x28]);
    rx = M[0x2B];
    s9EF9();
}

/* $8CF5: Y += $10 (faster movement table on later levels) */
void s8CF5(void)
{
    ry = (uint8_t)(ry + 0x10);
}

/* $8D23: fetch the screen description for level $58 screen $57 */
void s8D23(void)
{
    s8DC8();
    M[0x60] = M[0xB800 + ry];
    M[0x61] = M[0xB900 + ry];
    ra = M[0x62] = M[0xBA00 + ry];
    rx = 1;
    if (ra & 1) rx = 0xFF;
    M[0x72] = rx;
    ra = 0xFF;
    rx = M[Z_SCREEN];
    if (rx && rx < 0x0B) ra = M[0xBBFF + ry];
    M[0x65] = ra;
    M[0x64] = M[0xBB00 + ry];
    M[0x63] = M[0xBC00 + ry];
    ra = 0xFF;
    rx = M[Z_SCREEN];
    if (rx && rx < 0x0B) ra = M[0xBDFF + ry];
    M[0x68] = ra;
    M[0x67] = M[0xBD00 + ry];
    M[0x66] = M[0xBE00 + ry];
    M[0x2B] = M[0xBF00 + ry];
    M[0x2A] = (uint8_t)(M[0x2B] >> 2);
    M[0x6A] = (M[0x2B] & 1) ? 0x80 : 0x00;
    ra = 0;
    if (M[Z_SCREEN] < 0x0A) ra = 0xC0;
    rx = M[0x2A];
    if (rx == 2) ra = 0x80;
    if (rx == 0x26) ra = 0x40;
    M[0x69] = ra;
    rx = M[Z_LEVEL];
    ra = M[Z_SCREEN];
    if (NEG(cmp(ra, 0x0A)) && ra == M[0xA3D0 + rx]) M[0x69] = 0xC0;
    M[0x6B] = (uint8_t)((M[0x2B] << 6) & 0x80);
    M[0x6E] = (uint8_t)(M[0x2B] >> 2);
    ra = M[0x6E];
    CYC(300);
}

/* $8DC8: Y = level start index + screen */
void s8DC8(void)
{
    ry = M[Z_LEVEL];
    ry = (uint8_t)(M[0xA3A8 + ry] + M[Z_SCREEN]);
    ra = ry;
}

/* $8DD2: colour the middle band (2 = water/lava screens, else $59) */
void s8DD2(void)
{
    ra = 2;
    if (!NEG(M[0x6B])) ra = M[0x59];
    s8E90();
}

/* $8DDE: draw byte A as two 4-column nibbles (mirrored when $6F bit 7) */
void s8DDE(void)
{
    if (NEG(M[0x6F])) s9F39();
    M[0x2E] = ra;
    ra = (uint8_t)(ra >> 4);
    s8DFC();
    ra = M[0x2E] & 0x0F;
    s8DFC();
}

void s8DF6(void) { ra = 0; s8DFC(); }        /* $8DF6: empty nibble */
void s8DFA(void) { ra = 0x0F; s8DFC(); }     /* $8DFA: solid nibble */

/* $8DFC: draw nibble A as 4 columns x 5 rows at ($22)+Y */
void s8DFC(void)
{
    push(ra);
    M[0x31] ^= 0xFF;
    ra = pull();
    rx = ra;
    rx = M[0xA3E4 + rx];
    M[0x2F] = 4;
    do {
        s8E1A();
        ra = (uint8_t)(ry - 0xC7);
        ry = ra;
    } while (--M[0x2F]);
}

/* $8E1A: one column of 5 chars from the pattern tables */
void s8E1A(void)
{
    M[0x30] = 5;
    do {
        if (M[0x22] == 0xF0 && NEG(M[0x6B])) ra = M[0xB700 + rx];
        else {
            ra = M[0xB500 + rx];
            if (!NEG(M[0x31])) ra = M[0xB600 + rx];
        }
        stiy(0x22, ra);
        rx++;
        ry = (uint8_t)(ry + 0x28);
    } while (--M[0x30]);
    CYC(230);
}

/* $8E45: draw the destructible wall (A: $FF = full, $0F = burning, 0 = gone) */
void s8E45(void)
{
    M[0x2A] = ra;
    rx = 0xDC;
    if (ra == 0xFF) rx = 0xC8;
    else if (ra == 0x0F) rx = 0xD2;
    ry = M[0x6E];
    if (!ry) return;
    if (NEG(M[0x69]) && ry == 2) return;
    if ((M[0x69] & 0x40) && ry == 0x26) return;
    M[0x2B] = ry;
    if (M[0x2A] == 0) M[0x6E] = 0;
    push(rx);
    s8F0B();
    rx = pull();
    ry = M[0x2B];
    M[0x22] = 0xF0;
    M[0x23] = 0x04;
    s8E1A();
    ry = (uint8_t)(ry - 0xC7);
    s8E1A();
}

/* $8E90: fill colour rows 6-10 ($D8F0-$D9B7) with A */
void s8E90(void)
{
    M[0x22] = 0xEF;
    M[0x23] = 0xD8;
    s8EDD();
}

/* $8E9A: colour the three bands (X = rock colour); dark when lights are out */
void s8E9A(void)
{
    M[0x2A] = rx;
    if (!NEG(M[0x6D])) {
        M[0x5E] = M[0x5B];
        M[0x5F] = M[0x5C];
    } else {
        ra = 0;
        if (NEG(M[0x6C])) ra = 0x0B;
        M[0x2A] = ra;
        M[0x5E] = ra;
        M[0x5F] = ra;
    }
    M[0x22] = 0x27; M[0x23] = 0xD8; s8EDD();
    if (!NEG(M[0x6B])) { M[0x22] = 0xEF; M[0x23] = 0xD8; s8EDD(); }
    ra = M[0x2A];
    M[0x22] = 0xB7; M[0x23] = 0xD9;
    s8EDD();
}

/* $8EDD: fill ($22)+1..$C8 with A */
void s8EDD(void)
{
    for (ry = 0xC8; ry; ry--) stiy(0x22, ra);
    CYC(2300);
}

/* $8EE5: lights out on a water/lava screen: darken the empty middle cells */
void s8EE5(void)
{
    if (!NEG(M[0x6D]) || !NEG(M[0x6B]) || NEG(M[0x74])) return;
    for (ry = 0xC8; ry; ry--) {
        ra = M[0x04EF + ry] & 0x3F;
        if (NEG(cmp(ra, 0x11))) {
            ra = 0;
            if (NEG(M[0x6C])) ra = 0x0B;
            hw_wr(0xD8EF + ry, ra);
        }
    }
    CYC(3000);
}

/* $8F0B: colour the 2x5 wall cells at column $2B */
void s8F0B(void)
{
    ra = M[0x2B];
    if (!ra) return;
    ra = M[0x5E];
    if (rx != 0xDC) {
        ra = 2;
        if (!NEG(M[0x6B])) {
            ra = 0x0F;
            if (NEG(M[0x6D])) ra = 0;
        }
    }
    for (rx = 4; !NEG(rx); rx--) {
        hw_wr(0xD8F0 + ry, ra);
        ry++;
        hw_wr(0xD8F0 + ry, ra);
        ry = (uint8_t)(ry + 0x27);
    }
}

/* $8F37: sprite colours (all dark grey when the lights are out) */
void s8F37(void)
{
    if (NEG(M[0x6D])) {
        for (rx = 9; !NEG(rx); rx--)
            if (rx != 5) VIC_WR(0xD025 + rx, 0x0B);
        ra = 0x0B;
        return;
    }
    for (rx = 5; !NEG(rx); rx--) VIC_WR(0xD025 + rx, M[0x8F56 + rx]);
}

/* $9C93: shade the rock edges (7 passes from the table at $9CD3/$9CE1) */
void s9C93(void)
{
    for (rx = 3; !NEG(rx); rx--) ZPX(0x22, rx) = M[0x9CEF + rx];
    setptr(0x34, 0x9CD3);
    if (NEG(M[0x6B])) setptr(0x34, 0x9CE1);
    ry = 0;
    do {
        M[0x32] = ldiy(0x34); ry++;
        M[0x33] = ldiy(0x34); ry++;
        push(ry);
        for (ry = 8; !NEG(ry); ry--) M[0x26 + ry] = ldiy(0x32);
        s9D40();
        ry = pull();
    } while (ry != 0x0E);
}

/* $9D40: one pass over $2E rows of 40 cells */
void s9D40(void)
{
    M[0x30] = 0x28;
    do {
        ry = 0;
        M[0x31] = 0;
        while (M[0x26] | M[0x27]) {
            ra = ldiy(0x22) & 0x3F;
            if (NEG(cmp(ra, 0x11))) s9DA5();
            else s9D76();
            ry++;
            CYC(70);
            if (!NEG(cmp(ry, 0x28))) break;
        }
        for (rx = 0; rx != 4; rx += 2) { ry = 0x0E; s9F13(); }
    } while (--M[0x2E]);
}

/* $9D76: solid cell: pick an edge char depending on neighbours */
void s9D76(void)
{
    if (NEG(cmp(ldiy(0x26) & 0x3F, 0x11))) {
        if (NEG(cmp(ldiy(0x24) & 0x3F, 0x11))) {
            ra = M[0x2C];
            stiy(0x22, ra);
            if (ra) return;
        }
        ra = ldiy(0x2A);                       /* $9D8C */
        stiy(0x22, ra);
        if (ra) return;
    }
    if (!NEG(cmp(ldiy(0x24) & 0x3F, 0x11))) return;   /* $9D92 */
    rx = 7;
    s9E8B();
    ra = M[0xAAE8 + rx];
    stiy(0x22, ra);
}

/* $9DA5: empty cell: random dust char; edge below */
void s9DA5(void)
{
    rx = 3;
    s9E8B();
    ra = M[0xAAF8 + rx];
    stiy(0x22, ra);
    if (NEG(cmp(ldiy(0x24) & 0x3F, 0x11))) return;
    int done = 0;
    if (NEG(cmp(ldiy(0x28) & 0x3F, 0x11))) {
        ra = M[0x2D];
        stiy(0x24, ra);
        done = ra != 0;
    }
    if (!done) {                               /* $9DC5 */
        rx = 7;
        s9E8B();
        ra = M[0xAAF0 + rx];
        stiy(0x24, ra);
    }
    ry++;
}

/* $9DD1: walls between bands (4 passes from the table at $9DFB) */
void s9DD1(void)
{
    setptr(0x3A, 0x9DFB);
    ry = 0;
    do {
        M[0x38] = ldiy(0x3A); ry++;
        M[0x39] = ldiy(0x3A); ry++;
        push(ry);
        for (ry = 0x15; !NEG(ry); ry--) M[0x22 + ry] = ldiy(0x38);
        s9E03();
        ry = pull();
    } while (ry != 8);
}

/* $9E03 */
void s9E03(void)
{
    for (ry = 0; ry != 0x28; ry++) {
        CYC(60);
        if (NEG(cmp(ldiy(0x22) & 0x3F, 0x11))) continue;
        if (!NEG(cmp(ldiy(0x24) & 0x3F, 0x11))) continue;
        stiy(0x24, ldiy(0x2A));
        stiy(0x26, ldiy(0x2C));
        ra = M[0x5E];
        stiy(0x32, ra);
        stiy(0x34, ra);
        if (NEG(M[0x30])) continue;
        stiy(0x36, ra);
        stiy(0x28, ldiy(0x2E));
    }
}

/* $9E8B: X = ((raster ROR 3) ^ Y) & X - cheap random from the beam position */
void s9E8B(void)
{
    ra = VIC_RD(0xD012);
    for (int i = 0; i < 3; i++) {
        int c = ra & 1;
        ra = (uint8_t)(ra >> 1 | rc << 7);
        rc = c;
    }
    M[0x32] = ry;
    ra ^= M[0x32];
    M[0x32] = rx;
    ra &= M[0x32];
    rx = ra;
}
