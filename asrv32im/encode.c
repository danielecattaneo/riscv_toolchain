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
   res |= SHIFT_MASK(imm >> 12, 12, 20);
   res |= SHIFT_MASK(imm >> 11, 20, 21);
   res |= SHIFT_MASK(imm >> 1, 21, 31);
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

int encPhysicalInstruction(t_instruction instr, uint32_t pc, t_data *res)
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
      { INSTR_OPC_LUI,    'U', ENC_OPCODE_LUI,    0, 0         },
      { INSTR_OPC_AUIPC,  'U', ENC_OPCODE_AUIPC,  0, 0         },
      { INSTR_OPC_SB,     'S', ENC_OPCODE_STORE,  0, 0x00 << 5 },
      { INSTR_OPC_SH,     'S', ENC_OPCODE_STORE,  1, 0x00 << 5 },
      { INSTR_OPC_SW,     'S', ENC_OPCODE_STORE,  2, 0x00 << 5 },
      { INSTR_OPC_JAL,    'J', ENC_OPCODE_JAL,    0, 0         },
      { INSTR_OPC_JALR,   'I', ENC_OPCODE_JALR,   0, 0         },
      { INSTR_OPC_ECALL,  'I', ENC_OPCODE_SYSTEM, 0, 0         },
      { INSTR_OPC_EBREAK, 'I', ENC_OPCODE_SYSTEM, 0, 1         },
      { -1 }
   };
   const t_encInstrData *info;
   uint32_t buf, imm;

   for (info = opInstData; info->instID != -1; info++) {
      if (info->instID == instr.opcode)
         break;
   }
   assert(info->instID != -1);

   switch (info->type) {
      case 'R':
         buf = encPackRFormat(info->opcode, info->funct3, info->funct7, instr.dest, instr.src1, instr.src2);
         break;
      case 'I':
         buf = encPackIFormat(info->opcode, info->funct3, instr.dest, instr.src1, instr.constant | info->funct7);
         break;
      case 'S':
         buf = encPackSFormat(info->opcode, info->funct3, instr.src1, instr.src2, instr.constant | info->funct7);
         break;
      case 'U':
         buf = encPackUFormat(info->opcode, instr.dest, instr.constant);
         break;
      case 'J':
         buf = encPackJFormat(info->opcode, instr.dest, instr.constant);
         break;
      default:
         assert("invalid instruction encoding type");
   }

   res->initialized = 1;
   res->dataSize = 4;
   res->data[0] = buf & 0xFF;
   res->data[1] = (buf >> 8) & 0xFF;
   res->data[2] = (buf >> 16) & 0xFF;
   res->data[3] = (buf >> 24) & 0xFF;
   return 1;
}


int encExpandPseudoInstruction(t_instruction instr, t_instruction mInstBuf[MAX_EXP_FACTOR])
{
   int mInstSz = 0;

   switch (instr.opcode) {
      case INSTR_OPC_NOP:
         mInstBuf[mInstSz].opcode = INSTR_OPC_ADDI;
         mInstBuf[mInstSz].dest = 0;
         mInstBuf[mInstSz].src1 = 0;
         mInstBuf[mInstSz].immMode = INSTR_IMM_CONST;
         mInstBuf[mInstSz].constant = 0;
         mInstSz++;
         break;
      default:
         mInstBuf[mInstSz++] = instr;
   }

   return mInstSz;
}


int encResolveImmediates(t_instruction *instr, uint32_t pc)
{
   t_objLabel *otherInstrLbl, *actualLbl;
   t_objSecItem *otherInstr;
   int32_t imm;
   uint32_t tgt, otherPc;

   if (instr->immMode == INSTR_IMM_CONST)
      return 1;
   
   if (!objLabelGetPointedItem(instr->label)) {
      fprintf(stderr, "label \"%s\" used but not defined!\n", objLabelGetName(instr->label));
      return 0;
   }

   switch (instr->immMode) {
      case INSTR_IMM_LBL:
         imm = objLabelGetPointer(instr->label) - pc;
         if (instr->opcode == INSTR_OPC_JAL && (imm < -0x100000 || imm > 0xFFFFF)) {
            fprintf(stderr, "JAL to label \"%s\" too far!\n", objLabelGetName(instr->label));
            return 0;
         }
         if (instr->opcode == INSTR_OPC_JALR && (imm < -0x800 || imm > 0x7FF)) {
            fprintf(stderr, "JALR to label \"%s\" too far!\n", objLabelGetName(instr->label));
            return 0;
         }
         break;

      case INSTR_IMM_LBL_LO12:
         imm = objLabelGetPointer(instr->label);
         imm &= 0xFFF;
         break;

      case INSTR_IMM_LBL_HI20:
         imm = objLabelGetPointer(instr->label);
         imm = ((imm >> 12) + (imm & 0x800 ? 1 : 0)) & 0xFFFFF;
         break;

      case INSTR_IMM_LBL_PCREL_LO12:
         /* %pcrel_lo addressing needs to compensate for the fact that the instr.
          * that loads the low part has a different PC than the one that loads the
          * high part, so the argument does not point to the symbol address to load
          * but to the instruction that loads the high part of the address...
          * This can go wrong in too many ways (i.e. more than zero ways) */
         otherInstrLbl = instr->label;
         otherInstr = objLabelGetPointedItem(otherInstrLbl);
         if (!otherInstr) {
            fprintf(stderr, "label \"%s\" used but not defined\n", objLabelGetName(otherInstrLbl));
            return 0;
         } else if (otherInstr->class != OBJ_SEC_ITM_CLASS_INSTR) {
            fprintf(stderr, "argument to %%pcrel_lo must be a label to an instruction\n");
            return 0;
         } else if (otherInstr->body.instr.immMode != INSTR_IMM_LBL_PCREL_HI20) {
            fprintf(stderr, "argument to %%pcrel_lo must be a label to an instruction using %%pcrel_hi\n");
            return 0;
         }
         actualLbl = otherInstr->body.instr.label;
         if (!objLabelGetPointedItem(actualLbl)) {
            fprintf(stderr, "label \"%s\" used but not defined!\n", objLabelGetName(actualLbl));
            return 0;
         }
         otherPc = otherInstr->address;
         imm = (objLabelGetPointer(actualLbl) - pc) & 0xFFF;
         break;

      case INSTR_IMM_LBL_PCREL_HI20:
         imm = objLabelGetPointer(instr->label) - pc;
         imm = ((imm >> 12) + (imm & 0x800 ? 1 : 0)) & 0xFFFFF;
         break;
   }

   instr->constant = imm;
   return 1;
}

