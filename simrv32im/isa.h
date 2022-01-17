#ifndef ISA_H
#define ISA_H

#include <stdint.h>


#define ISA_XSIZE (32)
typedef uint32_t t_isaXSize;
typedef int32_t t_isaInt;
typedef uint32_t t_isaUInt;

#define SRA(x, amt) (((x)>>(amt)) | (((x)<0)?(((1<<(amt))-1)<<(32-(amt))):0))
#define SEXT(x, s)  ((x) | (((x) & (1<<((s)-1))) ? ((uint32_t)-1)<<(s) : 0))

#define BITS(x, a, b)      (((x) >> (a)) & (((uint32_t)1 << ((b) - (a))) - 1))

#define ISA_INST_OPCODE(x)       BITS(x,  0,  7)
#define ISA_INST_RD(x)           BITS(x,  7, 12)
#define ISA_INST_FUNCT3(x)       BITS(x, 12, 15)
#define ISA_INST_RS1(x)          BITS(x, 15, 20)
#define ISA_INST_RS2(x)          BITS(x, 20, 25)
#define ISA_INST_FUNCT7(x)       BITS(x, 25, 32)
#define ISA_INST_I_IMM12(x)      BITS(x, 20, 32)
#define ISA_INST_I_IMM12_SEXT(x) SEXT(ISA_INST_I_IMM12(x), 12)
#define ISA_INST_S_IMM12(x)      (BITS(x, 7, 12) | (BITS(x, 25, 32) << 5))
#define ISA_INST_S_IMM12_SEXT(x) SEXT(ISA_INST_S_IMM12(x), 12)
#define ISA_INST_B_IMM13(x)      ((BITS(x,7,8)<<11) | (BITS(x,8,12)<<1) | \
                                  (BITS(x,25,31)<<5) | (BITS(x,31,32)<<12))
#define ISA_INST_B_IMM13_SEXT(x) SEXT(ISA_INST_B_IMM13(x), 13)
#define ISA_INST_U_IMM20(x)      BITS(x, 12, 32)
#define ISA_INST_U_IMM20_SEXT(x) SEXT(ISA_INST_U_IMM20(x), 20)
#define ISA_INST_J_IMM21(x)      ((BITS(x,12,20)<<12) | (BITS(x,20,21)<<11) | \
                                  (BITS(x,21,31)<<1) | (BITS(x,31,32)<<20))
#define ISA_INST_J_IMM21_SEXT(x) SEXT(ISA_INST_J_IMM21(x), 21)

#define ISA_INST_OPCODE_CODE(x)  (((x) << 2) | 3)
#define ISA_INST_OPCODE_LOAD     ISA_INST_OPCODE_CODE(0x00)
#define ISA_INST_OPCODE_OPIMM    ISA_INST_OPCODE_CODE(0x04)
#define ISA_INST_OPCODE_AUIPC    ISA_INST_OPCODE_CODE(0x05)
#define ISA_INST_OPCODE_STORE    ISA_INST_OPCODE_CODE(0x08)
#define ISA_INST_OPCODE_OP       ISA_INST_OPCODE_CODE(0x0C)
#define ISA_INST_OPCODE_LUI      ISA_INST_OPCODE_CODE(0x0D)
#define ISA_INST_OPCODE_BRANCH   ISA_INST_OPCODE_CODE(0x18)
#define ISA_INST_OPCODE_JALR     ISA_INST_OPCODE_CODE(0x19)
#define ISA_INST_OPCODE_JAL      ISA_INST_OPCODE_CODE(0x1B)
#define ISA_INST_OPCODE_SYSTEM   ISA_INST_OPCODE_CODE(0x1C)


int isaDisassemble(uint32_t instr, char *out, int bufsz);


#endif
