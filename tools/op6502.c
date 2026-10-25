#include "op6502.h"

const int am6502_len[] = { 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 2, 2, 2 };

#define I AM_IMP
#define A AM_ACC
#define M AM_IMM
#define Z AM_ZP
#define ZX AM_ZPX
#define ZY AM_ZPY
#define B AM_ABS
#define BX AM_ABX
#define BY AM_ABY
#define N AM_IND
#define IX AM_IZX
#define IY AM_IZY
#define R AM_REL

const op6502_t op6502[256] = {
/* 00 */ {"BRK",I},{"ORA",IX},{"jam",I},{"slo",IX},{"nop",Z},{"ORA",Z},{"ASL",Z},{"slo",Z},
/* 08 */ {"PHP",I},{"ORA",M},{"ASL",A},{"anc",M},{"nop",B},{"ORA",B},{"ASL",B},{"slo",B},
/* 10 */ {"BPL",R},{"ORA",IY},{"jam",I},{"slo",IY},{"nop",ZX},{"ORA",ZX},{"ASL",ZX},{"slo",ZX},
/* 18 */ {"CLC",I},{"ORA",BY},{"nop",I},{"slo",BY},{"nop",BX},{"ORA",BX},{"ASL",BX},{"slo",BX},
/* 20 */ {"JSR",B},{"AND",IX},{"jam",I},{"rla",IX},{"BIT",Z},{"AND",Z},{"ROL",Z},{"rla",Z},
/* 28 */ {"PLP",I},{"AND",M},{"ROL",A},{"anc",M},{"BIT",B},{"AND",B},{"ROL",B},{"rla",B},
/* 30 */ {"BMI",R},{"AND",IY},{"jam",I},{"rla",IY},{"nop",ZX},{"AND",ZX},{"ROL",ZX},{"rla",ZX},
/* 38 */ {"SEC",I},{"AND",BY},{"nop",I},{"rla",BY},{"nop",BX},{"AND",BX},{"ROL",BX},{"rla",BX},
/* 40 */ {"RTI",I},{"EOR",IX},{"jam",I},{"sre",IX},{"nop",Z},{"EOR",Z},{"LSR",Z},{"sre",Z},
/* 48 */ {"PHA",I},{"EOR",M},{"LSR",A},{"alr",M},{"JMP",B},{"EOR",B},{"LSR",B},{"sre",B},
/* 50 */ {"BVC",R},{"EOR",IY},{"jam",I},{"sre",IY},{"nop",ZX},{"EOR",ZX},{"LSR",ZX},{"sre",ZX},
/* 58 */ {"CLI",I},{"EOR",BY},{"nop",I},{"sre",BY},{"nop",BX},{"EOR",BX},{"LSR",BX},{"sre",BX},
/* 60 */ {"RTS",I},{"ADC",IX},{"jam",I},{"rra",IX},{"nop",Z},{"ADC",Z},{"ROR",Z},{"rra",Z},
/* 68 */ {"PLA",I},{"ADC",M},{"ROR",A},{"arr",M},{"JMP",N},{"ADC",B},{"ROR",B},{"rra",B},
/* 70 */ {"BVS",R},{"ADC",IY},{"jam",I},{"rra",IY},{"nop",ZX},{"ADC",ZX},{"ROR",ZX},{"rra",ZX},
/* 78 */ {"SEI",I},{"ADC",BY},{"nop",I},{"rra",BY},{"nop",BX},{"ADC",BX},{"ROR",BX},{"rra",BX},
/* 80 */ {"nop",M},{"STA",IX},{"nop",M},{"sax",IX},{"STY",Z},{"STA",Z},{"STX",Z},{"sax",Z},
/* 88 */ {"DEY",I},{"nop",M},{"TXA",I},{"xaa",M},{"STY",B},{"STA",B},{"STX",B},{"sax",B},
/* 90 */ {"BCC",R},{"STA",IY},{"jam",I},{"sha",IY},{"STY",ZX},{"STA",ZX},{"STX",ZY},{"sax",ZY},
/* 98 */ {"TYA",I},{"STA",BY},{"TXS",I},{"tas",BY},{"shy",BX},{"STA",BX},{"shx",BY},{"sha",BY},
/* A0 */ {"LDY",M},{"LDA",IX},{"LDX",M},{"lax",IX},{"LDY",Z},{"LDA",Z},{"LDX",Z},{"lax",Z},
/* A8 */ {"TAY",I},{"LDA",M},{"TAX",I},{"lxa",M},{"LDY",B},{"LDA",B},{"LDX",B},{"lax",B},
/* B0 */ {"BCS",R},{"LDA",IY},{"jam",I},{"lax",IY},{"LDY",ZX},{"LDA",ZX},{"LDX",ZY},{"lax",ZY},
/* B8 */ {"CLV",I},{"LDA",BY},{"TSX",I},{"las",BY},{"LDY",BX},{"LDA",BX},{"LDX",BY},{"lax",BY},
/* C0 */ {"CPY",M},{"CMP",IX},{"nop",M},{"dcp",IX},{"CPY",Z},{"CMP",Z},{"DEC",Z},{"dcp",Z},
/* C8 */ {"INY",I},{"CMP",M},{"DEX",I},{"sbx",M},{"CPY",B},{"CMP",B},{"DEC",B},{"dcp",B},
/* D0 */ {"BNE",R},{"CMP",IY},{"jam",I},{"dcp",IY},{"nop",ZX},{"CMP",ZX},{"DEC",ZX},{"dcp",ZX},
/* D8 */ {"CLD",I},{"CMP",BY},{"nop",I},{"dcp",BY},{"nop",BX},{"CMP",BX},{"DEC",BX},{"dcp",BX},
/* E0 */ {"CPX",M},{"SBC",IX},{"nop",M},{"isc",IX},{"CPX",Z},{"SBC",Z},{"INC",Z},{"isc",Z},
/* E8 */ {"INX",I},{"SBC",M},{"NOP",I},{"sbc",M},{"CPX",B},{"SBC",B},{"INC",B},{"isc",B},
/* F0 */ {"BEQ",R},{"SBC",IY},{"jam",I},{"isc",IY},{"nop",ZX},{"SBC",ZX},{"INC",ZX},{"isc",ZX},
/* F8 */ {"SED",I},{"SBC",BY},{"nop",I},{"isc",BY},{"nop",BX},{"SBC",BX},{"INC",BX},{"isc",BX},
};
