/* H.E.R.O. game logic: hand translation of the C64 6502 code.
 *
 * Conventions
 *  - One C function per original routine, named sXXXX after its C64 address,
 *    with a comment saying what it does. Keep the original behaviour, quirks
 *    included; see docs/c64_logic.md for the higher-level description.
 *  - Values passed in 6502 registers use the globals ra/rx/ry and the carry rc,
 *    exactly like the original, so implicit register passing stays correct.
 *  - State lives at the original addresses in M[] (see hw.h). Names below for
 *    the zero-page variables that are understood.
 *  - Main-thread code accounts CPU time with CYC(n) so long jobs (screen
 *    drawing) spread over frames like on the real machine.
 */
#ifndef HERO_H
#define HERO_H
#include <stdint.h>
#include "hw.h"

extern uint8_t ra, rx, ry;   /* 6502 A, X, Y */
extern int rc;               /* carry */

/* software stack for PHA/PLA */
void push(uint8_t v);
uint8_t pull(void);

#define CYC(n) hw_cycles(n)

static inline uint16_t ptr(uint8_t zp) { return (uint16_t)(M[zp] | M[(uint8_t)(zp + 1)] << 8); }
static inline void setptr(uint8_t zp, uint16_t v) { M[zp] = (uint8_t)v; M[(uint8_t)(zp + 1)] = (uint8_t)(v >> 8); }
/* LDA (zp),Y / STA (zp),Y */
static inline uint8_t ldiy(uint8_t zp) { return hw_rd((uint16_t)(ptr(zp) + ry)); }
static inline void stiy(uint8_t zp, uint8_t v) { hw_wr((uint16_t)(ptr(zp) + ry), v); }

/* binary ADC / SBC / CMP on ra (decimal mode handled where used) */
static inline void adc(uint8_t v) { unsigned r = ra + v + (unsigned)rc; rc = r > 0xFF; ra = (uint8_t)r; }
static inline void sbc(uint8_t v) { unsigned r = (unsigned)ra - v - (unsigned)(1 - rc); rc = r < 0x100; ra = (uint8_t)r; }
/* CMP: sets carry; returns the 8-bit difference (test & 0x80 for N, == 0 for Z) */
static inline uint8_t cmp(uint8_t r, uint8_t v) { rc = r >= v; return (uint8_t)(r - v); }
#define NEG(v) (((uint8_t)(v)) & 0x80)
/* zero-page indexed (zp,X / zp,Y): wraps within page 0 */
#define ZPX(base, idx) M[(uint8_t)((base) + (idx))]

/* ---- zero page (named where understood) ---------------------------------- */
#define Z_SPRX(i)    (0x3C + 2 * (i))  /* sprite shadow X (low 8 bits), i = 0..7 */
#define Z_SPRY(i)    (0x3D + 2 * (i))  /* sprite shadow Y */
#define Z_SPRXHI     0x4C              /* sprite shadow $D010 */
#define Z_WAIT       0x4D              /* frame countdowns $4D-$50, IRQ decrements */
#define Z_SCORE      0x53              /* 3 bytes BCD, $53 = most significant */
#define Z_SCREEN     0x57              /* screen index within the level */
#define Z_LEVEL      0x58              /* level 0..19 */
#define Z_POWER      0x7A
#define Z_FRAME      0xA4              /* decremented every frame (also used as RNG) */
#define Z_JOY2       0x90              /* debounced joystick, port 2 ($DC00), active high */
#define Z_JOY1       0x91              /* port 1 */
#define Z_LIVES2     0x98              /* lives * 2 */
#define Z_DYN2       0x99              /* dynamite * 2 */
#define Z_DEMO       0xB1              /* $FF = attract mode */
#define Z_PAUSE      0xB2
#define Z_STARTLVL   0xB4              /* F3 start-level selection 0..4 */
#define Z_BOTTOMPAGE 0xB6              /* bottom text row page; != 0 uses ROM font */
#define Z_START      0xB8              /* F1 pressed: start game */
#define Z_REDRAW     0xBD              /* $FF = screen (re)draw pending/in progress */

/* ---- entry points used by the platform ------------------------------------ */
void hero_main(void);        /* $8009 cold start + main loop (main-thread coroutine) */
void hero_irq(void);         /* raster IRQ dispatcher on the $0314 vector */
void hero_restore(void);     /* RESTORE key (NMI $90DB) */
void hero_check_restart(void); /* resume hook: perform a pending RESTORE restart */

/* ---- translated routines --------------------------------------------------- */
/* hero_main.c */
void s8169(void); void s818B(void); void s81E5(void); void s8206(void); void s8218(void);
void s8236(void); void s8290(void); void s82BC(void); void s82DB(void); void s82F7(void);
void s8339(void); void s836E(void);
void s84D7(void); void s84DF(void); void s84EE(void); void s8515(void); void s8775(void);
void s8CFD(void);
/* hero_sound.c */
void s8384(void); void s838F(void); void s83B5(void); void s83DB(void); void s8401(void);
void s8427(void); void s8445(void); void s8490(void); void s84A8(void);
/* hero_panel.c */
void s85A1(void); void s85D5(void); void s85F5(void); void s8631(void); void s863F(void);
void s865E(void); void s867B(void); void s86AB(void); void s86ED(void); void s8709(void);
void s8734(void); void s873E(void); void s91DD(void); void s9207(void);
/* hero_screen.c */
void s877A(void); void s886C(void); void s8906(void); void s891A(void); void s8940(void);
void s8987(void); void s89BD(void); void s8A14(void); void s8A6E(void); void s8A9A(void);
void s8AFB(void); void s8B45(void); void s8B59(void); void s8B86(void); void s8B91(void);
void s8BB4(void); void s8BCD(void); void s8BE3(void); void s8C3D(void); void s8C5F(void);
void s8CAA(void); void s8CF5(void); void s8D23(void); void s8DC8(void); void s8DD2(void);
void s8DDE(void); void s8DF6(void); void s8DFA(void); void s8DFC(void); void s8E1A(void);
void s8E45(void); void s8E90(void); void s8E9A(void); void s8EDD(void); void s8EE5(void);
void s8F0B(void); void s8F37(void);
void s9C93(void); void s9D40(void); void s9D76(void); void s9DA5(void); void s9DD1(void);
void s9E03(void); void s9E8B(void);
/* hero_play.c */
void s8F5C(void); void s8F75(void); void s8F8E(void); void s8FC0(void); void s8FE8(void);
void s9033(void); void s904B(void); void s90A9(void); void s90CE(void); void s9103(void);
void s9162(void); void s91AD(void); void s9231(void); void s92E3(void); void s9300(void);
void s937C(void); void s93A4(void); void s93EE(void); void s9408(void); void s9412(void);
void s942D(void); void s944B(void); void s946C(void); void s9498(void); void s94C4(void);
void s9524(void); void s9539(void); void s9563(void); void s9598(void); void s95AE(void);
void s9619(void); void s9632(void); void s967A(void); void s9685(void); void s96A9(void);
void s96D2(void); void s970B(void);
/* hero_events.c */
void s98ED(void); void s9912(void); void s99A0(void); void s9A22(void); void s9A33(void);
void s9A44(void); void s9A5D(void); void s9A6A(void); void s9A75(void); void s9A83(void);
void s9B1D(void); void s9B61(void); void s9B6A(void); void s9BDC(void); void s9BEC(void);
void s9C0B(void); void s9F45(void); void s9F4F(void);
/* hero_util.c */
void s9E9B(void); void s9EA4(void); void s9EAD(void); void s9EB4(void); void s9EC5(void);
void s9ED2(void); void s9EDB(void); void s9EF5(void); void s9EF9(void); void s9F13(void);
void s9F23(void); void s9F2E(void); void s9F39(void);
/* hero_irq.c */
void s9734(void); void s975F(void); void s97AC(void); void s97CB(void);

/* JMP (indirect) / callback dispatch by original address */
void call_addr(uint16_t addr);

#endif
