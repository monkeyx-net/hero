/* Cold start, main loop, sprite setup/animation, timers, level start. */
#include "hero.h"
#include <setjmp.h>
#include <stdio.h>

#ifndef __EMSCRIPTEN__
static jmp_buf restart;
#endif
static volatile int restart_req;

/* $8009 cold start + $8154 main loop. Runs as the main-thread coroutine. */
void hero_main(void)
{
#ifndef __EMSCRIPTEN__
    setjmp(restart);
#endif
    restart_req = 0;
    hw.irq_masked = 1;
    cia1_wr(0x02, 0);
    for (rx = 0x9C; rx != 0xFF; rx--) M[(uint8_t)(0x22 + rx)] = 0;
    for (int i = 0x0400; i < 0x0900; i++) M[i] = 0;
    CYC(6000);
    s8169();
    VIC_WR(0xD015, 0xFF);
    VIC_WR(0xD015, 0x00);
    setptr(0x22, 0x9F82); setptr(0x24, 0x2800); s8290();
    setptr(0x22, 0xA054); setptr(0x24, 0x3800); s8290();
    VIC_WR(0xD025, 0x02);
    VIC_WR(0xD026, 0x0F);
    for (int i = 0x40; i >= 0; i--) M[0x3CC0 + i] = 0xFF;
    M[0x3A88] = 0x80;
    M[0x3AC8] = 0x20;
    M[0x3B08] = 0x20;
    M[0x3A42] = 0xA0;
    M[0x3A45] = 0xA0;
    setptr(0x22, 0x8D15); setptr(0x24, 0x3940); s8CFD();
    setptr(0x22, 0x8D1C); setptr(0x24, 0x3900); s8CFD();
    setptr(0x24, 0x3980); s8CFD();
    for (int i = 0; i < 0x500; i++) M[0x2000 + i] = M[0xA4A0 + i];   /* charset */
    CYC(16000);
    /* $DD00 |= 3: VIC bank 0 (fixed in hw) */
    VIC_WR(0xD016, 0x08);
    VIC_WR(0xD018, 0x18);
    VIC_WR(0xD011, 0x58);
    for (int i = 0x1F; i >= 0; i--) M[0x0820 + i] = M[0x9F62 + i];
    M[0x94] = M[0x95] = 3;
    setptr(0x85, 0x8FE8);    /* joystick direction handler */
    setptr(0x87, 0x9033);    /* fire button handler */
    M[0x4E] = 0xB4;
    s8384();
    s85A1();
    M[0xB0] = M[Z_LIVES2] = 6;
    s85F5();
    M[Z_DEMO] = 0xFF;
    s84D7();
    s97AC();
    hw.irq_masked = 0;
    s84DF();
    for (;;) {                                   /* $8154 */
        s877A();
        s91AD();
        s8339();
        s8F75();
        s98ED();
        s9A83();
        CYC(250);
    }
}

/* Called by hw_yield's caller side: honour a pending RESTORE restart. */
void hero_check_restart(void)
{
#ifndef __EMSCRIPTEN__
    if (restart_req) longjmp(restart, 1);
#endif
}

/* $90DB: NMI (RESTORE). With RUN/STOP held: silence and cold start. */
void hero_restore(void)
{
    cia1_wr(0x02, 0xFF);
    cia1_wr(0x00, 0x7F);
    uint8_t k = cia1_rd(0x01);
    if (!(k & 0x80)) {
        VIC_WR(0xD015, 0);
        SID_WR(0xD404, 0);
        SID_WR(0xD40B, 0);
        SID_WR(0xD412, 0);
        restart_req = 1;
#ifdef __EMSCRIPTEN__
        hw_restart_main();          /* the fiber starts over from $8009 */
#endif
    }
    cia1_wr(0x02, 0x00);
}

/* $8169: clear VIC sprite/colour registers */
void s8169(void)
{
    for (int i = 0x11; i >= 0; i--) VIC_WR(0xD000 + i, 0);
    VIC_WR(0xD015, 0); VIC_WR(0xD01B, 0); VIC_WR(0xD01C, 0); VIC_WR(0xD01D, 0); VIC_WR(0xD017, 0);
    for (int i = 0x0E; i >= 0; i--) VIC_WR(0xD020 + i, 0);
    ra = 0;
    rx = 0xFF;
}

/* $818B: set up sprite A from the 9-byte record at ($22):
 *   [X/2|msb, Y, $0843 value, colour, flags(enable,yexp,xexp,prio,mc << 1)]
 *   followed by the 4-byte animation record handled by $81E5. $22 += 5. */
void s818B(void)
{
    uint8_t n = ra;
    push(n);
    ra = (uint8_t)(n << 1);
    push(ra);
    rx = ra;
    ry = 0;
    M[0x28] = ldiy(0x22);
    ry++;
    M[0x29] = ldiy(0x22);
    s9EF5();
    ry++;
    ra = pull();
    ra = (uint8_t)(ra << 1);
    rx = ra;
    ra = ldiy(0x22);
    M[0x0843 + rx] = ra;
    ry++;
    ra = pull();
    push(ra);
    rx = ra;
    ra = ldiy(0x22);
    VIC_WR(0xD027 + rx, ra);
    ry++;
    ra = M[0x81FE + rx];
    M[0x2A] = ra;
    M[0x2B] = ra ^ 0xFF;
    ra = ldiy(0x22);
    M[0x2C] = (uint8_t)(ra << 1);
    ry = 0x15; s8206();
    ry = 0x17; s8206();
    ry = 0x1D; s8206();
    ry = 0x1B; s8206();
    ry = 0x1C; s8206();
    rx = 0;
    M[0x25] = 0;
    ry = 2;
    M[0x24] = 5;
    s9F13();
    ra = pull();
    s81E5();
}

/* $81E5: copy the 4-byte animation record at ($22) to $0800 + 4*A and set
 * sprite A's pointer to its base frame. */
void s81E5(void)
{
    uint8_t n = ra;
    ry = 0;
    push(n);
    rx = (uint8_t)(n << 2);
    do {
        ra = ldiy(0x22);
        M[0x0800 + rx] = ra;
        rx++;
        ry++;
    } while (ry != 4);
    ry = ra;
    rx = pull();
    ra = ry;
    M[0x07F8 + rx] = ra;
}

/* $8206: shift next flag bit out of $2C into VIC register $D000+Y (mask $2A/$2B) */
void s8206(void)
{
    rc = M[0x2C] >> 7;
    M[0x2C] = (uint8_t)(M[0x2C] << 1);
    ra = VIC_RD(0xD000 + ry);
    if (rc) ra |= M[0x2A];
    else ra &= M[0x2B];
    VIC_WR(0xD000 + ry, ra);
}

/* $8218: set up X sprites starting at A from consecutive 9-byte records at ($22) */
void s8218(void)
{
    M[0x2D] = ra;
    do {
        push(rx);
        ra = M[0x2D];
        s818B();
        M[0x2D]++;
        ry = 2;
        rx = 0;
        M[0x25] = 0;
        M[0x24] = 4;
        s9F13();
        ra = pull();
        rx = ra;
        rx--;
    } while (rx);
}

/* $8236: (IRQ) step sprite animations: $0800+4n = [speed, count, frames, base] */
void s8236(void)
{
    ra = 0x1C;
    do {
        push(ra);
        M[0x2A] = (uint8_t)(ra >> 2);
        rx = pull();
        ra = M[0x0800 + rx];
        if (ra) {
            ra = (uint8_t)(M[0x0801 + rx] - 1);
            M[0x0801 + rx] = ra;
            if (!ra) {
                M[0x0801 + rx] = M[0x0800 + rx];
                M[0x0802 + rx]++;
                M[0x2B] = M[0x0802 + rx] & 0x0F;
                ra = (uint8_t)(M[0x0802 + rx] >> 4);
                if (NEG(cmp(ra, M[0x2B]))) M[0x2B] = 0;
                M[0x0802 + rx] = (uint8_t)((M[0x0802 + rx] & 0xF0) | M[0x2B]);
                ra = (uint8_t)(M[0x0803 + rx] + M[0x2B]);
                M[0x07F8 + M[0x2A]] = ra;
            }
        }
        ra = (uint8_t)(rx - 4);
    } while (!NEG(ra));
}

/* $8290: build multicolour sprites from the shape list at ($22) into ($24)... */
void s8290(void)
{
    ry = 0;
    do {
        do {
            rx = 4; s9EC5();          /* ($26) = pixel data */
            rx = 6; s9EC5();          /* ($28) = segment headers */
            push(ry);
            s82BC();
            setptr(0x24, (uint16_t)(ptr(0x24) + 0x40));
            ry = pull();
            ra = ldiy(0x22);
        } while (ra);
        ry++;
        ra = ldiy(0x22);
        if (!ra) return;
        ry--;
    } while (1);
}

/* $82BC: build one 64-byte sprite */
void s82BC(void)
{
    ry = 0x40;
    s9ED2();
    ry = 0;
    M[0x2D] = 0;
    M[0x2F] = 0;
    M[0x34] = ldiy(0x26);
    ry++;
    M[0x2E] = ry;
    do {
        s82DB();
        s82F7();
        CYC(400);
    } while (!NEG(cmp(M[0x34], M[0x2E])));
}

/* $82DB: segment header: low nibble = rows ($2A), bits 7-6 = colour pair ($2B),
 * bit 5 = mirror ($2C) */
void s82DB(void)
{
    ry = M[0x2D];
    ra = ldiy(0x28);
    M[0x2D]++;
    rx = ra;
    M[0x2A] = ra & 0x0F;
    M[0x2B] = (uint8_t)(rx >> 6);
    rc = (rx >> 5) & 1;
    M[0x2C] = (uint8_t)rc;
}

/* $82F7: expand $2A rows of 1-bit pixels into 2-bit pixels of colour $2B */
void s82F7(void)
{
    do {
        ry = M[0x2E];
        M[0x30] = ldiy(0x26);
        M[0x2E]++;
        M[0x32] = 2;
        do {
            M[0x33] = 4;
            M[0x31] = 0;
            do {
                ra = 0;
                uint8_t v = M[0x31];
                v = (uint8_t)(v << 1);                /* ASL $31 */
                rc = v >> 7;                          /* ASL $31: carry = bit 7 */
                v = (uint8_t)(v << 1);
                M[0x31] = v;
                int c;
                if (M[0x2C] == 0) {                   /* ROL $30 */
                    c = M[0x30] >> 7;
                    M[0x30] = (uint8_t)(M[0x30] << 1 | rc);
                } else {                              /* ROR $30 (mirrored) */
                    c = M[0x30] & 1;
                    M[0x30] = (uint8_t)(M[0x30] >> 1 | rc << 7);
                }
                rc = c;
                if (rc) ra = M[0x2B];
                ra |= M[0x31];
                M[0x31] = ra;
            } while (--M[0x33]);
            ry = M[0x2F];
            stiy(0x24, ra);
            M[0x2F]++;
        } while (--M[0x32]);
        M[0x2F]++;
    } while (--M[0x2A]);
}

/* $8339: (main) run expired timers: $0820+4n = [reload, count, callback lo/hi] */
void s8339(void)
{
    ry = 0x1C;
    do {
        if (M[0x0820 + ry] && M[0x0821 + ry] == 0) {
            M[0x0821 + ry] = M[0x0820 + ry];
            M[0x23] = M[0x0823 + ry];
            M[0x22] = M[0x0822 + ry];
            M[0x52] = ry;
            call_addr(ptr(0x22));
            ry = M[0x52];
        }
        ra = (uint8_t)(ry - 4);
        ry = ra;
        CYC(30);
    } while (!NEG(ra));
}

/* $836E: (IRQ) count down timers */
void s836E(void)
{
    for (ry = 1; NEG((uint8_t)(ry - 0x1F)); ry += 4)
        if (M[0x0820 + ry]) M[0x0820 + ry]--;
}

/* $84D7: wait for raster line >= $FB */
void s84D7(void)
{
    while (VIC_RD(0xD012) < 0xFB) CYC(10);
}

/* $84DF: game/attract start: level 0 */
void s84DF(void)
{
    VIC_WR(0xD024, 0);
    M[0x7E] = 0xFF;
    M[Z_LEVEL] = 0;
    s8515();
}

/* $84EE: next level (levels 13-20 repeat after 20; PRO mode picks at random) */
void s84EE(void)
{
    if (NEG(M[Z_DEMO]) || !NEG(M[0xA7])) {
        M[Z_LEVEL]++;
        if (!NEG(cmp(0x13, M[Z_LEVEL]))) { s8515(); return; }
        M[Z_LEVEL] = 0x0C;
        M[0xA6] = 0xFF;
        s8515();
        return;
    }
    ra = M[Z_FRAME] & 0x1F;
    if (!NEG(cmp(ra, 0x14))) ra = (uint8_t)(ra - 0x0C);
    M[Z_LEVEL] = ra;
    s8515();
}

/* $8515: start the level in $58 */
void s8515(void)
{
    hw.irq_masked = 1;
    M[0x7E] = 0xFF;
    s8490();
    ry = (uint8_t)((M[Z_LEVEL] << 2) & 0x0F);
    M[0x5B] = M[0x858D + ry]; ry++;
    M[0x5C] = M[0x858D + ry]; ry++;
    M[0x59] = M[0x858D + ry]; ry++;
    rx = M[0x858D + ry];
    M[0x5A] = rx;
    s8B86();
    M[Z_SCREEN] = 0;
    M[0x9C] = 0;
    M[0x9D] = 0;
    M[0x9B] = 0xFF;
    M[Z_REDRAW] = 0xFF;
    s8775();
    setptr(0x22, 0xA2F3);
    ra = 0;
    rx = 4;
    s8218();
    M[Z_POWER] = 0x80;
    s867B();
    M[Z_DYN2] = 0x0C;
    s863F();
    s8734();
    M[0x8B] = 1;
    M[0x3F] = 0x60;
    hw.irq_masked = 0;
    s877A();
    s9B6A();
    s9BDC();
    ra = 0x0F;
    s9BEC();
    M[0x7E] = 0;
    M[0xB3] = 0;
}

/* $8775: request screen redraw */
void s8775(void)
{
    M[Z_REDRAW] = 0xFF;
}

/* $8CFD: copy 7 bytes from ($22)+0..6 into every 3rd byte at ($24)+2.. */
void s8CFD(void)
{
    M[0x2A] = 2;
    for (rx = 6; !NEG(rx); rx--) {
        ry = rx;
        ra = ldiy(0x22);
        ry = M[0x2A];
        stiy(0x24, ra);
        M[0x2A] = (uint8_t)(ry + 3);
    }
}

/* JMP (indirect) targets */
void call_addr(uint16_t addr)
{
    switch (addr) {
    case 0x9F44: break;               /* RTS */
    case 0x9F45: s9F45(); break;
    case 0x8B45: s8B45(); break;
    case 0x8A6E: s8A6E(); break;
    case 0x8A9A: s8A9A(); break;
    case 0x9A22: s9A22(); break;
    case 0x9A33: s9A33(); break;
    case 0x9A5D: s9A5D(); break;
    case 0x937C: s937C(); break;
    case 0x93A4: s93A4(); break;
    case 0x93EE: s93EE(); break;
    case 0x8FE8: s8FE8(); break;
    case 0x9033: s9033(); break;
    default:
        fprintf(stderr, "call_addr: unknown target $%04X\n", addr);
        break;
    }
}
