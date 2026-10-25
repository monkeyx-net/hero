/* difftest: routine-level differential test of the C translation against the
 * original 6502 code.
 *
 * 1. Run the original game on the reference model (tools/c64sys) with random
 *    joystick input and take state snapshots (RAM, colour RAM, VIC, SID).
 * 2. For every translated routine and snapshot: run the original 6502 routine
 *    on the CPU core and the C function on identical copies of the state, both
 *    through the port's hardware model (src/hw.c), then compare everything.
 *
 * Routines that wait for the next frame are detected (step limit / yield) and
 * skipped for that snapshot.
 *
 * usage: difftest <H.E.R.O.tap | image> [snapshots] [seed] [only-routine-hex]
 *        difftest <H.E.R.O.tap | image> screens
 *   "screens": build every screen of every level with the original $877A and
 *   with the C translation (from the same game state) and compare.
 */
#define _POSIX_C_SOURCE 200809L
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/hero.h"
#include "../src/tape.h"
#include "../tools/c64sys.h"

typedef struct { uint16_t addr; void (*fn)(void); const char *name; int irq; } routine_t;

#define R(a, f) { 0x##a, f, #f, 0 }
#define I(a, f) { 0x##a, f, #f, 1 }    /* IRQ handler: ends with RTI */
static const routine_t routines[] = {
    R(8169, s8169), R(818B, s818B), R(81E5, s81E5), R(8206, s8206), R(8218, s8218),
    R(8236, s8236), R(8290, s8290), R(82BC, s82BC), R(82DB, s82DB), R(82F7, s82F7),
    R(8339, s8339), R(836E, s836E), R(8384, s8384), R(838F, s838F), R(83B5, s83B5),
    R(83DB, s83DB), R(8401, s8401), R(8427, s8427), R(8445, s8445), R(8490, s8490),
    R(84A8, s84A8), R(84DF, s84DF), R(84EE, s84EE), R(8515, s8515), R(85A1, s85A1),
    R(85D5, s85D5), R(85F5, s85F5), R(8631, s8631), R(863F, s863F), R(865E, s865E),
    R(867B, s867B), R(86AB, s86AB), R(86ED, s86ED), R(8709, s8709), R(8734, s8734),
    R(873E, s873E), R(8775, s8775), R(877A, s877A), R(886C, s886C), R(8906, s8906),
    R(891A, s891A), R(8940, s8940), R(8987, s8987), R(89BD, s89BD), R(8A14, s8A14),
    R(8A6E, s8A6E), R(8A9A, s8A9A), R(8AFB, s8AFB), R(8B45, s8B45), R(8B59, s8B59),
    R(8B86, s8B86), R(8B91, s8B91), R(8BB4, s8BB4), R(8BCD, s8BCD), R(8BE3, s8BE3),
    R(8C3D, s8C3D), R(8C5F, s8C5F), R(8CAA, s8CAA), R(8CF5, s8CF5), R(8CFD, s8CFD),
    R(8D23, s8D23), R(8DC8, s8DC8), R(8DD2, s8DD2), R(8DDE, s8DDE), R(8DF6, s8DF6),
    R(8DFA, s8DFA), R(8DFC, s8DFC), R(8E1A, s8E1A), R(8E45, s8E45), R(8E90, s8E90),
    R(8E9A, s8E9A), R(8EDD, s8EDD), R(8EE5, s8EE5), R(8F0B, s8F0B), R(8F37, s8F37),
    R(8F5C, s8F5C), R(8F75, s8F75), R(8F8E, s8F8E), R(8FC0, s8FC0), R(8FE8, s8FE8),
    R(9033, s9033), R(904B, s904B), R(90A9, s90A9), R(90CE, s90CE), R(9103, s9103),
    R(9162, s9162), R(91AD, s91AD), R(91DD, s91DD), R(9207, s9207), R(9231, s9231),
    R(92E3, s92E3), R(9300, s9300), R(937C, s937C), R(93A4, s93A4), R(93EE, s93EE),
    R(9408, s9408), R(9412, s9412), R(942D, s942D), R(944B, s944B), R(946C, s946C),
    R(9498, s9498), R(94C4, s94C4), R(9524, s9524), R(9539, s9539), R(9563, s9563),
    R(9598, s9598), R(95AE, s95AE), R(9619, s9619), R(9632, s9632), R(967A, s967A),
    R(9685, s9685), R(96A9, s96A9), R(96D2, s96D2), R(970B, s970B), R(97AC, s97AC),
    R(98ED, s98ED), R(9912, s9912), R(99A0, s99A0), R(9A22, s9A22), R(9A33, s9A33),
    R(9A44, s9A44), R(9A5D, s9A5D), R(9A6A, s9A6A), R(9A75, s9A75), R(9A83, s9A83),
    R(9B1D, s9B1D), R(9B61, s9B61), R(9B6A, s9B6A), R(9BDC, s9BDC), R(9BEC, s9BEC),
    R(9C0B, s9C0B), R(9C93, s9C93), R(9D40, s9D40), R(9D76, s9D76), R(9DA5, s9DA5),
    R(9DD1, s9DD1), R(9E03, s9E03), R(9E8B, s9E8B), R(9EB4, s9EB4), R(9EC5, s9EC5),
    R(9ED2, s9ED2), R(9EDB, s9EDB), R(9EF5, s9EF5), R(9EF9, s9EF9), R(9F13, s9F13),
    R(9F23, s9F23), R(9F2E, s9F2E), R(9F39, s9F39), R(9F45, s9F45), R(9F4F, s9F4F),
    I(9734, s9734), I(975F, s975F), I(97CB, s97CB),
};

/* ---- state snapshots ------------------------------------------------------ */

typedef struct {
    uint8_t ram[0x10000], color[0x400], vic[0x40], sid[0x20];
} snap_t;

static void load_snap(const snap_t *s)
{
    memcpy(hw.mem, s->ram, sizeof hw.mem);
    memcpy(hw.color, s->color, sizeof hw.color);
    memcpy(hw.vic, s->vic, sizeof hw.vic);
    memcpy(hw.sid, s->sid, sizeof hw.sid);
    hw.line = 0x40;
    hw.cia1_pra = 0xFF;
    hw.cia1_ddra = 0;
    hw.joy[0] = hw.joy[1] = 0;
    hw.keys = 0;
    hw.nsidw = 0;
    hw.irq_masked = 0;
}

typedef struct {
    uint8_t mem[0x10000], color[0x400], vic[0x40], sid[0x20];
    uint8_t a, x, y;
    unsigned line;
} result_t;

static void save_result(result_t *r)
{
    memcpy(r->mem, hw.mem, sizeof r->mem);
    memcpy(r->color, hw.color, sizeof r->color);
    memcpy(r->vic, hw.vic, sizeof r->vic);
    memcpy(r->sid, hw.sid, sizeof r->sid);
    r->line = hw.line;
}

/* ---- running the original 6502 routine on the port's hardware model ------- */

static int cpu_in_irq;

static uint8_t cpu_rd(cpu6502_t *c, uint16_t a)
{
    (void)c;
    /* busy waits on the raster: advance the beam one line per read, like the
     * C version's hw_raster_wait */
    if (a == 0xD012 && cpu_in_irq) {
        hw_raster_wait(hw.line + 1);
        return (uint8_t)hw.line;
    }
    return hw_rd(a);
}

static void cpu_wr(cpu6502_t *c, uint16_t a, uint8_t v) { (void)c; hw_wr(a, v); }

/* returns 0 ok, 1 step limit (waits / runaway) */
static int run_6502(const routine_t *r, uint8_t a, uint8_t x, uint8_t y, int carry, result_t *out)
{
    static uint8_t stk[256];
    cpu6502_t c = {0};
    c.read = cpu_rd;
    c.write = cpu_wr;
    c.stack = stk;                    /* keep page 1 as plain shared data */
    c.a = a; c.x = x; c.y = y;
    c.p = (uint8_t)(F_U | (carry ? F_C : 0));
    /* return address $0000 - 1 */
    stk[0xFF] = 0xFF; stk[0xFE] = 0xFF; c.s = 0xFD;
    if (r->irq) {                     /* KERNAL-style frame: PC, P, A, X, Y */
        stk[0xFF] = 0x00; stk[0xFE] = 0x00; stk[0xFD] = F_U;
        stk[0xFC] = a; stk[0xFB] = x; stk[0xFA] = y;
        c.s = 0xF9;
    }
    c.pc = r->addr;
    cpu_in_irq = r->irq;
    for (long steps = 0; steps < 2000000; steps++) {
        if (r->irq ? (c.pc == 0x0000) : (c.pc == 0x0000)) {
            out->a = c.a; out->x = c.x; out->y = c.y;
            save_result(out);
            return 0;
        }
        if (c.pc >= 0xE000 || c.jammed) return 1;
        /* frame waits are skipped identically on both sides (see test_yield_hook):
         * $9E9F/$9EA8 wait for the $4D countdown, $9C02 waits for the joystick */
        if (c.pc == 0x9E9F || c.pc == 0x9EA8) hw.mem[0x4D] = 0;
        if (c.pc == 0x9C02 && !((hw.mem[0x90] | hw.mem[0x91]) & hw.mem[0x2A])) hw.mem[0x90] |= hw.mem[0x2A];
        cpu6502_step(&c);
    }
    return 1;
}

/* ---- running the C translation --------------------------------------------- */

static sigjmp_buf yield_jmp;
static int c_yielded;
static void test_yield_hook(void)
{
    /* "time passes": same effect as the 6502 side's wait emulation */
    if (hw.wait_reason == HW_WAIT_COUNTDOWN) hw.mem[0x4D] = 0;
    else if (hw.wait_reason == HW_WAIT_JOYSTICK) hw.mem[0x90] |= hw.mem[0x2A];
}
static void on_alarm(int sig) { (void)sig; siglongjmp(yield_jmp, 2); }   /* runaway C loop */
extern void (*hw_test_yield)(void);

static int run_c(const routine_t *r, uint8_t a, uint8_t x, uint8_t y, int carry, result_t *out)
{
    ra = a; rx = x; ry = y; rc = carry;
    c_yielded = 0;
    struct itimerval tv = { {0, 0}, {0, 300000} }, off = { {0, 0}, {0, 0} };
    if (sigsetjmp(yield_jmp, 1)) { setitimer(ITIMER_REAL, &off, NULL); return 1; }
    setitimer(ITIMER_REAL, &tv, NULL);
    r->fn();
    setitimer(ITIMER_REAL, &off, NULL);
    out->a = ra; out->x = rx; out->y = ry;
    save_result(out);
    return 0;
}

/* ---- comparison ------------------------------------------------------------ */

static int compare(const routine_t *r, const result_t *o, const result_t *t, int verbose)
{
    int bad = 0;
    for (unsigned i = 0; i < 0x10000; i++) {
        if (o->mem[i] != t->mem[i]) {
            if (verbose && bad < 8)
                printf("    $%04X: 6502 %02X  C %02X\n", i, o->mem[i], t->mem[i]);
            bad++;
        }
    }
    for (unsigned i = 0; i < 0x400; i++)
        if ((o->color[i] & 15) != (t->color[i] & 15)) {
            if (verbose && bad < 8) printf("    colour $%04X: 6502 %X  C %X\n", 0xD800 + i, o->color[i], t->color[i]);
            bad++;
        }
    for (unsigned i = 0; i < 0x2F; i++)
        if (o->vic[i] != t->vic[i] && i != 0x19) {
            if (verbose && bad < 8) printf("    VIC $D0%02X: 6502 %02X  C %02X\n", i, o->vic[i], t->vic[i]);
            bad++;
        }
    for (unsigned i = 0; i < 0x19; i++)
        if (o->sid[i] != t->sid[i]) {
            if (verbose && bad < 8) printf("    SID $D4%02X: 6502 %02X  C %02X\n", i, o->sid[i], t->sid[i]);
            bad++;
        }
    if (!r->irq && (o->a != t->a || o->x != t->x || o->y != t->y)) {
        if (verbose) printf("    regs: 6502 A=%02X X=%02X Y=%02X  C A=%02X X=%02X Y=%02X\n",
                            o->a, o->x, o->y, t->a, t->x, t->y);
        bad += 0x10000;                                  /* register-only marker */
    }
    return bad;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <H.E.R.O.tap | image> [snapshots] [seed] [only-routine-hex]\n", argv[0]);
        return 1;
    }
    static uint8_t img[0x10000];
    char err[256];
    if (tape_load_file(argv[1], img, err, sizeof err)) { fprintf(stderr, "%s: %s\n", argv[1], err); return 1; }
    int screens_mode = argc > 2 && !strcmp(argv[2], "screens");
    int nsnap = screens_mode ? 5 : argc > 2 ? atoi(argv[2]) : 40;
    unsigned seed = argc > 3 ? (unsigned)strtoul(argv[3], NULL, 10) : 1;
    int only = argc > 4 ? (int)strtol(argv[4], NULL, 16) : -1;
    setvbuf(stdout, NULL, _IOLBF, 0);

    /* 1. snapshots from the original running on the reference model: one game per
     *    F3 start option (levels 1, 5, 9, 13, PRO), random joystick, snapshots
     *    every 97 frames after the game started. */
    static c64sys_t ref;
    snap_t *snaps = malloc(sizeof(snap_t) * (size_t)nsnap);
    unsigned rnd = seed;
    int s = 0;
    for (int game = 0; s < nsnap; game++) {
        int f3 = game % 5, quota = (nsnap - s) / (5 - (game % 5 < 5 ? game % 5 : 4));
        if (quota < 1) quota = 1;
        c64sys_init(&ref, img, 0x7E00);
        unsigned next = 0;
        int taken = 0;
        for (int fr = 0; taken < quota && s < nsnap && fr < 60000; fr++) {
            if (fr > 420 && (unsigned)fr >= next) {
                rnd = rnd * 1103515245u + 12345u;
                static const uint8_t moves[] = { 0, JOY_UP, JOY_UP | JOY_LEFT, JOY_UP | JOY_RIGHT, JOY_LEFT,
                                                 JOY_RIGHT, JOY_DOWN, JOY_DOWN | JOY_LEFT, JOY_DOWN | JOY_RIGHT };
                uint8_t j = moves[(rnd >> 16) % 9];
                if ((rnd >> 8) & 1) j |= JOY_FIRE;
                ref.joy[0] = ref.joy[1] = j;
                next = (unsigned)fr + 5 + (rnd >> 20) % 36;
            }
            for (int k = 0; k < f3; k++) {                     /* F3 presses select the start level */
                if (fr == 200 + k * 30) c64sys_key(&ref, 0, 5, 1);
                if (fr == 208 + k * 30) c64sys_key(&ref, 0, 5, 0);
            }
            if (fr == 400) c64sys_key(&ref, 0, 4, 1);          /* F1: start the game */
            if (fr == 408) c64sys_key(&ref, 0, 4, 0);
            c64sys_run_frame(&ref);
            if (fr > 500 && fr % 97 == 0) {
                memcpy(snaps[s].ram, ref.ram, sizeof ref.ram);
                memcpy(snaps[s].color, ref.color, sizeof ref.color);
                memcpy(snaps[s].vic, ref.vic, sizeof ref.vic);
                memcpy(snaps[s].sid, ref.sid, sizeof ref.sid);
                s++;
                taken++;
            }
        }
        printf("game %d (F3 x%d): snapshots up to %d, level $%02X\n", game, f3, s, ref.ram[0x58]);
    }

    if (screens_mode) {
        hw_init(img);
        hw_test_yield = test_yield_hook;
        signal(SIGALRM, on_alarm);
        static result_t o2, t2;
        int screens = 0, bad_screens = 0;
        for (int lvl = 0; lvl < 20; lvl++) {
            int last = snaps[0].ram[0xA3D0 + lvl];
            for (int scr = 0; scr <= last; scr++) {
                snap_t st = snaps[0];
                st.ram[0x58] = (uint8_t)lvl;
                st.ram[0x57] = (uint8_t)scr;
                st.ram[0xBD] = 0xFF;
                const routine_t r877a = { 0x877A, s877A, "s877A", 0 };
                load_snap(&st);
                int w1 = run_6502(&r877a, 0, 0, 0, 0, &o2);
                load_snap(&st);
                int w2 = run_c(&r877a, 0, 0, 0, 0, &t2);
                int bad = (w1 || w2) ? -1 : compare(&r877a, &o2, &t2, 0) & 0xFFFF;
                screens++;
                if (bad) {
                    bad_screens++;
                    printf("level %2d screen %2d: %s\n", lvl + 1, scr, bad < 0 ? "did not finish" : "MISMATCH");
                    if (bad > 0) compare(&r877a, &o2, &t2, 1);
                }
            }
        }
        printf("%d screens built, %d differ\n", screens, bad_screens);
        free(snaps);
        return bad_screens ? 1 : 0;
    }

    /* 2. run every routine on every snapshot with a few register sets */
    hw_init(img);
    hw_test_yield = test_yield_hook;
    signal(SIGALRM, on_alarm);
    static const uint8_t regvals[] = { 0x00, 0x01, 0x02, 0x04, 0x07, 0x0E, 0x0F, 0x10, 0x80, 0xFF };
    static result_t o, t;
    int total = 0, failed_routines = 0, reg_routines = 0;
    for (size_t ri = 0; ri < sizeof routines / sizeof routines[0]; ri++) {
        const routine_t *r = &routines[ri];
        if (only >= 0 && r->addr != only) continue;
        int runs = 0, fails = 0, regfails = 0, skipped = 0, shown = 0;
        for (int s = 0; s < nsnap; s++) {
            for (int k = 0; k < 3; k++) {
                rnd = rnd * 1103515245u + 12345u;
                uint8_t a = regvals[(rnd >> 8) % 10], x = regvals[(rnd >> 12) % 10];
                uint8_t y = regvals[(rnd >> 16) % 10];
                int carry = (rnd >> 20) & 1;
                if (k == 0) { a = 0; x = 2; y = 0; }
                load_snap(&snaps[s]);
                int w1 = run_6502(r, a, x, y, carry, &o);
                load_snap(&snaps[s]);
                int w2 = run_c(r, a, x, y, carry, &t);
                if (w1 || w2) { skipped++; continue; }
                runs++;
                int bad = compare(r, &o, &t, 0);
                if (bad >= 0x10000) {
                    if (!regfails && getenv("DIFF_REGS"))
                        printf("  %s A=%02X X=%02X Y=%02X C=%d -> 6502 A=%02X X=%02X Y=%02X | C A=%02X X=%02X Y=%02X\n",
                               r->name, a, x, y, carry, o.a, o.x, o.y, t.a, t.x, t.y);
                    regfails++;
                    bad -= 0x10000;
                }
                if (bad) {
                    fails++;
                    if (shown++ < 2) {
                        printf("  %s ($%04X) snapshot %d A=%02X X=%02X Y=%02X C=%d: %d differences\n",
                               r->name, r->addr, s, a, x, y, carry, bad);
                        compare(r, &o, &t, 1);
                    }
                }
            }
        }
        total += runs;
        if (fails) failed_routines++;
        if (regfails) reg_routines++;
        printf("%-6s $%04X  %4d runs  %4d state-fail  %4d reg-fail  %4d skipped(wait)%s\n", r->name,
               r->addr, runs, fails, regfails, skipped, fails ? "  <-- STATE MISMATCH" : regfails ? "  <-- regs" : "");
    }
    printf("\n%d routine runs: %d routines with state mismatches, %d with register-only mismatches\n",
           total, failed_routines, reg_routines);
    free(snaps);
    return failed_routines ? 1 : 0;
}
