/* Raster interrupts $9734/$975F/$97CB and their setup $97AC.
 * The original chains three raster IRQs per frame through the $0314 vector. */
#include "hero.h"

void hero_irq(void)
{
    /* The real IRQ saves A/X/Y; the main thread may be parked mid-routine. */
    uint8_t sa = ra, sx = rx, sy = ry;
    int sc = rc;
    switch (M[0x314] | M[0x315] << 8) {
    case 0x9734: s9734(); break;
    case 0x975F: s975F(); break;
    case 0x97CB: s97CB(); break;
    default: break;   /* KERNAL default before the game installs its own */
    }
    ra = sa; rx = sx; ry = sy; rc = sc;
}

static void set_vector(uint16_t v)
{
    M[0x314] = (uint8_t)v;
    M[0x315] = (uint8_t)(v >> 8);
}

/* $9734: raster $AF - colour of the band row ($79), next IRQ at $B6 */
void s9734(void)
{
    VIC_WR(0xD019, 1);
    VIC_WR(0xD021, M[0x79]);
    VIC_WR(0xD012, 0xB6);
    VIC_WR(0xD011, VIC_RD(0xD011) & 0x7F);
    set_vector(0x975F);
}

/* $975F: raster $B6 - status panel: multicolour text, copy sprite shadows */
void s975F(void)
{
    VIC_WR(0xD019, 1);
    VIC_WR(0xD021, 0x0B);
    VIC_WR(0xD027, 0x0B);
    VIC_WR(0xD022, 0x02);
    VIC_WR(0xD023, 0x07);
    VIC_WR(0xD011, VIC_RD(0xD011) & 0xBF);
    VIC_WR(0xD016, VIC_RD(0xD016) | 0x10);
    s967A();
    s91DD();
    VIC_WR(0xD012, 0xED);
    VIC_WR(0xD011, VIC_RD(0xD011) & 0x7F);
    set_vector(0x97CB);
}

/* $97AC: install the IRQ chain */
void s97AC(void)
{
    set_vector(0x9734);
    VIC_WR(0xD012, 0xAF);
    VIC_WR(0xD011, VIC_RD(0xD011) & 0x7F);
    VIC_WR(0xD01A, 0x81);
}

/* $97CB: raster $ED - bottom row (logo rainbow / ROM-font text), restore the
 * ECM playfield, then the whole per-frame game update. */
void s97CB(void)
{
    VIC_WR(0xD019, 1);
    VIC_WR(0xD021, 0);
    VIC_WR(0xD027, M[0x79]);
    hw_raster_wait(0xEF);
    ra = (uint8_t)((VIC_RD(0xD018) & 0xF1) | (M[Z_BOTTOMPAGE] ? 0x04 : 0x08));
    VIC_WR(0xD018, ra);
    VIC_WR(0xD023, 2);
    VIC_WR(0xD022, 2);
    hw_raster_wait(0xF2); VIC_WR(0xD022, 8);
    hw_raster_wait(0xF3); VIC_WR(0xD023, 7);
    hw_raster_wait(0xF4); VIC_WR(0xD022, 7);
    hw_raster_wait(0xF5); VIC_WR(0xD023, 5);
    hw_raster_wait(0xF6); VIC_WR(0xD022, 5);
    hw_raster_wait(0xF7); VIC_WR(0xD023, 6);
    hw_raster_wait(0xFC);
    for (int i = 2; i >= 0; i--) VIC_WR(0xD021 + i, M[0x5D + i]);
    VIC_WR(0xD016, VIC_RD(0xD016) & 0xEF);
    VIC_WR(0xD011, VIC_RD(0xD011) | 0x40);
    VIC_WR(0xD018, (VIC_RD(0xD018) & 0xF1) | 0x08);

    uint8_t save[0x14];
    for (int i = 0; i < 0x14; i++) save[i] = M[0x22 + i];

    if (!NEG(M[Z_REDRAW])) {
        if (NEG(M[Z_PAUSE])) goto tail;
        s8236();
        s8BB4();
        s836E();
        s8C5F();
        if (!NEG(M[0x7E])) {
            s94C4();
            s9524();
        }
    }
    s9231();
    rx = 2;
    s9EDB();
    s9685();
    if (!NEG(M[Z_REDRAW])) {
        if (NEG(M[Z_DEMO])) s9103();
        else s8F5C();
    }
    s84A8();
    s8445();
    s8427();
tail:
    s9EB4();
    s904B();
    s9162();
    VIC_WR(0xD012, 0xAF);
    VIC_WR(0xD011, VIC_RD(0xD011) & 0x7F);
    for (int i = 0; i < 0x14; i++) M[0x22 + i] = save[i];
    set_vector(0x9734);
}
