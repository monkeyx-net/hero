#include "platform.h"
#include <SDL.h>
#include <stdio.h>

/* An integer scale is used when it fills at least this much of the fitted size;
 * otherwise the picture is fitted with "sharp bilinear": drawn crisp at the next
 * integer scale into a render target, then scaled down smoothly. */
#define INTEGER_FILL 0.9
#define TARGET_MAX 4096           /* largest render target side */

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture, *target;
static int tex_w, tex_h, target_w, target_h, accelerated;
static uint32_t *capture_buf;     /* tests: read the next presented output back */
static SDL_GameController *pad;
static SDL_AudioDeviceID audio;
static int audio_rate;

int platform_init(const char *title, int width, int height, int scale, int fullscreen)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return -1;
    }
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              width * scale, height * scale,
                              SDL_WINDOW_RESIZABLE | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!window) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return -1; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");          /* nearest */
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) /* KMSDRM without GLES, or a dummy driver */
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return -1; }
    SDL_RendererInfo info;
    accelerated = !SDL_GetRendererInfo(renderer, &info) && (info.flags & SDL_RENDERER_ACCELERATED);
    if (fullscreen) SDL_ShowCursor(SDL_DISABLE);
    for (int i = 0; i < SDL_NumJoysticks() && !pad; i++)
        if (SDL_IsGameController(i)) pad = SDL_GameControllerOpen(i);
    return 0;
}

void platform_shutdown(void)
{
    if (audio) SDL_CloseAudioDevice(audio);
    if (pad) SDL_GameControllerClose(pad);
    if (target) SDL_DestroyTexture(target);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void platform_poll(input_t *in)
{
    in->restore = in->toggle_hires = in->toggle_border = in->screenshot = in->redraw = 0;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) in->quit = 1;
        if (e.type == SDL_WINDOWEVENT) in->redraw = 1;
        if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) pad = SDL_GameControllerOpen(e.cdevice.which);
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE: in->quit = 1; break;
            case SDLK_F2: in->toggle_hires = 1; break;
            case SDLK_F4: in->toggle_border = 1; break;
            case SDLK_F12: in->screenshot = 1; break;
            case SDLK_PAGEUP: in->restore = 1; break;
            case SDLK_F11: {
                Uint32 fs = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                SDL_SetWindowFullscreen(window, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                break;
            }
            default: break;
            }
        }
    }
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    uint8_t j = 0, keys = 0;
    if (k[SDL_SCANCODE_UP]) j |= IN_UP;
    if (k[SDL_SCANCODE_DOWN]) j |= IN_DOWN;
    if (k[SDL_SCANCODE_LEFT]) j |= IN_LEFT;
    if (k[SDL_SCANCODE_RIGHT]) j |= IN_RIGHT;
    if (k[SDL_SCANCODE_LCTRL] || k[SDL_SCANCODE_RCTRL] || k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_SPACE]) j |= IN_FIRE;
    if (k[SDL_SCANCODE_F1]) keys |= IN_KEY_F1;
    if (k[SDL_SCANCODE_F3]) keys |= IN_KEY_F3;
    if (k[SDL_SCANCODE_F5]) keys |= IN_KEY_F5;
    if (k[SDL_SCANCODE_F7]) keys |= IN_KEY_F7;
    if (k[SDL_SCANCODE_P] || k[SDL_SCANCODE_PAUSE]) keys |= IN_KEY_RUNSTOP;
    in->fast = k[SDL_SCANCODE_TAB];
    if (pad) {
        const int dead = 12000;
        int ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        int ay = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        if (ay < -dead || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP)) j |= IN_UP;
        if (ay > dead || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) j |= IN_DOWN;
        if (ax < -dead || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) j |= IN_LEFT;
        if (ax > dead || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) j |= IN_RIGHT;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) ||
            SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B)) j |= IN_FIRE;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START)) keys |= IN_KEY_F1;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK)) keys |= IN_KEY_F3;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y)) keys |= IN_KEY_RUNSTOP;
        /* shoulders: L1 C64/hi-res picture, R1 border (on press) */
        static int l1, r1;
        int nl1 = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        int nr1 = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        if (nl1 && !l1) in->toggle_hires = 1;
        if (nr1 && !r1) in->toggle_border = 1;
        l1 = nl1;
        r1 = nr1;
    }
    in->joy = j;
    in->keys = keys;
}

/* The largest scale that fits the output, and whether it is used as is (an
 * integer scale) or the picture is fitted. */
static double fit_scale(int pic_w, int pic_h, int integer_only, int *integer)
{
    int out_w, out_h;
    SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
    double fx = (double)out_w / pic_w, fy = (double)out_h / pic_h;
    double fit = fx < fy ? fx : fy;
    int whole = (int)fit;
    *integer = integer_only || fit < 1 || whole >= fit * INTEGER_FILL;
    return *integer ? (whole < 1 ? 1 : whole) : fit;
}

static SDL_Texture *fit_target(int w, int h)
{
    if (target && target_w == w && target_h == h) return target;
    if (target) SDL_DestroyTexture(target);
    target = NULL;
    target_w = w;
    target_h = h;
    if (w > TARGET_MAX || h > TARGET_MAX || !SDL_RenderTargetSupported(renderer)) return NULL;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");          /* linear, read when the texture is made */
    target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    return target;
}

void platform_present(const uint32_t *pixels, int width, int height, int pic_w, int pic_h, int integer_only)
{
    if (!texture || tex_w != width || tex_h != height) {
        if (texture) SDL_DestroyTexture(texture);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, width, height);
        tex_w = width;
        tex_h = height;
        if (!texture) { fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError()); return; }
        if (!pixels) return;
    }
    if (pixels) SDL_UpdateTexture(texture, NULL, pixels, width * 4);

    int out_w, out_h, integer;
    SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
    double fit = fit_scale(pic_w, pic_h, integer_only, &integer);
    int w = (int)(pic_w * fit), h = (int)(pic_h * fit);
    SDL_Rect dst = { (out_w - w) / 2, (out_h - h) / 2, w, h };

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    int crisp = integer ? (int)fit : (int)fit + 1;              /* output pixels per C64 pixel before fitting */
    SDL_Texture *t = integer ? NULL : fit_target(pic_w * crisp, pic_h * crisp);
    if (t && !SDL_SetRenderTarget(renderer, t)) {
        SDL_RenderCopy(renderer, texture, NULL, NULL);          /* crisp: nearest */
        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderCopy(renderer, t, NULL, &dst);                /* smooth: linear */
    } else {
        SDL_RenderCopy(renderer, texture, NULL, &dst);
    }
    if (capture_buf) {      /* tests: the composed output, before the buffers swap */
        SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, capture_buf, out_w * 4);
        capture_buf = NULL;
    }
    SDL_RenderPresent(renderer);
}

int platform_accelerated(void) { return accelerated; }
void platform_capture_next(uint32_t *out) { capture_buf = out; }

uint64_t platform_ticks(void) { return SDL_GetPerformanceCounter(); }
uint64_t platform_tick_freq(void) { return SDL_GetPerformanceFrequency(); }
void platform_sleep_ms(unsigned ms) { SDL_Delay(ms); }

int platform_audio_open(int rate)
{
    SDL_AudioSpec want = {0}, have;
    want.freq = rate;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!audio) { fprintf(stderr, "audio: %s\n", SDL_GetError()); return -1; }
    audio_rate = have.freq;
    SDL_PauseAudioDevice(audio, 0);
    return 0;
}

void platform_audio_queue(const int16_t *samples, int n)
{
    if (!audio) return;
    /* keep latency bounded (~3 frames) */
    if (SDL_GetQueuedAudioSize(audio) > (Uint32)(audio_rate / 50 * 3 * 2)) return;
    SDL_QueueAudio(audio, samples, (Uint32)n * 2);
}

int platform_save_bmp(const char *path, const uint32_t *pixels, int width, int height)
{
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom((void *)pixels, width, height, 32,
                                                        width * 4, SDL_PIXELFORMAT_ARGB8888);
    int rc = s ? SDL_SaveBMP(s, path) : -1;
    if (rc) fprintf(stderr, "%s: %s\n", path, SDL_GetError());
    SDL_FreeSurface(s);
    return rc;
}

const char *platform_base_path(void)
{
    static char *base;
    static int done;
    if (!done) { base = SDL_GetBasePath(); done = 1; }
    return base;
}
