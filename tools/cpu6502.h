/* Minimal NMOS 6502 core (documented + stable undocumented opcodes).
 * Memory access goes through callbacks so hosts can model I/O. */
#ifndef CPU6502_H
#define CPU6502_H
#include <stdint.h>

typedef struct cpu6502 {
    uint16_t pc;
    uint8_t a, x, y, s, p;
    uint64_t cycles;
    int jammed;
    void *user;
    uint8_t *stack;          /* optional private 256-byte stack (tests); NULL = page 1 */
    uint8_t (*read)(struct cpu6502 *c, uint16_t addr);
    void (*write)(struct cpu6502 *c, uint16_t addr, uint8_t v);
} cpu6502_t;

enum { F_C = 1, F_Z = 2, F_I = 4, F_D = 8, F_B = 16, F_U = 32, F_V = 64, F_N = 128 };

void cpu6502_reset(cpu6502_t *c);        /* loads PC from $FFFC */
int  cpu6502_step(cpu6502_t *c);         /* executes one instruction, returns cycles */
void cpu6502_irq(cpu6502_t *c);
void cpu6502_nmi(cpu6502_t *c);

#endif
