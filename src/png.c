#include <stddef.h>
#include "png.h"
#include <stdio.h>
#include <stdlib.h>

uint32_t png_crc32(const uint8_t *p, size_t n, uint32_t crc)
{
    static uint32_t table[256];
    if (!table[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    crc = ~crc;
    while (n--) crc = table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

static int chunk(FILE *f, const char *type, const uint8_t *data, size_t n)
{
    uint8_t hdr[8];
    be32(hdr, (uint32_t)n);
    for (int i = 0; i < 4; i++) hdr[4 + i] = (uint8_t)type[i];
    uint32_t crc = png_crc32(hdr + 4, 4, 0);
    crc = png_crc32(data, n, crc);
    uint8_t tail[4];
    be32(tail, crc);
    return fwrite(hdr, 1, 8, f) == 8 && (n == 0 || fwrite(data, 1, n, f) == n) &&
           fwrite(tail, 1, 4, f) == 4 ? 0 : -1;
}

int png_write(const char *path, const uint32_t *px, int w, int h)
{
    size_t row = (size_t)w * 3 + 1, raw_len = row * (size_t)h;
    size_t nblocks = (raw_len + 65534) / 65535;
    size_t zlen = 2 + raw_len + nblocks * 5 + 4;
    uint8_t *raw = malloc(raw_len), *z = malloc(zlen);
    if (!raw || !z) { free(raw); free(z); return -1; }
    for (int y = 0; y < h; y++) {
        uint8_t *r = raw + row * (size_t)y;
        *r++ = 0;                                   /* filter: none */
        for (int x = 0; x < w; x++) {
            uint32_t c = px[(size_t)y * (size_t)w + (size_t)x];
            *r++ = (uint8_t)(c >> 16); *r++ = (uint8_t)(c >> 8); *r++ = (uint8_t)c;
        }
    }
    size_t o = 0;
    z[o++] = 0x78; z[o++] = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw_len; i++) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    for (size_t pos = 0; pos < raw_len;) {
        size_t n = raw_len - pos > 65535 ? 65535 : raw_len - pos;
        z[o++] = pos + n == raw_len;                /* BFINAL, stored */
        z[o++] = (uint8_t)n; z[o++] = (uint8_t)(n >> 8);
        z[o++] = (uint8_t)~n; z[o++] = (uint8_t)(~n >> 8);
        for (size_t i = 0; i < n; i++) z[o++] = raw[pos + i];
        pos += n;
    }
    be32(z + o, b << 16 | a);
    o += 4;

    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    uint8_t ihdr[13];
    be32(ihdr, (uint32_t)w);
    be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    FILE *f = fopen(path, "wb");
    int rc = -1;
    if (f) {
        rc = fwrite(sig, 1, 8, f) == 8 && !chunk(f, "IHDR", ihdr, 13) && !chunk(f, "IDAT", z, o) &&
             !chunk(f, "IEND", NULL, 0) ? 0 : -1;
        if (fclose(f)) rc = -1;
    }
    free(raw);
    free(z);
    return rc;
}
