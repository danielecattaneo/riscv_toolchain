#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "encode.h"

#define MASK(n) (((uint32_t)1 << (uint32_t)(n)) - (uint32_t)1)
#define SHIFT_MASK(x, a, b) (((uint32_t)(x) & MASK(b-a)) << a)

#define ENC_OPCODE_CODE(x)  (((x) << 2) | 3)
#define ENC_OPCODE_LOAD     ENC_OPCODE_CODE(0x00)
#define ENC_OPCODE_OPIMM    ENC_OPCODE_CODE(0x04)
#define ENC_OPCODE_AUIPC    ENC_OPCODE_CODE(0x05)
#define ENC_OPCODE_STORE    ENC_OPCODE_CODE(0x08)
#define ENC_OPCODE_OP       ENC_OPCODE_CODE(0x0C)
#define ENC_OPCODE_LUI      ENC_OPCODE_CODE(0x0D)
#define ENC_OPCODE_BRANCH   ENC_OPCODE_CODE(0x18)
#define ENC_OPCODE_JALR     ENC_OPCODE_CODE(0x19)
#define ENC_OPCODE_JAL      ENC_OPCODE_CODE(0x1B)
#define ENC_OPCODE_SYSTEM   ENC_OPCODE_CODE(0x1C)


static uint32_t encPackRFormat(int opcode, int funct3, int funct7, int rd, int rs1, int rs2)
{
   uint32_t res = 0;
   res |= SHIFT_MASK(opcode, 0, 7);
   res |= SHIFT_MASK(rd, 7, 12);
   res |= SHIFT_MASK(funct3, 12, 15);
   res |= SHIFT_MASK(rs1, 15, 20);
   res |= SHIFT_MASK(rs2, 20, 25);
   res |= SHIFT_MASK(funct7, 25, 32);
   return res;
}

static uint32_t encPackIFormat(int opcode, int funct3, int rd, int rs1, int32_t imm)
{
   uint32_t res = 0;
   res |= SHIFT_MASK(opcode, 0, 7);
   res |= SHIFT_MASK(rd, 7, 12);
   res |= SHIFT_MASK(funct3, 12, 15);
   res |= SHIFT_MASK(rs1, 15, 20);
   res |= SHIFT_MASK(imm, 20, 32);
   return res;
}

static uint32_t encPackSFormat(int opcode, int funct3, int rs1, int rs2, int32_t imm)
{
   uint32_t res = 0;
   res |= SHIFT_MASK(opcode, 0, 7);
   res |= SHIFT_MASK(imm, 7, 12);
   res |= SHIFT_MASK(funct3, 12, 15);
   res |= SHIFT_MASK(rs1, 15, 20);
   res |= SHIFT_MASK(rs2, 20, 25);
   res |= SHIFT_MASK(imm >> 5, 25, 32);
   return res;
}

static uint32_t encPackBFormat(int opcode, int funct3, int rs1, int rs2, int32_t imm)
{
   uint32_t res = 0;
   res |= SHIFT_MASK(opcode, 0, 7);
   res |= SHIFT_MASK(imm >> 11, 7, 8);
   res |= SHIFT_MASK(imm >> 1, 8, 12);
   res |= SHIFT_MASK(funct3, 12, 15);
   res |= SHIFT_MASK(rs1, 15, 20);
   res |= SHIFT_MASK(rs2, 20, 25);
   res |= SHIFT_MASK(imm >> 5, 25, 31);
   res |= SHIFT_MASK(imm >> 12, 31, 32);
   return res;
}

static uint32_t encPackUFormat(int opcode, int rd, int32_t imm)
{
   uint32_t res = 0;
   res |= SHIFT_MASK(opcode, 0, 7);
   res |= SHIFT_MASK(rd, 7, 12);
   res |= SHIFT_MASK(imm, 12, 32);
   return res;
}

static uint32_t encPackJFormat(int opcode, int rd, int32_t imm)
{
   uint32_t res = 0;
   res |= SHIFT_MASK(opcode, 0, 7);
   res |= SHIFT_MASK(rd, 7, 12);
   res |= SHIFT_MASK(imm, 12, 20);
   res |= SHIFT_MASK(imm >> 11, 20, 21);
   res |= SHIFT_MASK(imm >> 12, 21, 31);
   res |= SHIFT_MASK(imm >> 20, 31, 32);
   return res;
}


size_t encGetInstrLength(t_instruction instr)
{
   return 4;
}


typedef struct t_encInstrData {
   t_instrOpcode instID;
   char type;
   int opcode;
   int funct3;
   int funct7;  /* also used for immediates */
} t_encInstrData;

static uint32_t encPhysicalInstruction(t_instruction instr)
{
   static const t_encInstrData opInstData[] = {
      { INSTR_OPC_ADD,    'R', ENC_OPCODE_OP,     0, 0x00      },
      { INSTR_OPC_SUB,    'R', ENC_OPCODE_OP,     0, 0x20      },
      { INSTR_OPC_SLL,    'R', ENC_OPCODE_OP,     1, 0x00      },
      { INSTR_OPC_SLT,    'R', ENC_OPCODE_OP,     2, 0x00      },
      { INSTR_OPC_SLTU,   'R', ENC_OPCODE_OP,     3, 0x00      },
      { INSTR_OPC_XOR,    'R', ENC_OPCODE_OP,     4, 0x00      },
      { INSTR_OPC_SRL,    'R', ENC_OPCODE_OP,     5, 0x00      },
      { INSTR_OPC_SRA,    'R', ENC_OPCODE_OP,     5, 0x20      },
      { INSTR_OPC_OR,     'R', ENC_OPCODE_OP,     6, 0x00      },
      { INSTR_OPC_AND,    'R', ENC_OPCODE_OP,     7, 0x00      },
      { INSTR_OPC_ADDI,   'I', ENC_OPCODE_OPIMM,  0, 0x00 << 5 },
      { INSTR_OPC_SLLI,   'I', ENC_OPCODE_OPIMM,  1, 0x00 << 5 },
      { INSTR_OPC_SLTI,   'I', ENC_OPCODE_OPIMM,  2, 0x00 << 5 },
      { INSTR_OPC_SLTIU,  'I', ENC_OPCODE_OPIMM,  3, 0x00 << 5 },
      { INSTR_OPC_XORI,   'I', ENC_OPCODE_OPIMM,  4, 0x00 << 5 },
      { INSTR_OPC_SRLI,   'I', ENC_OPCODE_OPIMM,  5, 0x00 << 5 },
      { INSTR_OPC_SRAI,   'I', ENC_OPCODE_OPIMM,  5, 0x20 << 5 },
      { INSTR_OPC_ORI,    'I', ENC_OPCODE_OPIMM,  6, 0x00 << 5 },
      { INSTR_OPC_ANDI,   'I', ENC_OPCODE_OPIMM,  7, 0x00 << 5 },
      { INSTR_OPC_LB,     'I', ENC_OPCODE_LOAD,   0, 0x00 << 5 },
      { INSTR_OPC_LH,     'I', ENC_OPCODE_LOAD,   1, 0x00 << 5 },
      { INSTR_OPC_LW,     'I', ENC_OPCODE_LOAD,   2, 0x00 << 5 },
      { INSTR_OPC_LBU,    'I', ENC_OPCODE_LOAD,   4, 0x00 << 5 },
      { INSTR_OPC_LHU,    'I', ENC_OPCODE_LOAD,   5, 0x00 << 5 },
      { INSTR_OPC_SB,     'S', ENC_OPCODE_STORE,  0, 0x00 << 5 },
      { INSTR_OPC_SH,     'S', ENC_OPCODE_STORE,  1, 0x00 << 5 },
      { INSTR_OPC_SW,     'S', ENC_OPCODE_STORE,  2, 0x00 << 5 },
      { INSTR_OPC_ECALL,  'I', ENC_OPCODE_SYSTEM, 0, 0         },
      { INSTR_OPC_EBREAK, 'I', ENC_OPCODE_SYSTEM, 0, 1         },
      { -1 }
   };
   const t_encInstrData *info;
   uint32_t res;

   for (info = opInstData; info->instID != -1; info++) {
      if (info->instID == instr.opcode)
         break;
   }
   assert(info->instID != -1);

   switch (info->type) {
      case 'R':
         res = encPackRFormat(info->opcode, info->funct3, info->funct7, instr.dest, instr.src1, instr.src2);
         break;
      case 'I':
         res = encPackIFormat(info->opcode, info->funct3, instr.dest, instr.src1, instr.immediate | info->funct7);
         break;
      case 'S':
         res = encPackSFormat(info->opcode, info->funct3, instr.src1, instr.src2, instr.immediate | info->funct7);
         break;
      default:
         assert("invalid instruction encoding type");
   }
   return res;
}


int encodeInstruction(t_instruction instr, t_data *res)
{
   int i, mInstSz = 0;
   t_instruction mInstBuf[3] = { 0 };
   uint32_t buf;

   /* resolve pseudo-instructions */
   switch (instr.opcode) {
      case INSTR_OPC_NOP:
         mInstBuf[mInstSz].opcode = INSTR_OPC_ADDI;
         mInstBuf[mInstSz].dest = 0;
         mInstBuf[mInstSz].src1 = 0;
         mInstBuf[mInstSz].immediate = 0;
         mInstSz++;
         break;
      default:
         mInstBuf[mInstSz++] = instr;
   }

   res->initialized = 1;
   res->dataSize = 0;
   for (i = 0; i < mInstSz; i++) {
      buf = encPhysicalInstruction(mInstBuf[i]);
      res->data[res->dataSize++] = buf & 0xFF;
      res->data[res->dataSize++] = (buf >> 8) & 0xFF;
      res->data[res->dataSize++] = (buf >> 16) & 0xFF;
      res->data[res->dataSize++] = (buf >> 24) & 0xFF;
   }
   return 1;
}

