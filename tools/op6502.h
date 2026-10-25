/* 6502 opcode table shared by the disassembler and the CPU emulator. */
#ifndef OP6502_H
#define OP6502_H

enum am6502 {
    AM_IMP, AM_ACC, AM_IMM, AM_ZP, AM_ZPX, AM_ZPY, AM_ABS, AM_ABX, AM_ABY,
    AM_IND, AM_IZX, AM_IZY, AM_REL
};

typedef struct {
    const char *mn;  /* mnemonic; lower case = undocumented opcode */
    unsigned char am;
} op6502_t;

extern const op6502_t op6502[256];
extern const int am6502_len[];

#endif
