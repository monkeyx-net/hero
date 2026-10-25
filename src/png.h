/* Minimal PNG writer (uncompressed deflate): screenshots without libpng. */
#ifndef PNG_H
#define PNG_H
#include <stdint.h>
/* pixels: 0xAARRGGBB, written as 8-bit RGB. Returns 0 on success. */
int png_write(const char *path, const uint32_t *pixels, int width, int height);
uint32_t png_crc32(const uint8_t *p, size_t n, uint32_t crc);
#endif
