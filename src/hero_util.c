/* Small helpers $9E9B-$9F61: frame waits, timers, sprite shadow access, 16-bit math. */
#include "hero.h"

uint8_t ra, rx, ry;
int rc;

static uint8_t stack[256];
static uint8_t sp = 0xFF;
void push(uint8_t v) { stack[sp--] = v; }
uint8_t pull(void) { return stack[++sp]; }

/* $9E9B: wait 5 frames */
void s9E9B(void)
{
    M[Z_WAIT] = 5;
    hw.wait_reason = HW_WAIT_COUNTDOWN;
    while (M[Z_WAIT]) hw_yield();
    ra = 0;
}

/* $9EA4: wait 1 frame */
void s9EA4(void)
{
    M[Z_WAIT] = 1;
    hw.wait_reason = HW_WAIT_COUNTDOWN;
    while (M[Z_WAIT]) hw_yield();
    ra = 0;
}

/* $9EAD: wait X * 5 frames */
void s9EAD(void)
{
    do { s9E9B(); rx--; } while (rx);
}

/* $9EB4: per-frame: decrement frame counter $A4 and the countdowns $4D-$50 */
void s9EB4(void)
{
    M[Z_FRAME]--;
    for (rx = 3; !NEG(rx); rx--) {
        ra = M[(uint8_t)(0x4D + rx)];
        if (ra) M[(uint8_t)(0x4D + rx)] = --ra;
    }
}

/* $9EC5: copy word at ($22),Y to $22+X/$23+X, Y += 2; A = lo | hi */
void s9EC5(void)
{
    ra = ldiy(0x22);
    M[(uint8_t)(0x22 + rx)] = ra;
    ry++;
    ra = ldiy(0x22);
    M[(uint8_t)(0x23 + rx)] = ra;
    ra |= M[(uint8_t)(0x22 + rx)];
    ry++;
}

/* $9ED2: clear Y bytes at ($24) */
void s9ED2(void)
{
    ra = 0;
    ry--;
    do { stiy(0x24, ra); ry--; } while (!NEG(ry));
}

/* $9EDB: sprite X/2 (bit 7 = X MSB) -> $28, sprite Y -> $29, for shadow index X (= 2 * sprite) */
void s9EDB(void)
{
    M[0x29] = M[(uint8_t)(0x3D + rx)];
    ra = M[(uint8_t)(0x3C + rx)];
    rc = ra & 1;
    ra >>= 1;
    M[0x28] = ra;
    rc = rx & 1;
    rx >>= 1;
    ra = M[0x81FE + rx] & M[Z_SPRXHI];
    if (ra) ra = 0x80;
    ra |= M[0x28];
    M[0x28] = ra;
}

/* $9EF5: store $29 as sprite Y, then $9EF9 */
void s9EF5(void)
{
    M[(uint8_t)(0x3D + rx)] = M[0x29];
    s9EF9();
}

/* $9EF9: store $28 (X/2 + MSB flag) as sprite X for shadow index X */
void s9EF9(void)
{
    ra = M[0x28];
    rc = ra >> 7;
    ra = (uint8_t)(ra << 1);
    M[(uint8_t)(0x3C + rx)] = ra;
    rc = rx & 1;
    rx >>= 1;
    ra = M[0x81FE + rx];
    if (NEG(M[0x28])) {
        ra |= M[Z_SPRXHI];
    } else {
        ra ^= 0xFF;
        ra &= M[Z_SPRXHI];
    }
    M[Z_SPRXHI] = ra;
}

/* $9F13: 16-bit add: word $22+X += word $22+Y */
void s9F13(void)
{
    rc = 0;
    ra = M[(uint8_t)(0x22 + rx)];
    adc(M[0x22 + ry]);
    M[(uint8_t)(0x22 + rx)] = ra;
    ra = M[(uint8_t)(0x23 + rx)];
    adc(M[0x23 + ry]);
    M[(uint8_t)(0x23 + rx)] = ra;
}

/* $9F23: sign-extend byte $22+X into $23+X */
void s9F23(void)
{
    ra = 0;
    ry = M[(uint8_t)(0x22 + rx)];
    if (NEG(ry)) ra = 0xFF;
    M[(uint8_t)(0x23 + rx)] = ra;
}

/* $9F2E: word $22+X <<= Y */
void s9F2E(void)
{
    for (;;) {
        ry--;
        if (NEG(ry)) return;
        uint8_t lo = M[(uint8_t)(0x22 + rx)], hi = M[(uint8_t)(0x23 + rx)];
        rc = lo >> 7;
        lo = (uint8_t)(lo << 1);
        int c2 = hi >> 7;
        hi = (uint8_t)(hi << 1 | rc);
        rc = c2;
        M[(uint8_t)(0x22 + rx)] = lo;
        M[(uint8_t)(0x23 + rx)] = hi;
    }
}

/* $9F39: shift the 8 bits of A (LSB first) into $2A; A = $2A (bit reverse) */
void s9F39(void)
{
    for (rx = 8; rx; rx--) {
        rc = ra & 1;
        ra >>= 1;
        int c2 = M[0x2A] >> 7;
        M[0x2A] = (uint8_t)(M[0x2A] << 1 | rc);
        rc = c2;
    }
    ra = M[0x2A];
}
