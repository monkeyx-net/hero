#include "tape.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { P_S, P_M, P_L, P_BAD };

/* Pulse classes (TAP units = 8 cycles). Nominal S=$30 M=$42 L=$56. */
static int classify(uint32_t p)
{
    if (p >= 0x24 && p < 0x39) return P_S;
    if (p >= 0x39 && p < 0x4C) return P_M;
    if (p >= 0x4C && p < 0x64) return P_L;
    return P_BAD;
}

typedef struct {
    uint8_t *buf;
    size_t len, cap;
} bytes_t;

static int push_byte(bytes_t *b, uint8_t v)
{
    if (b->len == b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 1024;
        uint8_t *nb = realloc(b->buf, cap);
        if (!nb) return -1;
        b->buf = nb;
        b->cap = cap;
    }
    b->buf[b->len++] = v;
    return 0;
}

/* A block = 9 countdown sync bytes ($89.. or $09..) + payload + XOR checksum. */
static void close_block(bytes_t *cur, tape_block_fn fn, void *user, int *count)
{
    if (cur->len >= 11) {
        uint8_t s0 = cur->buf[0];
        int ok = s0 == 0x89 || s0 == 0x09;
        for (int k = 0; ok && k < 9; k++) ok = cur->buf[k] == (uint8_t)(s0 - k);
        if (ok) {
            const uint8_t *pl = cur->buf + 9;
            size_t n = cur->len - 10;
            uint8_t x = 0;
            for (size_t k = 0; k < n; k++) x ^= pl[k];
            (*count)++;
            if (x == cur->buf[cur->len - 1]) fn(user, pl, n, s0 == 0x09);
        }
    }
    cur->len = 0;
}

int tape_decode(const uint8_t *tap, size_t len, tape_block_fn fn, void *user)
{
    if (len < 20 || memcmp(tap, "C64-TAPE-RAW", 12) != 0) return -1;
    int version = tap[12];
    uint32_t *pul = malloc(len * sizeof *pul);
    if (!pul) return -1;
    size_t n = 0;
    for (size_t i = 20; i < len; i++) {
        if (tap[i]) pul[n++] = tap[i];
        else if (version >= 1 && i + 3 < len) {
            pul[n++] = (tap[i + 1] | tap[i + 2] << 8 | (uint32_t)tap[i + 3] << 16) / 8;
            i += 3;
        } else pul[n++] = 0x100;
    }

    bytes_t cur = {0};
    int count = 0;
    size_t i = 0;
    while (i + 1 < n) {
        int a = classify(pul[i]), b = classify(pul[i + 1]);
        if (a == P_L && b == P_M && i + 20 <= n) {            /* byte marker */
            uint8_t v = 0;
            int ok = 1, ones = 0, parity = 0;
            for (int bit = 0; bit < 9 && ok; bit++) {
                int x = classify(pul[i + 2 + bit * 2]), y = classify(pul[i + 3 + bit * 2]);
                int val = x == P_S && y == P_M ? 0 : x == P_M && y == P_S ? 1 : -1;
                if (val < 0) ok = 0;
                else if (bit < 8) { v |= (uint8_t)(val << bit); ones += val; }
                else parity = val;
            }
            if (ok && parity == ((ones & 1) ^ 1)) {
                if (push_byte(&cur, v)) break;
                i += 20;
                continue;
            }
            i++;
            continue;
        }
        if (a == P_L && b == P_S) {                              /* end of data */
            close_block(&cur, fn, user, &count);
            i += 2;
            continue;
        }
        if (a == P_BAD || (a == P_S && b == P_S && cur.len)) close_block(&cur, fn, user, &count);
        i++;
    }
    close_block(&cur, fn, user, &count);
    free(cur.buf);
    free(pul);
    return count;
}

/* ---- the two files on the tape ---------------------------------------------- */

typedef struct {
    uint8_t *mem;
    int pending;            /* header seen: type 1/3 program follows */
    uint16_t start;
    int files;              /* bit 0: loader at $0801, bit 1: game at $7E00 */
} collect_t;

static void on_block(void *user, const uint8_t *pl, size_t n, int repeat)
{
    collect_t *c = user;
    if (repeat) return;
    if (n == 192 && pl[0] >= 1 && pl[0] <= 5) {
        c->pending = pl[0] == 1 || pl[0] == 3;
        c->start = (uint16_t)(pl[1] | pl[2] << 8);
        return;
    }
    if (!c->pending) return;
    c->pending = 0;
    if (c->start + n > 0x10000) return;
    memcpy(c->mem + c->start, pl, n);
    if (c->start == 0x0801) c->files |= 1;
    if (c->start == 0x7E00) c->files |= 2;
}

/* the loader's cipher ($0FE8): per byte k += step; mem ^= k */
static void decrypt(uint8_t *mem, unsigned start, int pages, uint8_t key, uint8_t step)
{
    for (unsigned a = start; a < start + (unsigned)pages * 256u; a++) {
        key = (uint8_t)(key + step);
        mem[a] ^= key;
    }
}

static int check_image(const uint8_t *img, char *err, size_t errlen)
{
    static const uint8_t cbm80[5] = { 0xC3, 0xC2, 0xCD, 0x38, 0x30 };
    if (memcmp(img + 0x8004, cbm80, 5) != 0 || img[0x8000] != 0x09 || img[0x8001] != 0x80) {
        snprintf(err, errlen, "not the C64 H.E.R.O. tape (game image not found)");
        return -1;
    }
    return 0;
}

int tape_load_mem(const uint8_t *data, size_t len, uint8_t image[0x10000], char *err, size_t errlen)
{
    if (len == 0x10000) {                       /* already decrypted image */
        memcpy(image, data, 0x10000);
        return check_image(image, err, errlen);
    }
    memset(image, 0, 0x10000);
    collect_t c = { image, 0, 0, 0 };
    if (tape_decode(data, len, on_block, &c) < 0) {
        snprintf(err, errlen, "not a C64 tape image (.tap)");
        return -1;
    }
    if (c.files != 3) {
        snprintf(err, errlen, "tape incomplete or damaged (loader %s, game %s)",
                 c.files & 1 ? "ok" : "missing", c.files & 2 ? "ok" : "missing");
        return -1;
    }
    /* loader stages: $1000 -> $1100 -> LOAD -> $7E00/$8000 (docs/c64_memory_map.md) */
    decrypt(image, 0x1100, 1, image[0x102C], image[0x102D]);
    decrypt(image, 0x1A00, 4, image[0x112C], image[0x112D]);
    /* the game file was loaded after stage 2 read its key: re-apply in order */
    uint8_t k1 = image[0x1D35], s1 = image[0x1D36];
    decrypt(image, 0x7E00, 2, k1, s1);
    decrypt(image, 0x8000, 0x40, image[0x7FFA], image[0x7FFB]);
    return check_image(image, err, errlen);
}

int tape_load_file(const char *path, uint8_t image[0x10000], char *err, size_t errlen)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, errlen, "cannot open %s", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 16 * 1024 * 1024) {
        fclose(f);
        snprintf(err, errlen, "%s: unexpected size", path);
        return -1;
    }
    uint8_t *buf = malloc((size_t)n);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(buf);
        snprintf(err, errlen, "%s: read error", path);
        return -1;
    }
    fclose(f);
    int rc = tape_load_mem(buf, (size_t)n, image, err, errlen);
    free(buf);
    return rc;
}
