/* c64view: run the original game on the c64sys reference model.
 *
 * usage: c64view <mem.bin> [options]
 *   --pc HEX         start address (default 7E00 = protection stage, 8009 = game)
 *   --frames N       headless: run N frames then exit
 *   --dump FILE      headless: write RAM (64 KB) + colour RAM (1 KB) at the end
 *   --bmp FILE       headless: write a screenshot at the end
 *   --input SPEC     headless joystick script "frame:bits,frame:bits,..."
 *                    bits: u d l r f (e.g. "100:f,110:,200:r")
 *   --keys SPEC      headless key script "frame:key,..." keys: f1 f3 f5 f7 sp ret stop
 *   --chargen FILE   4 KB C64 character ROM (optional; ROM-font text is garbage without it)
 *   --coverage FILE  OR executed-opcode map into FILE (64 KB, created if missing)
 *   --watch HEX      headless: print frame number whenever RAM[HEX] changes
 *   --random SEED    headless: random joystick changes every 5-40 frames (coverage runs)
 *
 * Interactive keys: arrows = joystick (both ports), Left Ctrl / Z = fire,
 *   F1/F3/F5/F7, Space, Return, Escape = RUN/STOP, PageUp = RESTORE,
 *   F11 = RAM dump, F12 = screenshot, P = pause, Tab = fast forward.
 */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "c64sys.h"

static c64sys_t m;

static void dump_ram(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fwrite(m.ram, 1, sizeof m.ram, f);
    fwrite(m.color, 1, sizeof m.color, f);
    fclose(f);
    printf("dumped %s at frame %llu\n", path, (unsigned long long)m.frame);
}

static void fb_to_rgb(uint32_t *px)
{
    for (int i = 0; i < C64_FB_W * C64_FB_H; i++) px[i] = 0xFF000000u | c64_palette[m.fb[i] & 15];
}

static void save_bmp(const char *path)
{
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, C64_FB_W, C64_FB_H, 32, SDL_PIXELFORMAT_ARGB8888);
    fb_to_rgb(s->pixels);
    if (SDL_SaveBMP(s, path)) fprintf(stderr, "%s: %s\n", path, SDL_GetError());
    else printf("saved %s\n", path);
    SDL_FreeSurface(s);
}

typedef struct { unsigned frame; char what[8]; } script_t;

static int parse_script(const char *spec, script_t *out, int max)
{
    int n = 0;
    while (spec && *spec && n < max) {
        char *end;
        out[n].frame = (unsigned)strtoul(spec, &end, 10);
        if (*end != ':') break;
        spec = end + 1;
        size_t len = strcspn(spec, ",");
        snprintf(out[n].what, sizeof out[n].what, "%.*s", (int)len, spec);
        n++;
        spec += len;
        if (*spec == ',') spec++;
    }
    return n;
}

static uint8_t joy_bits(const char *s)
{
    uint8_t b = 0;
    for (; *s; s++)
        b |= *s == 'u' ? JOY_UP : *s == 'd' ? JOY_DOWN : *s == 'l' ? JOY_LEFT :
             *s == 'r' ? JOY_RIGHT : *s == 'f' ? JOY_FIRE : 0;
    return b;
}

static const struct { const char *name; SDL_Keycode sdl; int col, row; } keys[] = {
    { "f1", SDLK_F1, 0, 4 }, { "f3", SDLK_F3, 0, 5 }, { "f5", SDLK_F5, 0, 6 },
    { "f7", SDLK_F7, 0, 3 }, { "ret", SDLK_RETURN, 0, 1 }, { "sp", SDLK_SPACE, 7, 4 },
    { "stop", SDLK_ESCAPE, 7, 7 }, { "1", SDLK_1, 7, 0 }, { "2", SDLK_2, 7, 3 },
};

static void set_key_by_name(const char *name, int down)
{
    for (size_t k = 0; k < sizeof keys / sizeof keys[0]; k++)
        if (!strcmp(keys[k].name, name)) c64sys_key(&m, keys[k].col, keys[k].row, down);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <mem.bin> [--pc HEX] [--frames N --dump F --bmp F --input S --keys S]\n", argv[0]);
        return 1;
    }
    static uint8_t img[0x10000];
    FILE *f = fopen(argv[1], "rb");
    if (!f || fread(img, 1, sizeof img, f) != sizeof img) { perror(argv[1]); return 1; }
    fclose(f);

    unsigned pc = 0x7E00, frames = 0;
    const char *dump = NULL, *bmp = NULL;
    static script_t joy_s[256], key_s[256];
    int njoy = 0, nkey = 0;
    const uint8_t *chargen = NULL;
    const char *cov_path = NULL;
    unsigned rnd = 0;
    int watch = -1;
    static uint8_t cov[0x10000];
    for (int i = 2; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--pc")) pc = (unsigned)strtoul(argv[i + 1], NULL, 16);
        else if (!strcmp(argv[i], "--frames")) frames = (unsigned)strtoul(argv[i + 1], NULL, 10);
        else if (!strcmp(argv[i], "--dump")) dump = argv[i + 1];
        else if (!strcmp(argv[i], "--bmp")) bmp = argv[i + 1];
        else if (!strcmp(argv[i], "--input")) njoy = parse_script(argv[i + 1], joy_s, 256);
        else if (!strcmp(argv[i], "--keys")) nkey = parse_script(argv[i + 1], key_s, 256);
        else if (!strcmp(argv[i], "--coverage")) cov_path = argv[i + 1];
        else if (!strcmp(argv[i], "--watch")) watch = (int)strtol(argv[i + 1], NULL, 16);
        else if (!strcmp(argv[i], "--random")) rnd = (unsigned)strtoul(argv[i + 1], NULL, 10);
        else if (!strcmp(argv[i], "--chargen")) {
            static uint8_t cg[4096];
            FILE *cf = fopen(argv[i + 1], "rb");
            if (!cf || fread(cg, 1, sizeof cg, cf) != sizeof cg) { perror(argv[i + 1]); return 1; }
            fclose(cf);
            chargen = cg;
        }
        else { fprintf(stderr, "unknown option %s\n", argv[i]); return 1; }
    }
    c64sys_init(&m, img, (uint16_t)pc);
    m.chargen = chargen;
    if (cov_path) {
        FILE *cf = fopen(cov_path, "rb");
        if (cf) { if (fread(cov, 1, sizeof cov, cf) != sizeof cov) memset(cov, 0, sizeof cov); fclose(cf); }
        m.coverage = cov;
    }

    if (frames) {                                   /* headless */
        int ji = 0, ki = 0;
        char held[8] = "";
        for (unsigned fr = 0; fr < frames; fr++) {
            while (ji < njoy && joy_s[ji].frame == fr) { m.joy[0] = m.joy[1] = joy_bits(joy_s[ji].what); ji++; }
            if (held[0] && ki > 0 && fr == key_s[ki - 1].frame + 3) { set_key_by_name(held, 0); held[0] = 0; }
            while (ki < nkey && key_s[ki].frame == fr) {
                set_key_by_name(key_s[ki].what, 1);
                snprintf(held, sizeof held, "%s", key_s[ki].what);
                ki++;
            }
            if (rnd) {
                static unsigned next_change;
                if (fr >= next_change) {
                    rnd = rnd * 1103515245u + 12345u;
                    static const uint8_t moves[] = { 0, JOY_UP, JOY_UP | JOY_LEFT, JOY_UP | JOY_RIGHT, JOY_LEFT,
                                                     JOY_RIGHT, JOY_DOWN, JOY_DOWN | JOY_LEFT, JOY_DOWN | JOY_RIGHT };
                    uint8_t j = moves[(rnd >> 16) % 9];
                    if ((rnd >> 8) & 1) j |= JOY_FIRE;
                    m.joy[0] = m.joy[1] = j;
                    next_change = fr + 5 + (rnd >> 20) % 36;
                }
            }
            c64sys_run_frame(&m);
            if (watch >= 0) {
                static int last = -1;
                if (m.ram[watch] != last) {
                    printf("frame %u: $%04X = $%02X\n", fr, watch, m.ram[watch]);
                    last = m.ram[watch];
                }
            }
        }
        if (getenv("C64_STATS"))
            printf("main-thread cycles/frame: %.0f\n", (double)m.main_cycles / frames);
        if (dump) dump_ram(dump);
        if (cov_path) { FILE *cf = fopen(cov_path, "wb"); fwrite(cov, 1, sizeof cov, cf); fclose(cf); }
        if (bmp) save_bmp(bmp);
        return 0;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    SDL_Window *win = SDL_CreateWindow("c64view - H.E.R.O. reference", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, C64_FB_W * 3, C64_FB_H * 3,
                                       SDL_WINDOW_RESIZABLE);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ren, C64_FB_W, C64_FB_H);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                         C64_FB_W, C64_FB_H);
    static uint32_t px[C64_FB_W * C64_FB_H];
    int running = 1, paused = 0, shot = 0;
    uint64_t next = SDL_GetPerformanceCounter();
    const uint64_t period = SDL_GetPerformanceFrequency() * 19656u * 63u / 985248u / 63u;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && !e.key.repeat) {
                int down = e.type == SDL_KEYDOWN;
                SDL_Keycode k = e.key.keysym.sym;
                uint8_t bit = k == SDLK_UP ? JOY_UP : k == SDLK_DOWN ? JOY_DOWN :
                              k == SDLK_LEFT ? JOY_LEFT : k == SDLK_RIGHT ? JOY_RIGHT :
                              (k == SDLK_LCTRL || k == SDLK_z) ? JOY_FIRE : 0;
                for (int p = 0; p < 2; p++)
                    m.joy[p] = down ? (uint8_t)(m.joy[p] | bit) : (uint8_t)(m.joy[p] & ~bit);
                for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++)
                    if (keys[i].sdl == k) c64sys_key(&m, keys[i].col, keys[i].row, down);
                if (down && k == SDLK_PAGEUP) c64sys_restore(&m);
                if (down && k == SDLK_p) paused = !paused;
                if (down && k == SDLK_F11) {
                    char path[64];
                    snprintf(path, sizeof path, "ram_%06llu.bin", (unsigned long long)m.frame);
                    dump_ram(path);
                }
                if (down && k == SDLK_F12) {
                    char path[64];
                    snprintf(path, sizeof path, "shot_%03d.bmp", shot++);
                    save_bmp(path);
                }
            }
        }
        const Uint8 *ks = SDL_GetKeyboardState(NULL);
        int ff = ks[SDL_SCANCODE_TAB];
        if (!paused) for (int i = 0; i < (ff ? 8 : 1); i++) c64sys_run_frame(&m);
        fb_to_rgb(px);
        SDL_UpdateTexture(tex, NULL, px, C64_FB_W * 4);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
        next += period;
        uint64_t now = SDL_GetPerformanceCounter();
        if (next > now) SDL_Delay((Uint32)((next - now) * 1000 / SDL_GetPerformanceFrequency()));
        else next = now;
    }
    if (cov_path) { FILE *cf = fopen(cov_path, "wb"); fwrite(cov, 1, sizeof cov, cf); fclose(cf); }
    SDL_Quit();
    return 0;
}
