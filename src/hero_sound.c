/* SID sound effects $8384-$84D6. Register values are written exactly as the
 * original does; sid.c turns them into audio. */
#include "hero.h"

/* $8384: silence all SID registers */
void s8384(void)
{
    for (int i = 0x18; i >= 0; i--) SID_WR(0xD400 + i, 0);
    ra = 0;
}

/* voice 1 one-shot from an 8-byte register table (the 8th byte is the next
 * routine's first opcode in the original - kept as is) */
static void voice1_fx(uint16_t table)
{
    if (NEG(M[Z_DEMO])) return;
    SID_WR(0xD406, 0);
    SID_WR(0xD404, 0x80);
    for (int i = 7; i >= 0; i--) SID_WR(0xD400 + i, M[table + i]);
    SID_WR(0xD404, 0x81);
}

void s838F(void) { voice1_fx(0x83AE); }   /* $838F: wall destroyed / dynamite count */
void s83B5(void) { voice1_fx(0x83D4); }   /* $83B5: laser starts burning a wall */
void s83DB(void) { voice1_fx(0x83FA); }   /* $83DB: dynamite lit */
void s8401(void) { voice1_fx(0x8420); }   /* $8401: miner rescued */

/* $8427: (IRQ) flying noise filter wobble */
void s8427(void)
{
    if (M[0x0807] != 0xA8 || NEG(M[Z_DEMO]) || NEG(M[0xB3])) return;
    ra = (uint8_t)((M[Z_FRAME] & 3) << 1);
    SID_WR(0xD411, ra);
    ra = (uint8_t)(ra << 3);
    SID_WR(0xD40F, ra);
}

/* $8445: (IRQ) laser sound while sprite 3 is on */
void s8445(void)
{
    if (NEG(M[Z_DEMO]) || NEG(M[0xB3])) return;
    if (M[0x0807] == 0xA8) return;
    if (!(VIC_RD(0xD015) & 0x08)) {
        SID_WR(0xD412, 0);
        return;
    }
    rx = M[Z_FRAME] & 7;
    SID_WR(0xD40F, M[0x8480 + rx]);
    SID_WR(0xD411, M[0x8488 + rx]);
    SID_WR(0xD414, 0x20);
    SID_WR(0xD413, 0x00);
    SID_WR(0xD412, 0x41);
}

/* $8490: volume up, voice 2 preset */
void s8490(void)
{
    SID_WR(0xD418, 0x0F);
    for (int i = 7; i >= 0; i--) SID_WR(0xD407 + i, M[0x84A1 + i]);
}

/* $84A8: (IRQ) propeller sound pulses */
void s84A8(void)
{
    if (NEG(M[Z_DEMO]) || NEG(M[0xB3]) || NEG(M[0x7E])) return;
    if (M[0x0830]) return;
    if (!NEG(M[0x96]) && NEG(cmp(2, M[0x0804]))) return;
    if (M[0x4F]) return;
    M[0x4F] = M[0xAB];
    SID_WR(0xD40B, 0x80);
    SID_WR(0xD40B, 0x81);
}
