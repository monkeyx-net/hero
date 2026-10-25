/* Hardware model for the port.
 *
 * The game logic (hero_*.c) is a hand translation of the C64 6502 code. It
 * keeps its state at the original C64 addresses in M[] so behaviour, data
 * tables and lockstep tests line up 1:1 with the original. This file models the
 * parts of the C64 that logic touches: VIC-II registers (with per-line
 * application for raster splits, and sprite collisions), SID register writes,
 * CIA keyboard/joystick, colour RAM, and the split between the raster IRQ and
 * the "main thread" (run as a coroutine with a per-frame cycle budget).
 */
#ifndef HW_H
#define HW_H
#include <stdint.h>
#include "vic.h"

#define HW_MAIN_CYCLES_PER_FRAME 13600   /* measured on the reference model */

enum { HW_WAIT_BUDGET, HW_WAIT_COUNTDOWN, HW_WAIT_JOYSTICK };
enum { HW_UP = 1, HW_DOWN = 2, HW_LEFT = 4, HW_RIGHT = 8, HW_FIRE = 16 };

/* C64 keys the game reads (CIA1 matrix column, row). */
enum hw_key { KEY_F1, KEY_F3, KEY_F5, KEY_F7, KEY_RUNSTOP, KEY_COUNT };

typedef struct {
    uint8_t line;            /* raster line within the frame when written */
    uint8_t hi;              /* line bit 8 */
    uint8_t reg, val;
} hw_sidw_t;

typedef struct {
    uint8_t  mem[0x10000];   /* RAM; $8000-$BFFF holds the original game image */
    uint8_t  color[0x400];   /* colour RAM */
    uint8_t  vic[0x40];      /* VIC-II registers as currently set */
    uint8_t  sid[0x20];
    uint8_t  cia1_pra, cia1_ddra;
    uint8_t  joy[2];         /* port 1, port 2: HW_* bits held */
    uint8_t  keys;           /* bit per hw_key held */
    unsigned line;           /* current raster line (tick side) */
    uint64_t frame;

    /* frame output */
    uint8_t  fb[VIC_FB_W * VIC_FB_H];        /* C64-mode picture of the last frame */
    uint8_t  line_regs[VIC_LINES][0x30];     /* VIC registers used for each line */
    const uint8_t *chargen;                  /* lookalike char ROM (font.c) */

    /* SID writes of the last frame, for the synth */
    hw_sidw_t sidw[512];
    unsigned  nsidw;

    /* main-thread scheduling */
    int      irq_masked;     /* SEI in effect: raster IRQs are not taken */
    int      wait_reason;    /* why the main thread yields (HW_WAIT_*), for tests */
    void   (*resume_hook)(void);   /* called in the main thread after each yield */
    int32_t  budget;         /* cycles left for the main thread this frame */
    unsigned main_line;      /* raster line the main thread is "at" */

    /* test hooks */
    int (*raster_override)(void *user);      /* replay $D012 reads (lockstep) */
    void *hook_user;
} hw_t;

extern hw_t hw;
#define M (hw.mem)

/* Memory access with I/O dispatch (use for indirect/unknown addresses). */
uint8_t hw_rd(uint16_t a);
void    hw_wr(uint16_t a, uint8_t v);

/* VIC register access (reg = $D000 offset). */
uint8_t vic_rd(uint8_t reg);
void    vic_wr(uint8_t reg, uint8_t v);
#define VIC_RD(r) vic_rd((uint8_t)((r) - 0xD000))
#define VIC_WR(r, v) vic_wr((uint8_t)((r) - 0xD000), (uint8_t)(v))
void    sid_wr(uint8_t reg, uint8_t v);
#define SID_WR(r, v) sid_wr((uint8_t)((r) - 0xD400), (uint8_t)(v))
uint8_t cia1_rd(uint8_t reg);
void    cia1_wr(uint8_t reg, uint8_t v);

/* IRQ side: advance the beam to `line`, rendering lines on the way
 * (replaces the original's CMP $D012 / BNE busy waits). */
void hw_raster_wait(unsigned line);

/* Main-thread side. */
void hw_cycles(int n);       /* account CPU time; yields when the frame is used up */
void hw_yield(void);         /* give up the rest of this frame */

/* Lifecycle */
int  hw_init(const uint8_t image[0x10000]);
void hw_start_main(void (*entry)(void));
void hw_frame(void);         /* one PAL frame: IRQs, lines, main-thread slice */
void hw_shutdown(void);
void hw_set_irq(void (*fn)(void));  /* raster IRQ handler (dispatches on $0314) */
void hw_restart_main(void);         /* web: restart the main thread from its entry */

#endif
