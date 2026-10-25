/* Collision events, deaths, level end, hero entry, game over. */
#include "hero.h"

/* $98ED: (main) react to sprite-sprite collisions ($D01E masked by $80) */
void s98ED(void)
{
    ra = VIC_RD(0xD01E) & M[0x80];
    M[0x2A] = ra;
    if (!(ra & 0x0E)) return;
    if (ra & 0x08) {                       /* laser (sprite 3) involved */
        M[0x81] = 0xFF;
        s9912();
        return;
    }
    if (!(M[0x2A] & 0x06)) return;         /* hero body/legs (1, 2) */
    M[0x81] = 0;
    s9912();
}

/* $9912: which object was hit */
void s9912(void)
{
    if (M[0x2A] & 1) M[0xB5] = 0xFF;
    ra = M[0x2A] & 0xF0;
    if (ra == 0x10) { rx = 4; s99A0(); return; }
    if (ra == 0x20) { rx = 5; s99A0(); return; }
    if (ra == 0x40) {                      /* sprite 6: lantern / raft / tentacle */
        M[0x80] &= (uint8_t)~0x40;
        ra = M[0x081B];
        if (ra == 0xCC) { M[0x7C] = 0xFF; return; }
        if (ra == 0xE9) {                  /* raft */
            M[0x0831] = M[0x0830] = 1;
            M[0x95] = 0x0A;
            M[0x3F] = M[0x41] = 0x95;
            M[0xB5] = 0;
            M[0x7C] = 0;
            return;
        }
        rx = 0x0C;
        s9A75();
        if (!NEG(ra) && ra != 0) {
            M[0x80] |= 0x40;
            return;
        }
        M[0x6D] = 0xFF;                    /* lantern hit: lights out */
        s8E9A();
        s8EE5();
        s8F37();
        return;
    }
    if (ra == 0x80) {                      /* sprite 7: the miner */
        rx = M[Z_LEVEL];
        if (M[0xA3D0 + rx] != M[Z_SCREEN]) return;
        if (NEG(M[0x81])) return;
        M[0x80] &= 0x7F;
        s9C0B();
    }
}

/* $99A0: enemy X (4/5) touched (A = collision bits) */
void s99A0(void)
{
    if (!NEG(M[0x81])) {                   /* touched by the hero: deadly */
        ra &= 0x30;
        M[0x7B] = ra;
        M[0x80] &= (uint8_t)~ra;
        return;
    }
    M[0x2C] = ra;                          /* hit by the laser */
    M[0x2B] = rx;
    M[0x80] &= (uint8_t)~ra;
    if (M[0x2C] & 0x20) {
        if (M[0x0817] == 0xE1) {
            rx = 0x0A;
            s9A75();
            if (NEG(ra)) { M[0x80] |= 0x20; return; }
        }
        if (!NEG(M[0x6D])) VIC_WR(0xD02C, 1);
        M[0x0839] = M[0x0838] = 0x1E;
    } else {
        if (M[0x0813] == 0xE1) {
            rx = 8;
            s9A75();
            if (NEG(ra)) { M[0x80] |= 0x10; return; }
        }
        if (!NEG(M[0x6D])) VIC_WR(0xD02B, 1);
        M[0x0835] = M[0x0834] = 0x1E;
    }
    setptr(0x22, 0xA373);
    ra = M[0x2B];
    s81E5();
    ra = 0x50;
    rx = 0;
    s86AB();
}

/* $9A22: (timer) remove sprite 4 */
void s9A22(void)
{
    M[0x0834] = M[0x0835] = 0;
    VIC_WR(0xD015, VIC_RD(0xD015) & 0xEF);
}

/* $9A33: (timer) remove sprite 5 */
void s9A33(void)
{
    M[0x0838] = M[0x0839] = 0;
    VIC_WR(0xD015, VIC_RD(0xD015) & 0xDF);
}

/* $9A44: tentacle retracts when the hero is close */
void s9A44(void)
{
    if (M[0x081B] != 0xCC) return;
    rx = 2;
    s9A6A();
    if (!NEG(cmp(ra, 9))) return;
    VIC_WR(0xD015, VIC_RD(0xD015) & 0xBF);
}

/* $9A5D: (timer) reached the miner? */
void s9A5D(void)
{
    rx = 0x0E;
    s9A6A();
    if (NEG(cmp(ra, 9))) s9C0B();
}

/* $9A6A: |hero X - sprite X| */
void s9A6A(void)
{
    s9A75();
    if (NEG(ra)) {
        ra ^= 0xFF;
        rc = 0;
        adc(1);
    }
}

/* $9A75: hero X/2 - sprite(X) X/2 */
void s9A75(void)
{
    s9EDB();
    M[0x2A] = ra;
    rx = 2;
    s9EDB();
    rc = 1;
    sbc(M[0x2A]);
}

/* $9A83: (main) deaths, lantern/raft events, game over */
void s9A83(void)
{
    if (!NEG(M[0x7D])) {
        if (NEG(M[0x84])) {                /* touched a deadly wall */
            if (M[0x74]) {
                M[0x74] = 0;
                ra = 0;
                s8BCD();
            }
        } else {
            if (M[0x7B]) {                 /* touched an enemy */
                s9B61();
                VIC_WR(0xD015, VIC_RD(0xD015) & (uint8_t)~M[0x7B]);
                goto after_death;
            }
            if (M[0x0830]) return;
            if (!NEG(M[0x7C])) {
                if (M[Z_SCREEN] < 0x0A) return;
                if (!NEG(M[0xB5])) return;
                if (NEG(cmp(M[0x3F], 0x99))) return;
                M[0x7C] = 0xFF;
                return;
            }
            s9B1D();                       /* sinking (water/tentacle) */
            rx = 4;
            do {
                M[0x3F]++;
                s9E9B();
            } while (M[0x3F] != 0xA8);
            s9A44();
            goto after_death;
        }
    }
    s9B61();
after_death:
    if (M[Z_LIVES2] == 0) {                /* game over -> attract */
        rx = 0x3C;
        s9EAD();
        M[Z_DEMO] = 0xFF;
        M[Z_LIVES2] = 6;
        s85F5();
        M[Z_JOY2] = M[Z_JOY1] = 0;
        VIC_WR(0xD015, 0);
        M[Z_LEVEL] = 0;
        s8515();
        return;
    }
    M[Z_LIVES2] -= 2;
    s85F5();
    s9B6A();
    s9BDC();
    ra = 0x1F;
    s9BEC();
    M[0x7E] = 0;
}

/* $9B1D: hero dies: freeze, death frames, sound, cancel dynamite */
void s9B1D(void)
{
    M[0x7E] = 0xFF;
    VIC_WR(0xD015, VIC_RD(0xD015) & 0xF7);
    M[0x5D] = 0;
    setptr(0x22, 0xA342); ra = 1; s81E5();
    setptr(0x22, 0xA330); ra = 2; s81E5();
    if (!NEG(M[Z_DEMO])) {
        SID_WR(0xD414, 0);
        SID_WR(0xD412, 0);
        SID_WR(0xD413, 0x4A);
        SID_WR(0xD412, 0x41);
    }
    s9498();
}

/* $9B61: die and wait 60 frames */
void s9B61(void)
{
    s9B1D();
    rx = 0x0C;
    s9EAD();
}

/* $9B6A: hero drops in from the top of the screen */
void s9B6A(void)
{
    ra = M[0x3F];
    if (!NEG(cmp(ra, 0x87))) ra = 0x88;
    else if (!NEG(cmp(ra, 0x5F))) ra = 0x60;
    else ra = 0x38;
    M[0x2A] = ra;
    M[0x3F] = 0x1E;
    rx = 2;
    s9EDB();
    s9685();
    s967A();
    s9619();
    if (NEG(M[0x8B])) {
        setptr(0x22, 0xA339); ra = 1; s81E5();
        setptr(0x22, 0xA31C);
    } else {
        setptr(0x22, 0xA301); ra = 1; s81E5();
        setptr(0x22, 0xA30A);
    }
    ra = 2;
    s81E5();
    do {
        s9EA4();
        M[Z_PAUSE] = 0;
        M[0x3F]++;
    } while (NEG(cmp(M[0x3F], M[0x2A])));
    M[0x95] = 0x0A;
}

/* $9BDC: clear collision latches and event flags */
void s9BDC(void)
{
    (void)VIC_RD(0xD01E);
    M[0x84] = M[0x7B] = M[0x7C] = M[0x7D] = M[0xB5] = 0;
    ra = 0;
}

/* $9BEC: reset movement and wait for joystick bits A */
void s9BEC(void)
{
    M[0x2A] = ra;
    for (rx = 3; !NEG(rx); rx--) {
        ZPX(0x8C, rx) = 0;
        M[0x0844 + rx] = 0;
    }
    M[0x89] = M[0x8A] = M[0x92] = M[0x93] = 0;
    hw.wait_reason = HW_WAIT_JOYSTICK;
    while (!((M[Z_JOY2] | M[Z_JOY1]) & M[0x2A])) hw_yield();
    ra = (uint8_t)((M[Z_JOY2] | M[Z_JOY1]) & M[0x2A]);
}

/* $9C0B: miner rescued: count power and dynamite into the score, next level */
void s9C0B(void)
{
    M[0x7E] = 0xFF;
    M[0x07FF]++;
    VIC_WR(0xD015, VIC_RD(0xD015) & 0xF7);
    M[0x0808] = 0;
    if (M[0x3F] == 0x6E) M[0x07FA] = NEG(M[0x8B]) ? 0xAC : 0xAB;
    M[0x0808] = 0;
    ra = 0;
    rx = 0x10;
    s86AB();                               /* 1000 points */
    s8401();
    M[0x7F] = 0xFF;
    do {                                   /* power -> score */
        s9EA4();
        ry = M[Z_LEVEL];
        ra = M[0xA44E + ry];
        rx = M[0xA462 + ry];
        s86AB();
        s865E();
        ra = M[Z_POWER];
        SID_WR(0xD401, ra);
        SID_WR(0xD402, ra);
    } while (ra);
    SID_WR(0xD404, ra);
    M[0x7F] = ra;
    for (;;) {                             /* dynamite -> score */
        if (NEG(M[Z_START])) { s91AD(); return; }
        rx = 2;
        s9EAD();
        M[Z_DYN2] -= 2;
        if (NEG(M[Z_DYN2])) break;
        s8631();
        s838F();
        ra = 0x50;
        rx = 0;
        s86AB();
    }
    rx = 5;
    s9EAD();
    if (NEG(M[Z_START])) { s91AD(); return; }
    s84EE();
}

/* $9F45: (timer) power drains */
void s9F45(void)
{
    M[Z_POWER] &= 0x7F;
    s865E();
}

/* $9F4F: (unreferenced) next screen or next level */
void s9F4F(void)
{
    M[Z_SCREEN]++;
    rx = M[Z_LEVEL];
    if (NEG(cmp(M[0xA3D0 + rx], M[Z_SCREEN]))) s84EE();
    else s8775();
}
