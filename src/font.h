/* Lookalike replacement for the C64 character ROM (screen codes), used only
 * for the text row that the original draws with the ROM font. Original
 * glyph designs - the Commodore ROM is not shipped. */
#ifndef FONT_H
#define FONT_H
#include <stdint.h>
const uint8_t *font_chargen(void);   /* 4 KB, C64 char ROM layout */
#endif
