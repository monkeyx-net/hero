/* herounpack: rebuild the decrypted C64 H.E.R.O. memory image from the two
 * PRGs on the tape (output of tapdump), replicating the loader's stages:
 *
 *   $1000  decrypt $1100-$11FF   key = $102C/$102D
 *   $1100  decrypt $1A00-$1DFF   key = $112C/$112D, JSR $1A00
 *          LOAD "ACT_IVIS_ION" -> $7E00-$BFFF
 *          decrypt $7E00-$7FFF   key = $1D35/$1D36
 *          decrypt $8000-$BFFF   key = $7FFA/$7FFB, JMP $7E00
 *
 * Cipher ($0FE8): per byte  k += step; mem ^= k  (k carries across pages).
 *
 * usage: herounpack <loader.prg> <game.prg> <out.bin>
 *   out.bin = full 64 KB RAM image after decryption (entry point $7E00)
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t mem[0x10000];

static void load_prg(const char *path, uint16_t expect)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    int lo = fgetc(f), hi = fgetc(f);
    uint16_t addr = (uint16_t)(lo | hi << 8);
    if (addr != expect) {
        fprintf(stderr, "%s: load address $%04X, expected $%04X\n", path, addr, expect);
        exit(1);
    }
    size_t n = fread(mem + addr, 1, 0x10000u - addr, f);
    fclose(f);
    printf("%s: $%04X-$%04zX\n", path, addr, addr + n);
}

static void decrypt(uint16_t start, int pages, uint8_t key, uint8_t step)
{
    for (unsigned a = start; a < start + pages * 256u; a++) {
        key = (uint8_t)(key + step);
        mem[a] ^= key;
    }
    printf("decrypt $%04X-$%04X key=$%02X step=$%02X\n",
           start, start + pages * 256 - 1, key, step);
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s <loader.prg> <game.prg> <out.bin>\n", argv[0]);
        return 1;
    }
    load_prg(argv[1], 0x0801);
    decrypt(0x1100, 1, mem[0x102C], mem[0x102D]);
    decrypt(0x1A00, 4, mem[0x112C], mem[0x112D]);
    uint8_t k1 = mem[0x1D35], s1 = mem[0x1D36];   /* read before LOAD */
    load_prg(argv[2], 0x7E00);
    decrypt(0x7E00, 2, k1, s1);
    decrypt(0x8000, 0x40, mem[0x7FFA], mem[0x7FFB]);

    FILE *f = fopen(argv[3], "wb");
    if (!f) { perror(argv[3]); return 1; }
    fwrite(mem, 1, sizeof mem, f);
    fclose(f);
    printf("wrote %s (entry $7E00)\n", argv[3]);
    return 0;
}
