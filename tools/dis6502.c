/* dis6502: linear 6502 disassembler for PRG or raw binaries.
 *
 * usage: dis6502 [-r loadaddr] [-l labels] file [start [end]]
 *   file      PRG (first two bytes = load address) unless -r is given
 *   -r addr   treat file as raw binary loaded at hex addr
 *   -l file   labels/annotations: lines "ADDR NAME [; comment]" (hex addr),
 *             or "ADDR-END .data NAME" to dump a range as bytes
 *   -c file   coverage map (64 KB, nonzero = opcode executed): code not
 *             executed is printed as .byte rows
 *   start/end hex addresses to restrict output
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "op6502.h"

typedef struct {
    uint16_t addr, end;
    int is_data;
    char name[48];
    char comment[160];
} label_t;

static label_t *labels;
static int nlabels;

/* Recursive descent from executed code: mark statically reachable instructions
 * with 2 so not-yet-executed paths still disassemble as code. Stops at
 * undocumented opcodes, which almost always means we ran into data. */
static void flow(const uint8_t *mem, uint8_t *cov, unsigned lo, unsigned hi)
{
    static uint16_t work[0x10000];
    unsigned n = 0;
    for (unsigned a = lo; a < hi; a++) if (cov[a]) work[n++] = (uint16_t)a;
    while (n) {
        uint16_t pc = work[--n];
        const op6502_t *o = &op6502[mem[pc]];
        uint16_t w = (uint16_t)(mem[(uint16_t)(pc + 1)] | mem[(uint16_t)(pc + 2)] << 8);
        uint16_t next[2];
        int k = 0;
        int stop = !strcmp(o->mn, "RTS") || !strcmp(o->mn, "RTI") || !strcmp(o->mn, "JMP") ||
                   !strcmp(o->mn, "BRK") || !strcmp(o->mn, "jam");
        if (!stop) next[k++] = (uint16_t)(pc + am6502_len[o->am]);
        if (o->am == AM_REL) next[k++] = (uint16_t)(pc + 2 + (int8_t)mem[(uint16_t)(pc + 1)]);
        if (o->am == AM_ABS && (!strcmp(o->mn, "JSR") || !strcmp(o->mn, "JMP"))) next[k++] = w;
        for (int i = 0; i < k; i++) {
            uint16_t t = next[i];
            if (t < lo || t >= hi || cov[t]) continue;
            const char *mn = op6502[mem[t]].mn;
            if (mn[0] >= 'a' || !strcmp(mn, "BRK")) continue;
            cov[t] = 2;
            work[n++] = t;
        }
    }
}

static const label_t *find_label(uint16_t a)
{
    for (int i = 0; i < nlabels; i++)
        if (labels[i].addr == a) return &labels[i];
    return NULL;
}

static const char *addr_name(uint16_t a, char *buf, int width)
{
    const label_t *l = find_label(a);
    if (l && l->name[0]) return l->name;
    snprintf(buf, 8, width == 2 ? "$%02X" : "$%04X", a);
    return buf;
}

static void load_labels(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); exit(1); }
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || !*p) continue;
        label_t l = {0};
        char *end;
        l.addr = (uint16_t)strtoul(p, &end, 16);
        l.end = l.addr;
        p = end;
        if (*p == '-') { l.end = (uint16_t)strtoul(p + 1, &end, 16); p = end; }
        char *semi = strchr(p, ';');
        if (semi) {
            *semi = 0;
            char *c = semi + 1;
            while (*c == ' ') c++;
            c[strcspn(c, "\r\n")] = 0;
            snprintf(l.comment, sizeof l.comment, "%s", c);
        }
        char w1[48] = "", w2[48] = "";
        sscanf(p, "%47s %47s", w1, w2);
        if (strcmp(w1, ".data") == 0) { l.is_data = 1; snprintf(l.name, sizeof l.name, "%s", w2); }
        else snprintf(l.name, sizeof l.name, "%s", w1);
        labels = realloc(labels, (size_t)(nlabels + 1) * sizeof *labels);
        labels[nlabels++] = l;
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    long raw = -1;
    const char *lbl = NULL;
    const char *covp = NULL;
    int ai = 1;
    while (ai < argc && argv[ai][0] == '-') {
        if (!strcmp(argv[ai], "-r") && ai + 1 < argc) raw = strtol(argv[++ai], NULL, 16);
        else if (!strcmp(argv[ai], "-l") && ai + 1 < argc) lbl = argv[++ai];
        else if (!strcmp(argv[ai], "-c") && ai + 1 < argc) covp = argv[++ai];
        else { fprintf(stderr, "bad option %s\n", argv[ai]); return 1; }
        ai++;
    }
    if (ai >= argc) {
        fprintf(stderr, "usage: %s [-r loadaddr] [-l labels] file [start [end]]\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[ai], "rb");
    if (!f) { perror(argv[ai]); return 1; }
    static uint8_t buf[0x10002];
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    if (lbl) load_labels(lbl);
    static uint8_t cov[0x10000];
    if (covp) {
        FILE *cf = fopen(covp, "rb");
        if (!cf || fread(cov, 1, sizeof cov, cf) != sizeof cov) { perror(covp); return 1; }
        fclose(cf);
    }

    static uint8_t mem[0x10000];
    unsigned load;
    const uint8_t *src = buf;
    if (raw >= 0) load = (unsigned)raw;
    else { load = buf[0] | buf[1] << 8; src += 2; n -= 2; }
    if (load + n > 0x10000) n = 0x10000 - load;
    memcpy(mem + load, src, n);

    unsigned start = load, end = load + (unsigned)n;
    if (ai + 1 < argc) start = (unsigned)strtoul(argv[ai + 1], NULL, 16);
    if (ai + 2 < argc) end = (unsigned)strtoul(argv[ai + 2], NULL, 16);
    if (covp)                              /* code labels seed the flow analysis */
        for (int i = 0; i < nlabels; i++)
            if (!labels[i].is_data && labels[i].addr >= start && labels[i].addr < end && !cov[labels[i].addr])
                cov[labels[i].addr] = 2;
    if (covp) flow(mem, cov, start, end);

    unsigned pc = start;
    while (pc < end) {
        const label_t *l = find_label((uint16_t)pc);
        if (l) {
            if (l->comment[0]) printf("\n; %s\n", l->comment);
            if (l->name[0]) printf("%s:\n", l->name);
            if (l->is_data) {
                for (unsigned a = pc; a <= l->end && a < end; a += 16) {
                    printf("%04X  .byte ", a);
                    for (unsigned k = a; k < a + 16 && k <= l->end; k++)
                        printf("%s$%02X", k == a ? "" : ",", mem[k]);
                    printf("\n");
                }
                pc = l->end + 1u;
                continue;
            }
        }
        if (covp && !cov[pc]) {           /* not executed: dump bytes up to next code/label */
            unsigned a = pc, k = 0;
            printf("%04X  .byte ", a);
            while (pc < end && !cov[pc] && k < 16 && (k == 0 || !find_label((uint16_t)pc))) {
                printf("%s$%02X", k ? "," : "", mem[pc]);
                pc++; k++;
            }
            printf("\n");
            continue;
        }
        uint8_t opc = mem[pc];
        const op6502_t *o = &op6502[opc];
        int len = am6502_len[o->am];
        if (pc + (unsigned)len > end) len = (int)(end - pc);
        uint8_t lo = mem[(pc + 1) & 0xFFFF], hi = mem[(pc + 2) & 0xFFFF];
        uint16_t w = (uint16_t)(lo | hi << 8);
        char tmp[8], arg[64] = "";
        switch (o->am) {
        case AM_IMP: break;
        case AM_ACC: snprintf(arg, sizeof arg, "A"); break;
        case AM_IMM: snprintf(arg, sizeof arg, "#$%02X", lo); break;
        case AM_ZP:  snprintf(arg, sizeof arg, "%s", addr_name(lo, tmp, 2)); break;
        case AM_ZPX: snprintf(arg, sizeof arg, "%s,X", addr_name(lo, tmp, 2)); break;
        case AM_ZPY: snprintf(arg, sizeof arg, "%s,Y", addr_name(lo, tmp, 2)); break;
        case AM_ABS: snprintf(arg, sizeof arg, "%s", addr_name(w, tmp, 4)); break;
        case AM_ABX: snprintf(arg, sizeof arg, "%s,X", addr_name(w, tmp, 4)); break;
        case AM_ABY: snprintf(arg, sizeof arg, "%s,Y", addr_name(w, tmp, 4)); break;
        case AM_IND: snprintf(arg, sizeof arg, "(%s)", addr_name(w, tmp, 4)); break;
        case AM_IZX: snprintf(arg, sizeof arg, "(%s,X)", addr_name(lo, tmp, 2)); break;
        case AM_IZY: snprintf(arg, sizeof arg, "(%s),Y", addr_name(lo, tmp, 2)); break;
        case AM_REL: snprintf(arg, sizeof arg, "%s",
                              addr_name((uint16_t)(pc + 2 + (int8_t)lo), tmp, 4)); break;
        }
        printf(covp && cov[pc] == 2 ? "%04X? " : "%04X  ", pc);
        for (int k = 0; k < 3; k++)
            if (k < len) printf("%02X ", mem[(pc + (unsigned)k) & 0xFFFF]);
            else printf("   ");
        printf(" %s %s\n", o->mn, arg);
        pc += (unsigned)len;
    }
    return 0;
}
