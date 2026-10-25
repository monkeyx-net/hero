/* Input, hero movement/physics, laser, dynamite, map collision, screen exits. */
#include "hero.h"

/* $8F5C: (IRQ) read both joysticks; accept a value when stable for 2 reads */
void s8F5C(void)
{
    ry = 0;
    for (rx = 1; !NEG(rx); rx--) {
        ra = (uint8_t)(cia1_rd(rx) ^ 0xFF);
        if (ra == ZPX(0x92, rx)) {
            ZPX(0x90, rx) = ra;
            ry = 0xFF;
        }
        M[0x51] = ry;
        ZPX(0x92, rx) = ra;
    }
}

/* $8F75: (main) dispatch joystick changes to the handlers in ($85)/($87) */
void s8F75(void)
{
    M[0x26] = 1;
    do {
        push(M[0x26]);
        s8F8E();
        M[0x26] = pull();
        push(M[0x26]);
        s8FC0();
        M[0x26] = pull();
        M[0x26]--;
        CYC(60);
    } while (!NEG(M[0x26]));
}

/* $8F8E: direction changed -> $24 = direction code, call ($85) */
void s8F8E(void)
{
    rx = M[0x26];
    ra = ZPX(0x90, rx) & 0x0F;
    if (ra == ZPX(0x8C, rx)) return;
    ZPX(0x8C, rx) = ra;
    ry = ra;
    M[0x24] = M[0x8FB0 + ry];
    if (!(M[0x85] | M[0x86])) return;
    call_addr(ptr(0x85));
}

/* $8FC0: fire changed -> $24 = 1 pressed / $FF released, call ($87) */
void s8FC0(void)
{
    rx = M[0x26];
    ra = ZPX(0x90, rx) & 0x10;
    if (ra == ZPX(0x8E, rx)) return;
    ZPX(0x8E, rx) = ra;
    ra = ZPX(0x8E, rx);
    rx = ra ? 1 : 0xFF;
    M[0x24] = rx;
    if (!(M[0x87] | M[0x88])) return;
    call_addr(ptr(0x87));
}

/* $8FE8: direction handler: set dx/dy ($89/$8A), facing, then dynamite check */
void s8FE8(void)
{
    ra = 0;
    ry = 8;
    M[0x89] = 0;
    M[0x8A] = 0;
    ra = M[0x24];
    if (!NEG(ra)) {
        rx = (uint8_t)(ra << 1);
        ra = M[0x9023 + rx];
        M[0x89] = ra;
        if (ra) {
            if (NEG(ra)) ry = 0xF8;
            M[0x8B] = ra;
            if (M[0x0830] == 0) M[0x0845] = ry;
            ra = NEG(M[0x8B]) ? 0xA4 : 0xA0;
            M[0x0807] = ra;
            M[0x07F9] = ra;
        }
        rx++;
        M[0x8A] = M[0x9023 + rx];
    }
    s9300();
}

/* $9033: fire handler: laser (sprite 3) on while fire is held */
void s9033(void)
{
    rx = 3;
    ra = M[0x81FE + rx];
    if (NEG(M[0x24])) {
        ra ^= 0xFF;
        ra &= VIC_RD(0xD015);
        VIC_WR(0xD015, ra);
    }
    ra |= VIC_RD(0xD015);
    VIC_WR(0xD015, ra);
}

/* $904B: (IRQ) every 4th frame: keyboard. Attract: F3 = start level, F1 = start.
 * In game: RUN/STOP = pause. */
void s904B(void)
{
    if (--M[0xB0]) return;
    M[0xB0] = 4;
    s90A9();
    if (NEG(M[Z_DEMO])) {
        ra = M[0xAE];
        if (ra == 0xDF) {                     /* F3 */
            ra = (uint8_t)(M[Z_STARTLVL] + 1);
            if (!NEG(cmp(ra, 5))) ra = 0;
            M[Z_STARTLVL] = ra;
            ra = (uint8_t)(ra << 2);
            M[0xB7] = ra;
            M[0xA7] = ra == 0x10 ? 0xFF : 0x00;   /* 5th choice = PRO */
            s873E();
            return;
        }
        if (ra == 0xEF) M[Z_START] = 0xFF;   /* F1 */
        return;
    }
    if (NEG(M[Z_DEMO])) return;
    if (M[0xAF] != 0x7F) return;              /* RUN/STOP */
    M[Z_PAUSE] ^= 0xFF;
    if (!M[Z_PAUSE]) return;
    SID_WR(0xD404, 0);
    SID_WR(0xD40B, 0);
    SID_WR(0xD412, 0);
}

/* $90A9: scan keyboard columns 7 and 0 into $AE/$AF (edge detected) */
void s90A9(void)
{
    cia1_wr(0x02, 0xFF);
    cia1_wr(0x00, 0x7F);
    ra = cia1_rd(0x01);
    rx = 1;
    s90CE();
    cia1_wr(0x00, 0xFE);
    ra = cia1_rd(0x01);
    rx = 0;
    s90CE();
    cia1_wr(0x02, 0x00);
}

/* $90CE */
void s90CE(void)
{
    if (ra == ZPX(0xAC, rx)) ra = 0xFF;
    else ZPX(0xAC, rx) = ra;
    ZPX(0xAE, rx) = ra;
}

/* $9103: (IRQ) attract mode joystick from the per-screen demo table */
void s9103(void)
{
    setptr(0x22, 0x9154);
    if (M[Z_LEVEL] == 3) setptr(0x22, 0x915A);
    else if (M[Z_LEVEL] == 4) setptr(0x22, 0x9157);
    ry = M[Z_SCREEN];
    M[0x2B] = ldiy(0x22);
    if (!NEG(M[0x6C])) {
        if (M[0x6E] == 0) goto out;
        s92E3();
        if (!NEG(cmp(ra, 3))) goto out;
        ra = M[Z_FRAME] & 4;
        if (ra) ra = 2;
        M[0x2B] = ra;
        if (ra) goto out;
    }
    M[0x2B] ^= 0x0C;
out:
    M[Z_JOY2] = M[0x2B];
}

/* $9162: (IRQ) blank the screen after a long idle time */
void s9162(void)
{
    if (!NEG(M[Z_DEMO]) && ((M[Z_JOY2] | M[Z_JOY1]) & 0x1F)) {
        VIC_WR(0xD011, VIC_RD(0xD011) | 0x10);
        M[0xB9] = M[0xBA] = M[0xBB] = 0;
        return;
    }
    if (!NEG(cmp(M[0xBB], 3))) {
        VIC_WR(0xD011, VIC_RD(0xD011) & 0xEF);
        SID_WR(0xD404, 0);
        SID_WR(0xD40B, 0);
        SID_WR(0xD412, 0);
        return;
    }
    if (++M[0xB9] == 0 && ++M[0xBA] == 0) M[0xBB]++;
}

/* $91AD: (main) F1 pressed: start a new game */
void s91AD(void)
{
    if (!NEG(M[Z_START])) return;
    s9498();
    M[Z_START] = 0;
    M[Z_DEMO] = 0;
    M[Z_PAUSE] = 0;
    M[0x53] = M[0x54] = M[0x55] = 0;
    s86ED();
    M[Z_LIVES2] = 6;
    s85F5();
    M[Z_LEVEL] = (uint8_t)(M[Z_STARTLVL] << 2);
    s9207();
    M[0xB3] = 0xFF;
    s8515();
}

/* $9231: (IRQ) laser length / burning through the destructible wall */
void s9231(void)
{
    M[0x2C] = 0;
    if (!(VIC_RD(0xD015) & 0x08)) { rx = 0xF0; ra = 0xF9; goto set; }
    if (M[0x6E] == 0) goto full;
    if (NEG(cmp(M[0x43], 0x5F))) goto full;
    if (!NEG(cmp(M[0x43], 0x87))) goto full;
    s92E3();
    M[0x2B] = ra;
    if ((M[0x8B] ^ M[0x2A]) & 0x80) goto full;
    M[0x2C] = 0xFF;
    if (!NEG(M[0x8B])) M[0x2B]--;
    ra = M[0x2B];
    if (NEG(cmp(ra, 4))) { rx = 0xF0; ra = 0xF9; goto set; }
    if (NEG(cmp(ra, 6))) { rx = 0xED; ra = 0xF1; goto set; }
full:
    rx = 0xEA;
    ra = 0xED;
set:
    M[0xA8] = ra;
    M[0x080F] = rx;
    if (!NEG(M[0x2C])) return;
    if (!NEG(cmp(M[0x2B], 7))) return;
    rc = 0;
    ra = M[0xA9];
    adc(1);
    M[0xA9] = ra;
    if (!rc) return;
    if (!NEG(M[0xAA])) {                       /* first time: wall starts burning */
        M[0xAA] = 0xFF;
        ra = 0x0F;
        s8E45();
        s83B5();
        return;
    }
    setptr(0x22, 0xA377);                      /* wall gone: debris sprite */
    ra = 7;
    s818B();
    M[0x28] = (uint8_t)((M[0x6E] << 2) + 0x0C);
    rx = 0x0E;
    s9EF9();
    ra = 0x75;
    rx = 0;
    s86AB();
    ra = 0;
    s8E45();
    s838F();
    M[0x082A] = 0xEE;
    M[0x082B] = 0x93;
    M[0x0829] = M[0x0828] = 0x1E;
}

/* $92E3: |wall column - hero column| -> A ($2A keeps the signed difference) */
void s92E3(void)
{
    rx = 2;
    s9EDB();
    M[0x2A] = (uint8_t)((uint8_t)(M[0x28] - 0x0C) >> 2);
    rc = 1;
    ra = M[0x6E];
    sbc(M[0x2A]);
    M[0x2A] = ra;
    if (NEG(ra)) {
        ra ^= 0xFF;
        rc = 0;
        adc(1);
    }
}

/* $9300: pushing down while standing: light a stick of dynamite */
void s9300(void)
{
    if (M[0x8A] == 0 || NEG(M[0x8A])) return;
    if (M[0x89]) return;
    if (M[0x6C]) return;
    rx = M[Z_LEVEL];
    if (M[0xA3D0 + rx] == M[Z_SCREEN]) return;
    if (M[0x96]) return;
    if (!NEG(M[0x83])) return;
    if (M[Z_DYN2] == 0) return;
    M[Z_DYN2] -= 2;
    s8631();
    setptr(0x22, 0xA384);
    ra = 7;
    s818B();
    M[0x6C] = 0xFF;
    if (M[Z_SPRXHI] & 2) M[Z_SPRXHI] |= 0x80;
    else M[Z_SPRXHI] &= 0x7F;
    M[0x4A] = M[0x3E];
    M[0x4B] = (uint8_t)(M[0x3F] + 0x0D);
    s967A();
    s83DB();
    M[0x082A] = 0x7C;
    M[0x082B] = 0x93;
    M[0x0828] = M[0x0829] = 0x1E;
    if (NEG(M[0x6D])) {
        s8E9A();
        s8EE5();
    }
}

/* $937C: (timer) dynamite explodes */
void s937C(void)
{
    M[0x082A] = 0xA4;
    M[0x082B] = 0x93;
    M[0x0829] = 1;
    M[0x78] = 3;
    ra = 7;
    setptr(0x22, 0xA380);
    s81E5();
    M[0x5D] = 1;
    s838F();
}

/* $93A4: (timer) explosion flash, then damage */
void s93A4(void)
{
    rx = M[0x78];
    if (!NEG(rx)) {
        M[0x5D] = M[0x93EA + rx];
        M[0x78]--;
        M[0x0829] = 1;
        return;
    }
    M[0x6C] = 0;
    M[0x5D] = 0;
    if (NEG(M[0x6D])) {
        s8E9A();
        s8EE5();
    }
    M[0x082A] = 0xEE;
    M[0x082B] = 0x93;
    s942D();
    s944B();
    ra = 7;
    setptr(0x22, 0xA37C);
    s81E5();
    M[0x4B] = (uint8_t)(M[0x4B] - 0x10);
}

/* $93EE: (timer) clear sprite 7 and park it */
void s93EE(void)
{
    M[0x0828] = M[0x0829] = 0;
    M[0x082A] = 0x44;
    M[0x082B] = 0x9F;
    VIC_WR(0xD015, VIC_RD(0xD015) & 0x7F);
    s9408();
}

/* $9408: sprite 7 X = $C0 (off to the side) */
void s9408(void)
{
    rx = 0x0E;
    M[0x28] = 0xC0;
    s9EF9();
}

/* $9412: clear timer 2 and sprite 7 */
void s9412(void)
{
    M[0x0828] = M[0x0829] = 0;
    M[0x082A] = 0x44;
    M[0x082B] = 0x9F;
    VIC_WR(0xD015, VIC_RD(0xD015) & 0x7F);
}

/* $942D: explosion next to the wall destroys it */
void s942D(void)
{
    M[0x2B] = 0x6E;
    ra = M[0x6E];
    if (!ra) { s9412(); return; }
    M[0x2A] = ra;
    s946C();
    if (!NEG(M[0x2A])) { s9412(); return; }
    ra = 0;
    s8E45();
    ra = 0x75;
    rx = 0;
    s86AB();
}

/* $944B: explosion next to the hero kills him */
void s944B(void)
{
    rx = 2;
    s9EDB();
    M[0x2B] = M[0x29];
    M[0x2A] = (uint8_t)((uint8_t)(M[0x28] - 0x0C) >> 2);
    s946C();
    if (!NEG(M[0x2A])) return;
    s93EE();
    M[0x7D] = 0xFF;
}

/* $946C: $2A = $FF if the explosion (sprite 7) is within 4 columns of column
 * $2A on row $2B */
void s946C(void)
{
    rx = 0x0E;
    s9EDB();
    ra = (uint8_t)(M[0x29] - 0x0D);
    if (ra == M[0x2B]) {
        ra = (uint8_t)((uint8_t)(M[0x28] - 0x0C) >> 2);
        rc = 1;
        sbc(M[0x2A]);
        if (NEG(ra)) {
            ra ^= 0xFF;
            rc = 0;
            adc(1);
        }
        ra = NEG(cmp(ra, 4)) ? 0xFF : 0x00;
    } else {
        ra = 0;
    }
    M[0x2A] = ra;
}

/* $9498: finish a pending dynamite sequence immediately (screen change/death) */
void s9498(void)
{
    while (M[0x0828]) {
        ra = M[0x082A];
        if (ra != 0x7C && ra != 0xA4) ra = 0xEE;
        rx = 0x93;
        M[0x24] = ra;
        M[0x25] = rx;
        call_addr(ptr(0x24));
    }
}

/* $94C4: (IRQ) propeller: spin up while pushing up, spin down otherwise */
void s94C4(void)
{
    if (!NEG(M[0x94])) {
        if (--M[0x94] == 0) {
            M[0x94] = 3;
            ry = M[0x95];
            if (NEG(M[0x8A])) {
                if (ry != 0x0F) ry++;
            } else if (ry) {
                ry--;
            }
            M[0x95] = ry;
            M[0x0847] = M[0x9504 + ry];
            M[0x0804] = M[0x9514 + ry];
            M[0xAB] = 3;
        }
    }
    if (M[0x89]) return;
    rx = 2;
    s9EDB();
    if (!(ra & 3)) M[0x0845] = 0;
}

/* $9524: (IRQ) hero physics */
void s9524(void)
{
    rx = 2;
    s9EDB();
    s9539();
    s9563();
    s95AE();
    s9619();
    s9632();
}

/* $9539: horizontal move, undone on collision */
void s9539(void)
{
    M[0x83] = 0;
    M[0x32] = M[0x28];
    M[0x23] = M[0x28];
    rx = M[0x0844];
    ry = M[0x0845];
    s9598();
    M[0x28] = ra;
    M[0x0844] = M[0x22];
    s96A9();
    if (M[0x83]) M[0x28] = M[0x32];
    M[0x83] = 0;
}

/* $9563: vertical move; on collision snap back and halve the speed */
void s9563(void)
{
    M[0x23] = M[0x29];
    rx = M[0x0846];
    ry = M[0x0847];
    s9598();
    M[0x29] = ra;
    M[0x0846] = M[0x22];
    s96A9();
    if (!M[0x83]) return;
    M[0x29] = M[0x3F];
    rc = 0;
    ra = M[0x0847];
    if (ra == 0x08 || ra == 0xF8) return;
    if (NEG(M[0x0847])) rc = 1;
    int c = M[0x0847] & 1;
    M[0x0847] = (uint8_t)(M[0x0847] >> 1 | rc << 7);
    rc = c;
}

/* $9598: fixed point step: ($22 frac, $23 int) += sign-extended Y << 5 */
void s9598(void)
{
    M[0x22] = rx;
    M[0x24] = ry;
    rx = 2;
    s9F23();
    ry = 5;
    s9F2E();
    rx = 0;
    ry = 2;
    s9F13();
}

/* $95AE: leaving the screen edge moves to the neighbouring screen */
void s95AE(void)
{
    ry = 0;
    for (rx = 1; !NEG(rx); rx--) {
        rc = 1;
        ra = ZPX(0x28, rx);
        sbc(M[0x9611 + ry]);
        if (!rc) goto exit_screen;
        ry += 2;
        rc = 1;
        sbc(M[0x9611 + ry]);
        if (rc) goto exit_screen;
        ry += 2;
    }
    rx = 2;
    s9EF5();
    return;

exit_screen:
    push(rx);
    push(ry);
    push(M[0x28]);
    push(M[0x29]);
    s9498();
    M[0x2A] = M[Z_SCREEN];
    M[0x29] = pull();
    M[0x28] = pull();
    ry = pull();
    rx = pull();
    ry++;
    ZPX(0x28, rx) = M[0x9611 + ry];
    rx = 2;
    s9EF5();
    ry = (uint8_t)(ry >> 1);
    ry--;
    if (ry == 0) {
        ry++;
        ra = ry;
    } else if (NEG(ry)) {
        ra = ry;
    } else {
        ra = M[0x72];
        ry--;
        if (ry == 0) {
            ra ^= 0xFF;
            ry = ra;
            ry++;
            ra = ry;
        }
    }
    rc = 0;
    adc(M[Z_SCREEN]);
    M[Z_SCREEN] = ra;
    s8940();
    s8775();
}

/* $9619: standing on a floor row? -> $96 */
void s9619(void)
{
    rx = 2;
    s9EDB();
    ry = 0xFF;
    ra = M[0x29];
    if (ra == 0x46 || ra == 0x6E) {
        if (!NEG(M[0x83])) return;
        ry++;
    }
    M[0x96] = ry;
}

/* $9632: hero animation state from (in air, facing, moving) */
void s9632(void)
{
    M[0x24] = 0;
    M[0x24] = (uint8_t)(M[0x24] << 1 | (M[0x96] >> 7));
    M[0x24] = (uint8_t)(M[0x24] << 1 | (M[0x8B] >> 7));
    rc = M[0x0845] != 0 ? 1 : 0;   /* else carry from ROL $24 = 0 */
    M[0x24] = (uint8_t)(M[0x24] << 1 | rc);
    ra = M[0x24];
    if (M[0x0830]) ra &= 2;
    if (ra == M[0x97]) return;
    M[0x97] = ra;
    rx = (uint8_t)(ra << 1);
    M[0x22] = M[0x966A + rx];
    rx++;
    M[0x23] = M[0x966A + rx];
    ra = 2;
    s81E5();
}

/* $967A: copy sprite shadows $3C-$4C to VIC $D000-$D010 */
void s967A(void)
{
    for (int i = 0x10; i >= 0; i--) VIC_WR(0xD000 + i, M[0x3C + i]);
    rx = 0xFF;
}

/* $9685: (IRQ) attach propeller (0), legs (2) and laser (3) to the hero */
void s9685(void)
{
    rx = 0;
    s9EF9();
    M[0x29] = (uint8_t)(M[0x29] + 3);
    rx = 4;
    s9EF5();
    ra = 7;
    if (NEG(M[0x8B])) ra = M[0xA8];
    M[0x28] = (uint8_t)(M[0x28] + ra);
    rx = 6;
    s9EF5();
}

/* $96A9: test the hero's 4 corners against the map -> $83 (solid), $84 (deadly) */
void s96A9(void)
{
    ry = 0;
    do {
        M[0x26] = (uint8_t)(M[0x28] + M[0x96CA + ry]);
        ry++;
        ra = (uint8_t)(M[0x29] + M[0x96CA + ry]);
        M[0x27] = ry;
        s96D2();
        s970B();
        ry = (uint8_t)(M[0x27] + 1);
    } while (ry != 8);
}

/* $96D2: char at pixel (x/2 = $26, y = A) -> $26 */
void s96D2(void)
{
    ra = (uint8_t)((uint8_t)(ra - 0x2F) & 0xF8);
    M[0x22] = ra;
    M[0x2A] = ra;
    M[0x23] = 0;
    M[0x2B] = 0;
    rx = 0;
    ry = 2;
    s9F2E();
    ry = 8;
    s9F13();
    M[0x2A] = (uint8_t)((uint8_t)(M[0x26] - 0x0C) >> 2);
    s9F13();
    M[0x2A] = 0;
    M[0x2B] = 4;
    s9F13();
    ry = 0;
    M[0x26] = ldiy(0x22);
}

/* $970B: classify char $26: $0D-$37 solid -> $83, $2B-$37 deadly -> $84 */
void s970B(void)
{
    ry = 0;
    rx = 0;
    do {
        ra = M[0x26] & 0x3F;
        if (NEG(cmp(ra, M[0x9730 + ry]))) {
            ry++;
            ra = 0;
        } else {
            ry++;
            ra = NEG(cmp(ra, M[0x9730 + ry])) ? 0xFF : 0x00;
        }
        ra |= ZPX(0x83, rx);
        ZPX(0x83, rx) = ra;
        rx++;
        ry++;
    } while (NEG(cmp(ry, 4)));
}
