/* Small SID (6581-ish) synthesizer driven by the game's register writes. */
#ifndef SID_H
#define SID_H
#include <stdint.h>
#include "hw.h"

#define SID_RATE 44100

void sid_reset(void);
/* Render one PAL frame of audio, applying the frame's register writes at their
 * raster line. Returns the number of samples written to out (max 1024). */
int sid_render_frame(const hw_sidw_t *w, unsigned nw, int16_t *out);

#endif
