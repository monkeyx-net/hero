/* Platform layer: window, input, audio, timing. All SDL use except threads
 * (hw.c) and BMP writing lives here. */
#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdint.h>

enum {   /* joystick bits, same meaning as the C64 port bits (active high) */
    IN_UP = 1, IN_DOWN = 2, IN_LEFT = 4, IN_RIGHT = 8, IN_FIRE = 16
};
enum {   /* C64 keys the game reads */
    IN_KEY_F1 = 1, IN_KEY_F3 = 2, IN_KEY_F5 = 4, IN_KEY_F7 = 8, IN_KEY_RUNSTOP = 16
};

typedef struct {
    uint8_t joy;          /* IN_* bits held */
    uint8_t keys;         /* IN_KEY_* held */
    int quit;
    int restore;          /* RESTORE pressed this frame */
    int toggle_hires;     /* F2 / L1 */
    int toggle_border;    /* F4 / R1 */
    int redraw;           /* window changed: present again */
    int screenshot;       /* F12 */
    int fast;             /* Tab held: fast forward */
} input_t;

/* fullscreen: desktop fullscreen (handhelds, default); else a window of width*scale */
int  platform_init(const char *title, int width, int height, int scale, int fullscreen);
void platform_shutdown(void);
void platform_poll(input_t *in);
/* Show a picture: pixels (width x height ARGB; NULL = the last one again) whose
 * logical size is pic_w x pic_h (width may be 2x of it in hi-res mode). Integer
 * scale when it fills >= 90% of the screen (or integer_only), else sharp bilinear. */
void platform_present(const uint32_t *pixels, int width, int height, int pic_w, int pic_h, int integer_only);
int  platform_accelerated(void);
void platform_capture_next(uint32_t *out);   /* tests: read back the next presented output */
uint64_t platform_ticks(void);
uint64_t platform_tick_freq(void);
void platform_sleep_ms(unsigned ms);
int  platform_audio_open(int rate);
void platform_audio_queue(const int16_t *samples, int n);
const char *platform_base_path(void);  /* folder of the program, ends with a separator; may be NULL */
int  platform_save_bmp(const char *path, const uint32_t *pixels, int width, int height);

#endif
