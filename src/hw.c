#include "hw.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>

hw_t hw;

static void (*irq_handler)(void);
void hw_set_irq(void (*fn)(void)) { irq_handler = fn; }

static int in_main;                 /* 1 while the main-thread coroutine runs */
static void (*main_entry)(void);
#ifdef __EMSCRIPTEN__
/* Web: the main thread is an Emscripten fiber (Asyncify), no pthreads needed. */
#include <emscripten/fiber.h>
static emscripten_fiber_t fib_tick, fib_main;
static char main_cstack[512 * 1024], main_astack[256 * 1024], tick_astack[256 * 1024];
static int fibers_ready;
#else
/* Native: an SDL thread with a strict semaphore handoff (only one side runs). */
static SDL_sem *sem_main, *sem_tick;
static SDL_Thread *main_thread;
#endif

/* ---- VIC ----------------------------------------------------------------- */

static unsigned cur_line(void)
{
    if (!in_main) return hw.line;
    int used = HW_MAIN_CYCLES_PER_FRAME - hw.budget;
    if (used < 0) used = 0;
    return (0xFCu + (unsigned)used / 63u) % VIC_LINES;
}

uint8_t vic_rd(uint8_t reg)
{
    reg &= 0x3F;
    uint8_t v;
    switch (reg) {
    case 0x11: return (uint8_t)((hw.vic[0x11] & 0x7F) | ((cur_line() & 0x100) >> 1));
    case 0x12:
        if (in_main && hw.raster_override) return (uint8_t)hw.raster_override(hw.hook_user);
        return (uint8_t)cur_line();
    case 0x19: return (uint8_t)(hw.vic[0x19] | 0x70);
    case 0x1E: case 0x1F: v = hw.vic[reg]; hw.vic[reg] = 0; return v;
    default: return reg < 0x2F ? hw.vic[reg] : 0xFF;
    }
}

void vic_wr(uint8_t reg, uint8_t v)
{
    reg &= 0x3F;
    if (reg == 0x19) { hw.vic[0x19] &= (uint8_t)~(v & 0x0F); return; }
    if (reg == 0x1E || reg == 0x1F || reg >= 0x2F) return;
    hw.vic[reg] = v;
}

static void render_line(unsigned line)
{
    vic_mem_t mem = { hw.mem, hw.color, hw.chargen, 0 };
    uint8_t *out = NULL;
    if (line >= VIC_FIRST_LINE && line < VIC_FIRST_LINE + VIC_FB_H)
        out = hw.fb + (line - VIC_FIRST_LINE) * VIC_FB_W;
    memcpy(hw.line_regs[line], hw.vic, sizeof hw.line_regs[line]);
    vic_line(hw.vic, &mem, line, out, &hw.vic[0x1E], &hw.vic[0x1F]);
}

void hw_raster_wait(unsigned line)
{
    while (hw.line < line && hw.line < VIC_LINES) render_line(hw.line++);
}

/* ---- SID / CIA ------------------------------------------------------------ */

void sid_wr(uint8_t reg, uint8_t v)
{
    reg &= 0x1F;
    hw.sid[reg] = v;
    if (hw.nsidw < sizeof hw.sidw / sizeof hw.sidw[0]) {
        unsigned l = cur_line();
        hw.sidw[hw.nsidw++] = (hw_sidw_t){ (uint8_t)l, (uint8_t)(l >> 8), reg, v };
    }
}

static const struct { uint8_t col, row; } keypos[KEY_COUNT] = {
    [KEY_F1] = { 0, 4 }, [KEY_F3] = { 0, 5 }, [KEY_F5] = { 0, 6 }, [KEY_F7] = { 0, 3 },
    [KEY_RUNSTOP] = { 7, 7 },
};

uint8_t cia1_rd(uint8_t reg)
{
    uint8_t pra = (uint8_t)(hw.cia1_pra | ~hw.cia1_ddra);
    switch (reg & 0x0F) {
    case 0x00: return (uint8_t)(pra & ~hw.joy[1]);
    case 0x01: {
        uint8_t rows = 0xFF;
        for (int k = 0; k < KEY_COUNT; k++)
            if ((hw.keys >> k & 1) && !(pra >> keypos[k].col & 1)) rows &= (uint8_t)~(1 << keypos[k].row);
        return (uint8_t)(rows & ~hw.joy[0]);
    }
    case 0x02: return hw.cia1_ddra;
    default: return 0;
    }
}

void cia1_wr(uint8_t reg, uint8_t v)
{
    if ((reg & 0x0F) == 0) hw.cia1_pra = v;
    else if ((reg & 0x0F) == 2) hw.cia1_ddra = v;
}

/* ---- generic memory access ------------------------------------------------ */

uint8_t hw_rd(uint16_t a)
{
    if (a < 0xD000 || a >= 0xE000) return hw.mem[a];
    if (a < 0xD400) return vic_rd((uint8_t)a);
    if (a < 0xD800) return 0;
    if (a < 0xDC00) return (uint8_t)(hw.color[a & 0x3FF] | 0xF0);
    if (a < 0xDD00) return cia1_rd((uint8_t)a);
    if (a == 0xDD00) return 0xC3;            /* VIC bank 0 */
    return 0xFF;
}

void hw_wr(uint16_t a, uint8_t v)
{
    if (a < 0xD000 || a >= 0xE000) { hw.mem[a] = v; return; }
    if (a < 0xD400) vic_wr((uint8_t)a, v);
    else if (a < 0xD800) sid_wr((uint8_t)a, v);
    else if (a < 0xDC00) hw.color[a & 0x3FF] = v & 0x0F;
    else if (a < 0xDD00) cia1_wr((uint8_t)a, v);
}

/* ---- main-thread coroutine ------------------------------------------------ */

void (*hw_test_yield)(void);       /* tests: called instead of yielding */

#ifdef __EMSCRIPTEN__
static void main_fiber_fn(void *unused)
{
    (void)unused;
    main_entry();
    for (;;) hw_yield();      /* the original main loop never returns */
}

void hw_yield(void)
{
    if (hw_test_yield) { hw_test_yield(); return; }
    emscripten_fiber_swap(&fib_main, &fib_tick);
    if (hw.resume_hook) hw.resume_hook();
}

/* Throw the main thread away and start it again from its entry (RESTORE). */
void hw_restart_main(void)
{
    emscripten_fiber_init(&fib_main, main_fiber_fn, NULL, main_cstack, sizeof main_cstack,
                          main_astack, sizeof main_astack);
}
#else
static int main_thread_fn(void *unused)
{
    (void)unused;
    SDL_SemWait(sem_main);
    main_entry();
    for (;;) hw_yield();      /* the original main loop never returns */
    return 0;
}

void hw_yield(void)
{
    if (hw_test_yield) { hw_test_yield(); return; }
    SDL_SemPost(sem_tick);
    SDL_SemWait(sem_main);
    if (hw.resume_hook) hw.resume_hook();
}

void hw_restart_main(void) { }     /* native: handled by the resume hook (longjmp) */
#endif

void hw_cycles(int n)
{
    if (!in_main) return;          /* routines shared with the IRQ side */
    hw.budget -= n;
    if (hw.budget <= 0) {
        hw.wait_reason = HW_WAIT_BUDGET;
        hw_yield();
    }
}

int hw_init(const uint8_t image[0x10000])
{
    memset(&hw, 0, sizeof hw);
    memcpy(hw.mem, image, 0x10000);
#ifndef __EMSCRIPTEN__
    if (!sem_main) sem_main = SDL_CreateSemaphore(0);
    if (!sem_tick) sem_tick = SDL_CreateSemaphore(0);
    return sem_main && sem_tick ? 0 : -1;
#else
    return 0;
#endif
}

void hw_start_main(void (*entry)(void))
{
    main_entry = entry;
#ifdef __EMSCRIPTEN__
    if (!fibers_ready) {
        emscripten_fiber_init_from_current_context(&fib_tick, tick_astack, sizeof tick_astack);
        fibers_ready = 1;
    }
    hw_restart_main();
#else
    main_thread = SDL_CreateThread(main_thread_fn, "hero-main", NULL);
#endif
}

static void run_main_slice(void)
{
    hw.budget = HW_MAIN_CYCLES_PER_FRAME;
    in_main = 1;
#ifdef __EMSCRIPTEN__
    emscripten_fiber_swap(&fib_tick, &fib_main);
#else
    SDL_SemPost(sem_main);
    SDL_SemWait(sem_tick);
#endif
    in_main = 0;
}

void hw_frame(void)
{
    hw.nsidw = 0;
    hw.line = 0;
    int fired = -1;
    while (hw.line < VIC_LINES) {
        unsigned cmp = hw.vic[0x12] | ((unsigned)(hw.vic[0x11] & 0x80) << 1);
        if ((hw.vic[0x1A] & 1) && !hw.irq_masked && cmp == hw.line && fired != (int)hw.line && irq_handler) {
            fired = (int)hw.line;
            hw.vic[0x19] |= 1;
            irq_handler();
            continue;
        }
        render_line(hw.line++);
    }
    run_main_slice();
    hw.frame++;
}

void hw_shutdown(void)
{
#ifndef __EMSCRIPTEN__
    if (main_thread) SDL_DetachThread(main_thread);
    main_thread = NULL;
#endif
}
