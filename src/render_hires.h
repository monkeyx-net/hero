/* Turning the finished C64 frame (palette indices, VIC_FB_W x VIC_FB_H) into
 * the picture that is shown. Reads only the frame; never touches game state.
 *
 *  - crop: the 320x200 play area (default; this game's border is always black)
 *    or the whole frame with its border.
 *  - C64 mode: the pixels as they are (the GPU scales them, see platform_present).
 *  - hi-res mode: Scale2x on the CPU (2x), the rest of the scaling on the GPU.
 */
#ifndef RENDER_HIRES_H
#define RENDER_HIRES_H
#include <stdint.h>
#include "vic.h"

enum { CROP_SCREEN, CROP_BORDER };

#define HIRES_SCALE 2
#define HIRES_W (VIC_FB_W * HIRES_SCALE)    /* largest output (border shown) */
#define HIRES_H (VIC_FB_H * HIRES_SCALE)

/* Size of the shown area for a crop mode (the picture's logical size). */
void render_crop_size(int crop, int *w, int *h);
/* out gets w x h (C64) or 2w x 2h (hi-res) ARGB pixels; returns the scale (1 or 2). */
int render_c64_argb(const uint8_t *fb, int crop, uint32_t *out);
int render_hires_argb(const uint8_t *fb, int crop, uint32_t *out);

#endif
