/* H.E.R.O. - C11 + SDL2 port of the Commodore 64 version.
 *
 * usage: hero [--tap FILE] [--hires] [--border] [--integer] [--scale N] [--windowed]
 *        hero --frames N [--shot F.png] [--dump F] [--input S] [--keys S] [--hires] ...
 *        hero --dump-image FILE       write the decrypted 64 KB image (for the dev tools)
 *
 * The game needs the player's own C64 tape image. It is looked for, in order:
 * --tap FILE, $HERO_TAP, H.E.R.O.tap / hero.tap next to the program, the same
 * names in the current directory (and, in development builds, the source tree).
 * --image FILE is accepted as an alias of --tap (a decrypted 64 KB image works too).
 *
 *   --frames N    headless: run N frames, then write --shot (PNG; BMP if *.bmp)
 *                 and --dump (RAM + colour RAM). Without a tape: the no-tape screen.
 *   --input SPEC  headless joystick script "frame:bits,..." bits u d l r f
 *   --keys SPEC   headless key script "frame:key,..." keys f1 f3 f5 f7 stop (held 8 frames)
 *   --watch HEX   headless: print the frame whenever RAM[HEX] changes
 *   --wav FILE    headless: write the synthesized audio (mono 16-bit WAV)
 *   --hash        headless: print an FNV-1a hash of RAM + colour RAM (replay tests)
 *
 *   --border      show the whole C64 frame with its border (default: the 320x200 play area)
 *   --integer     integer scaling only (default: integer if it fills >= 90%, else sharp bilinear)
 *
 * Keys: arrows + Ctrl/Z/Space = joystick, F1 start, F3 start level, P = RUN/STOP
 *       (pause), PageUp = RESTORE (with P held: reset), F2 = C64/hi-res (pad L1),
 *       F4 = border (pad R1), F11 = fullscreen, F12 = screenshot, Tab = fast forward, Esc = quit.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "font.h"
#include "hero.h"
#include "hw.h"
#include "platform.h"
#include "png.h"
#include "render_hires.h"
#include "sid.h"
#include "tape.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static uint8_t image[0x10000];
static int have_game;
static char tape_error[256];
static uint32_t argb_c64[VIC_FB_W * VIC_FB_H];
static uint32_t argb_hires[HIRES_W * HIRES_H];
static uint8_t notape_fb[VIC_FB_W * VIC_FB_H];

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
        b |= *s == 'u' ? HW_UP : *s == 'd' ? HW_DOWN : *s == 'l' ? HW_LEFT :
             *s == 'r' ? HW_RIGHT : *s == 'f' ? HW_FIRE : 0;
    return b;
}

static uint8_t key_bit(const char *s)
{
    if (!strcmp(s, "f1")) return 1 << KEY_F1;
    if (!strcmp(s, "f3")) return 1 << KEY_F3;
    if (!strcmp(s, "f5")) return 1 << KEY_F5;
    if (!strcmp(s, "f7")) return 1 << KEY_F7;
    if (!strcmp(s, "stop")) return 1 << KEY_RUNSTOP;
    return 0;
}

/* ---- finding the player's tape ---------------------------------------------- */

static int try_tape(const char *path)
{
    if (!path || !*path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    if (tape_load_file(path, image, tape_error, sizeof tape_error) == 0) {
        have_game = 1;
        return 1;
    }
    fprintf(stderr, "%s: %s\n", path, tape_error);
    return 0;
}

static void find_tape(const char *arg)
{
    static const char *names[] = { "H.E.R.O.tap", "hero.tap", "HERO.tap", "H.E.R.O. (USA).tap" };
    char path[1024];
    if (arg) {
        if (!try_tape(arg) && !tape_error[0])
            snprintf(tape_error, sizeof tape_error, *arg ? "%s not found" : "no tape given", arg);
        return;
    }
    if (try_tape(getenv("HERO_TAP"))) return;
    const char *base = platform_base_path();
    for (size_t i = 0; base && i < sizeof names / sizeof names[0]; i++) {
        snprintf(path, sizeof path, "%s%s", base, names[i]);
        if (try_tape(path)) return;
    }
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
        if (try_tape(names[i])) return;
#ifdef __EMSCRIPTEN__
    if (try_tape("/H.E.R.O.tap")) return;
#endif
#ifdef HERO_SOURCE_DIR
    snprintf(path, sizeof path, "%s/H.E.R.O.tap", HERO_SOURCE_DIR);
    if (try_tape(path)) return;
#endif
    if (!tape_error[0]) snprintf(tape_error, sizeof tape_error, "H.E.R.O.tap not found");
}

/* ---- the screen shown without a tape --------------------------------------- */

static void notape_text(int row, const char *s, uint8_t colour)
{
    const uint8_t *rom = font_chargen();
    int len = (int)strlen(s), x0 = (VIC_FB_W - len * 8) / 2, y0 = 44 + row * 11;
    for (int i = 0; i < len; i++) {
        int c = (unsigned char)s[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        int code = c >= 'A' && c <= 'Z' ? c - 'A' + 1 : c == '_' ? 31 : c >= 32 && c < 64 ? c : 32;
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                if (rom[code * 8 + y] >> (7 - x) & 1)
                    notape_fb[(y0 + y) * VIC_FB_W + x0 + i * 8 + x] = colour;
    }
}

static void notape_draw(void)
{
    memset(notape_fb, 6, sizeof notape_fb);                      /* blue border */
    for (int y = 32; y < VIC_FB_H - 32; y++)
        memset(notape_fb + y * VIC_FB_W + 24, 0, VIC_FB_W - 48);
    notape_text(0, "H.E.R.O.", 7);
    notape_text(2, "C64 VERSION - NATIVE PORT", 15);
    notape_text(5, "THE GAME NEEDS YOUR OWN", 1);
    notape_text(6, "C64 TAPE IMAGE: H.E.R.O.TAP", 1);
    notape_text(8, "PUT IT NEXT TO THE PROGRAM, OR", 15);
    notape_text(9, "START WITH --TAP PATH/TO/H.E.R.O.TAP", 15);
    notape_text(10, "(OR SET HERO_TAP)", 15);
    char msg[48];
    snprintf(msg, sizeof msg, "%.40s", tape_error);
    notape_text(13, msg, 10);
    notape_text(16, "ESC TO QUIT", 12);
}

/* ---- output helpers ---------------------------------------------------------- */

static int save_image(const char *path, const uint32_t *px, int w, int h)
{
    size_t n = strlen(path);
    if (n > 4 && !strcmp(path + n - 4, ".bmp")) return platform_save_bmp(path, px, w, h);
    int rc = png_write(path, px, w, h);
    if (rc) fprintf(stderr, "%s: cannot write\n", path);
    return rc;
}

static const uint8_t *current_fb(void) { return have_game ? hw.fb : notape_fb; }

static void start_game(void)
{
    hw_init(image);
    hw.chargen = font_chargen();
    hw.resume_hook = hero_check_restart;
    hw_set_irq(hero_irq);
    sid_reset();
    hw_start_main(hero_main);
}

static void write_wav(const char *path, const int16_t *s, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    uint32_t data = (uint32_t)(n * 2), rate = SID_RATE, brate = SID_RATE * 2, riff = 36 + data;
    uint16_t pcm = 1, ch = 1, align = 2, bits = 16;
    uint32_t fmtlen = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fmtlen, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f);
    fwrite(&brate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f); fwrite(s, 2, n, f);
    fclose(f);
}

static uint32_t state_hash(void)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < sizeof hw.mem; i++) h = (h ^ hw.mem[i]) * 16777619u;
    for (size_t i = 0; i < sizeof hw.color; i++) h = (h ^ (hw.color[i] & 15)) * 16777619u;
    return h;
}

/* ---- headless ---------------------------------------------------------------- */

typedef struct {
    unsigned frames;
    const char *shot, *dump, *input, *keys, *wav;
    int hires, watch, hash, crop;
} headless_t;

static int headless(const headless_t *o)
{
    if (have_game) {
        int16_t *audio = o->wav ? malloc((size_t)o->frames * 1024 * sizeof *audio) : NULL;
        size_t naudio = 0;
        int last = -1;
        static script_t js[512], ks[512];
        int nj = parse_script(o->input, js, 512), nk = parse_script(o->keys, ks, 512), ji = 0, ki = 0;
        unsigned key_release = 0;
        start_game();
        for (unsigned fr = 0; fr < o->frames; fr++) {
            while (ji < nj && js[ji].frame == fr) { hw.joy[0] = hw.joy[1] = joy_bits(js[ji].what); ji++; }
            if (key_release && fr == key_release) { hw.keys = 0; key_release = 0; }
            while (ki < nk && ks[ki].frame == fr) { hw.keys |= key_bit(ks[ki].what); key_release = fr + 8; ki++; }
            hw_frame();
            if (audio) naudio += (size_t)sid_render_frame(hw.sidw, hw.nsidw, audio + naudio);
            if (o->watch >= 0 && hw.mem[o->watch] != last) {
                printf("frame %u: $%04X = $%02X\n", fr, o->watch, hw.mem[o->watch]);
                last = hw.mem[o->watch];
            }
        }
        if (audio) { write_wav(o->wav, audio, naudio); free(audio); }
        if (o->hash) printf("%08x level=%u screen=%u score=%02x%02x%02x lives=%u\n", state_hash(),
                            hw.mem[0x58] + 1u, hw.mem[0x57], hw.mem[0x53], hw.mem[0x54], hw.mem[0x55],
                            hw.mem[0x98] / 2u);
        if (o->dump) {
            FILE *f = fopen(o->dump, "wb");
            if (!f) { perror(o->dump); return 1; }
            fwrite(hw.mem, 1, sizeof hw.mem, f);
            fwrite(hw.color, 1, sizeof hw.color, f);
            fclose(f);
        }
    } else {
        fprintf(stderr, "no game: %s\n", tape_error);
        if (o->hash || o->dump || o->wav) return 1;
    }
    if (o->shot) {
        int w, h;
        render_crop_size(o->crop, &w, &h);
        if (o->hires) {
            render_hires_argb(current_fb(), o->crop, argb_hires);
            return save_image(o->shot, argb_hires, w * 2, h * 2) ? 1 : 0;
        }
        render_c64_argb(current_fb(), o->crop, argb_c64);
        return save_image(o->shot, argb_c64, w, h) ? 1 : 0;
    }
    return 0;
}

/* ---- interactive -------------------------------------------------------------- */

static struct {
    input_t in;
    int hires, crop, integer_only, shots, dirty;
    int fast_presents;        /* presents that did not wait for vsync, in a row */
    double acc;
    uint64_t last;
    int16_t samples[1024];
    const uint32_t *pic;      /* last picture made, and its sizes */
    int pic_w, pic_h, tex_w, tex_h;
} ui;

/* Make the picture from the current frame (only when something changed). */
static void make_picture(void)
{
    render_crop_size(ui.crop, &ui.pic_w, &ui.pic_h);
    if (ui.hires) {
        render_hires_argb(current_fb(), ui.crop, argb_hires);
        ui.pic = argb_hires;
        ui.tex_w = ui.pic_w * 2;
        ui.tex_h = ui.pic_h * 2;
    } else {
        render_c64_argb(current_fb(), ui.crop, argb_c64);
        ui.pic = argb_c64;
        ui.tex_w = ui.pic_w;
        ui.tex_h = ui.pic_h;
    }
}

static void run_once(void)
{
    const double frame_s = 63.0 * 312.0 / 985248.0;          /* PAL: 50.125 Hz */
    input_t *in = &ui.in;
    platform_poll(in);
    if (in->toggle_hires) { ui.hires = !ui.hires; ui.dirty = 1; }
    if (in->toggle_border) { ui.crop = ui.crop == CROP_SCREEN ? CROP_BORDER : CROP_SCREEN; ui.dirty = 1; }
    uint64_t now = platform_ticks();
    ui.acc += (double)(now - ui.last) / (double)platform_tick_freq();
    ui.last = now;
    if (ui.acc > frame_s * 5) ui.acc = frame_s * 5;
    int ran = 0;
    if (have_game) {
        hw.joy[0] = hw.joy[1] = in->joy;
        hw.keys = in->keys;
        if (in->restore) hero_restore();
        while (ui.acc >= frame_s || (in->fast && ran < 8)) {
            hw_frame();
            int n = sid_render_frame(hw.sidw, hw.nsidw, ui.samples);
            if (!in->fast) platform_audio_queue(ui.samples, n);
            if (ui.acc >= frame_s) ui.acc -= frame_s;
            ran++;
            if (!in->fast && ran >= 5) break;
        }
    } else {
        ui.acc = 0;
    }
    int fresh = ran || ui.dirty || !ui.pic;
    if (fresh) make_picture();
    /* With vsync the present paces the loop; without it (software renderer, or
     * a driver that ignores vsync) only present new pictures and sleep until
     * the next frame is due. */
    int vsync = platform_accelerated() && ui.fast_presents < 30;
    if (fresh || in->redraw || vsync) {
        uint64_t t0 = platform_ticks();
        platform_present(fresh ? ui.pic : NULL, ui.tex_w, ui.tex_h, ui.pic_w, ui.pic_h, ui.integer_only);
        double took = (double)(platform_ticks() - t0) / (double)platform_tick_freq();
        if (vsync) ui.fast_presents = took < 0.001 ? ui.fast_presents + 1 : 0;
    }
    ui.dirty = 0;
    if (in->screenshot) {
        char path[64];
        snprintf(path, sizeof path, "hero_%03d.png", ui.shots++);
        save_image(path, ui.pic, ui.tex_w, ui.tex_h);
    }
#ifndef __EMSCRIPTEN__
    if (!vsync || !ran) {
        double wait = frame_s - ui.acc;
        unsigned ms = wait > 0.002 ? (unsigned)((wait - 0.001) * 1000) : 0;
        platform_sleep_ms(ms ? ms : 1);
    }
#endif
}

#ifdef __EMSCRIPTEN__
static void web_frame(void) { run_once(); }
#endif

int main(int argc, char **argv)
{
    const char *tap = NULL, *dump_image = NULL;
    headless_t o = { 0, NULL, NULL, NULL, NULL, NULL, 0, -1, 0, CROP_SCREEN };
    int scale = 3, windowed = 0, integer_only = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--hires")) o.hires = 1;
        else if ((!strcmp(argv[i], "--tap") || !strcmp(argv[i], "--image")) && i + 1 < argc) tap = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--windowed")) windowed = 1;
        else if (!strcmp(argv[i], "--border")) o.crop = CROP_BORDER;
        else if (!strcmp(argv[i], "--integer")) integer_only = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) o.frames = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) o.shot = argv[++i];
        else if (!strcmp(argv[i], "--dump") && i + 1 < argc) o.dump = argv[++i];
        else if (!strcmp(argv[i], "--dump-image") && i + 1 < argc) dump_image = argv[++i];
        else if (!strcmp(argv[i], "--input") && i + 1 < argc) o.input = argv[++i];
        else if (!strcmp(argv[i], "--keys") && i + 1 < argc) o.keys = argv[++i];
        else if (!strcmp(argv[i], "--watch") && i + 1 < argc) o.watch = (int)strtol(argv[++i], NULL, 16);
        else if (!strcmp(argv[i], "--wav") && i + 1 < argc) o.wav = argv[++i];
        else if (!strcmp(argv[i], "--hash")) o.hash = 1;
        else if (!strcmp(argv[i], "--version")) { printf("hero %s\n", HERO_VERSION); return 0; }
        else {
            fprintf(stderr, "usage: %s [--tap FILE] [--hires] [--border] [--integer] [--scale N] [--windowed]\n"
                            "       %s --frames N [--shot F.png] [--dump F] [--input S] [--keys S] [--hires]\n"
                            "       %s --dump-image FILE\n", argv[0], argv[0], argv[0]);
            return 1;
        }
    }
    find_tape(tap);
    if (!have_game) notape_draw();
    if (dump_image) {
        if (!have_game) { fprintf(stderr, "%s\n", tape_error); return 1; }
        FILE *f = fopen(dump_image, "wb");
        if (!f || fwrite(image, 1, sizeof image, f) != sizeof image) { perror(dump_image); return 1; }
        fclose(f);
        return 0;
    }
    if (o.frames) return headless(&o);

#ifdef __EMSCRIPTEN__
    windowed = 1;
#endif
    int win_w, win_h;
    render_crop_size(o.crop, &win_w, &win_h);
    if (platform_init("H.E.R.O.", win_w, win_h, scale > 0 ? scale : 3, !windowed) != 0) return 1;
    if (have_game) {
        platform_audio_open(SID_RATE);
        start_game();
    } else {
        fprintf(stderr, "%s\n", tape_error);
    }
    ui.hires = o.hires;
    ui.crop = o.crop;
    ui.integer_only = integer_only;
    ui.last = platform_ticks();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(web_frame, 0, 1);      /* never returns */
#else
    while (!ui.in.quit) run_once();
    hw_shutdown();
    platform_shutdown();
#endif
    return 0;
}
