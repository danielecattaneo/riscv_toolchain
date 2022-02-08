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
   int funct7;
} t_encInstrData;

int encodeInstruction(t_instruction instr, t_data *res)
{
   static const t_encInstrData opInstData[] = {
      { INSTR_OPC_ADD,  'R', ENC_OPCODE_OP,    0, 0x00 },
      { INSTR_OPC_SUB,  'R', ENC_OPCODE_OP,    0, 0x20 },
      { INSTR_OPC_SLL,  'R', ENC_OPCODE_OP,    1, 0x00 },
      { INSTR_OPC_SLT,  'R', ENC_OPCODE_OP,    2, 0x00 },
      { INSTR_OPC_SLTU, 'R', ENC_OPCODE_OP,    3, 0x00 },
      { INSTR_OPC_XOR,  'R', ENC_OPCODE_OP,    4, 0x00 },
      { INSTR_OPC_SRL,  'R', ENC_OPCODE_OP,    5, 0x00 },
      { INSTR_OPC_SRA,  'R', ENC_OPCODE_OP,    5, 0x20 },
      { INSTR_OPC_OR,   'R', ENC_OPCODE_OP,    6, 0x00 },
      { INSTR_OPC_AND,  'R', ENC_OPCODE_OP,    7, 0x00 },
      { INSTR_OPC_ADDI, 'I', ENC_OPCODE_OPIMM, 0, 0x00 },
      { INSTR_OPC_SLLI, 'I', ENC_OPCODE_OPIMM, 1, 0x00 },
      { INSTR_OPC_SLTI, 'I', ENC_OPCODE_OPIMM, 2, 0x00 },
      { INSTR_OPC_SLTIU,'I', ENC_OPCODE_OPIMM, 3, 0x00 },
      { INSTR_OPC_XORI, 'I', ENC_OPCODE_OPIMM, 4, 0x00 },
      { INSTR_OPC_SRLI, 'I', ENC_OPCODE_OPIMM, 5, 0x00 },
      { INSTR_OPC_SRAI, 'I', ENC_OPCODE_OPIMM, 5, 0x20 },
      { INSTR_OPC_ORI,  'I', ENC_OPCODE_OPIMM, 6, 0x00 },
      { INSTR_OPC_ANDI, 'I', ENC_OPCODE_OPIMM, 7, 0x00 },
      { -1 }
   };
   const t_encInstrData *info;
   uint32_t buf;

   for (info = opInstData; info->instID != -1; info++) {
      if (info->instID == instr.opcode)
         break;
   }
   if (info->instID == -1)
      return 0;

   switch (info->type) {
      case 'R':
         buf = encPackRFormat(info->opcode, info->funct3, info->funct7, instr.dest, instr.src1, instr.src2);
         break;
      case 'I':
         buf = encPackIFormat(info->opcode, info->funct3, instr.dest, instr.src1, instr.immediate | info->funct7 << 5);
         break;
      default:
         return 0;
   }

   res->dataSize = 4;
   res->initialized = 1;
   res->data[0] = buf & 0xFF;
   res->data[1] = (buf >> 8) & 0xFF;
   res->data[2] = (buf >> 16) & 0xFF;
   res->data[3] = (buf >> 24) & 0xFF;
   return 1;
}

