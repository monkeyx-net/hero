/* tapdump: decode a C64 raw TAP (v0/v1) recorded with the standard CBM
 * Kernal ROM loader into its header/data blocks and write PRG files.
 *
 * usage: tapdump <file.tap> <outdir>
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { P_S, P_M, P_L, P_BAD };

/* Pulse thresholds (TAP units = 8 cycles). Nominal S=0x30 M=0x42 L=0x56. */
static int classify(uint32_t p)
{
    if (p >= 0x24 && p < 0x39) return P_S;
    if (p >= 0x39 && p < 0x4C) return P_M;
    if (p >= 0x4C && p < 0x64) return P_L;
    return P_BAD;
}

typedef struct {
    uint8_t *data;
    size_t len, cap;
    size_t pulse_pos; /* index of first pulse, for the log */
} block_t;

static block_t *blocks;
static size_t nblocks, capblocks;

static void block_push(block_t *b, uint8_t v)
{
    if (b->len == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        b->data = realloc(b->data, b->cap);
        if (!b->data) { perror("realloc"); exit(1); }
    }
    b->data[b->len++] = v;
}

static void blocks_close(block_t *cur)
{
    if (cur->len == 0) return;
    if (nblocks == capblocks) {
        capblocks = capblocks ? capblocks * 2 : 16;
        blocks = realloc(blocks, capblocks * sizeof *blocks);
        if (!blocks) { perror("realloc"); exit(1); }
    }
    blocks[nblocks++] = *cur;
    memset(cur, 0, sizeof *cur);
}

static uint8_t *load_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)n);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) { perror("read"); exit(1); }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

/* Expand TAP bytes into pulse lengths (v1 zero-byte = 24-bit overflow). */
static uint32_t *read_pulses(const uint8_t *tap, size_t len, size_t *npulses)
{
    if (len < 20 || memcmp(tap, "C64-TAPE-RAW", 12) != 0) {
        fprintf(stderr, "not a C64 TAP file\n");
        exit(1);
    }
    int version = tap[12];
    uint32_t *p = malloc(len * sizeof *p);
    size_t n = 0;
    for (size_t i = 20; i < len; i++) {
        if (tap[i] != 0) {
            p[n++] = tap[i];
        } else if (version >= 1 && i + 3 < len) {
            p[n++] = (tap[i + 1] | tap[i + 2] << 8 | (uint32_t)tap[i + 3] << 16) / 8;
            i += 3;
        } else {
            p[n++] = 0x100;
        }
    }
    *npulses = n;
    return p;
}

static void decode(const uint32_t *pul, size_t n)
{
    block_t cur = {0};
    size_t i = 0;
    while (i + 1 < n) {
        int a = classify(pul[i]), b = classify(pul[i + 1]);
        if (a == P_L && b == P_M) {          /* byte marker */
            if (i + 20 > n) break;
            uint8_t v = 0;
            int ok = 1, ones = 0, parity = 0;
            for (int bit = 0; bit < 9; bit++) {
                int x = classify(pul[i + 2 + bit * 2]);
                int y = classify(pul[i + 3 + bit * 2]);
                int val;
                if (x == P_S && y == P_M) val = 0;
                else if (x == P_M && y == P_S) val = 1;
                else { ok = 0; break; }
                if (bit < 8) { v |= (uint8_t)(val << bit); ones += val; }
                else parity = val;
            }
            if (ok && parity == ((ones & 1) ^ 1)) {
                if (cur.len == 0) cur.pulse_pos = i;
                block_push(&cur, v);
                i += 20;
                continue;
            }
            fprintf(stderr, "bad byte at pulse %zu (block %zu, offset %zu)\n", i, nblocks, cur.len);
            i++;
            continue;
        }
        if (a == P_L && b == P_S) {          /* end-of-data marker */
            blocks_close(&cur);
            i += 2;
            continue;
        }
        if (a == P_BAD || (a == P_S && b == P_S && cur.len)) /* gap or leader */
            blocks_close(&cur);
        i++;
    }
    blocks_close(&cur);
}

/* A decoded block = 9 countdown sync bytes + payload + XOR checksum. */
static int block_payload(const block_t *b, const uint8_t **pl, size_t *pl_len, int *repeat)
{
    if (b->len < 11) return -1;
    uint8_t s0 = b->data[0];
    if (s0 != 0x89 && s0 != 0x09) return -1;
    for (int k = 0; k < 9; k++)
        if (b->data[k] != (uint8_t)(s0 - k)) return -1;
    *repeat = s0 == 0x09;
    *pl = b->data + 9;
    *pl_len = b->len - 10;
    uint8_t x = 0;
    for (size_t k = 0; k < *pl_len; k++) x ^= (*pl)[k];
    return x == b->data[b->len - 1] ? 0 : 1;
}

static void petscii_name(const uint8_t *src, char *dst)
{
    int n = 16;
    while (n > 0 && src[n - 1] == 0x20) n--;
    for (int k = 0; k < n; k++)
        dst[k] = ((src[k] >= '0' && src[k] <= '9') || (src[k] >= 'A' && src[k] <= 'Z') ||
                  src[k] == '.' || src[k] == '-') ? (char)src[k] : '_';
    dst[n] = 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <file.tap> <outdir>\n", argv[0]);
        return 1;
    }
    size_t len, npul;
    uint8_t *tap = load_file(argv[1], &len);
    uint32_t *pul = read_pulses(tap, len, &npul);
    decode(pul, npul);
    printf("%zu pulses, %zu raw blocks\n", npul, nblocks);

    int have_hdr = 0, files = 0, errors = 0;
    uint16_t start = 0, end = 0;
    char name[17] = "";
    const uint8_t *prev = NULL;
    size_t prev_len = 0;

    for (size_t k = 0; k < nblocks; k++) {
        const uint8_t *pl;
        size_t pl_len;
        int repeat;
        int r = block_payload(&blocks[k], &pl, &pl_len, &repeat);
        if (r < 0) {
            printf("block %2zu: %5zu bytes, no CBM sync (non-standard data?) first=%02X\n",
                   k, blocks[k].len, blocks[k].data[0]);
            errors++;
            continue;
        }
        printf("block %2zu: %5zu bytes %s %s", k, pl_len, repeat ? "repeat" : "first ",
               r ? "CHECKSUM-BAD" : "ok");
        if (r) errors++;
        if (repeat) {
            int same = prev && prev_len == pl_len && memcmp(prev, pl, pl_len) == 0;
            printf(" %s\n", same ? "(matches first copy)" : "(DIFFERS from first copy)");
            continue;
        }
        prev = pl;
        prev_len = pl_len;

        if (pl_len == 192 && pl[0] >= 1 && pl[0] <= 5) {
            start = (uint16_t)(pl[1] | pl[2] << 8);
            end = (uint16_t)(pl[3] | pl[4] << 8);
            petscii_name(pl + 5, name);
            have_hdr = pl[0] == 1 || pl[0] == 3;
            printf(" HEADER type %d \"%s\" $%04X-$%04X\n", pl[0], name, start, end);
            continue;
        }
        printf(" DATA");
        if (have_hdr) {
            char path[512];
            snprintf(path, sizeof path, "%s/%02d_%s.prg", argv[2], files, name[0] ? name : "noname");
            FILE *f = fopen(path, "wb");
            if (!f) { perror(path); return 1; }
            fputc(start & 0xFF, f);
            fputc(start >> 8, f);
            fwrite(pl, 1, pl_len, f);
            fclose(f);
            printf(" -> %s ($%04X-$%04zX)", path, start, start + pl_len);
            if ((size_t)(end - start) != pl_len)
                printf(" [header says %u bytes]", (unsigned)(end - start));
            files++;
            have_hdr = 0;
        }
        printf("\n");
    }
    printf("%d file(s) written, %d error(s)\n", files, errors);
    return errors ? 2 : 0;
}
