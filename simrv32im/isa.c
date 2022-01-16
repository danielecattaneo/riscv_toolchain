#include <stdio.h>
#include <inttypes.h>
#include "isa.h"


int isaDisassembleIllegal(uint32_t instr, char *out, int bufsz);
int isaDisassembleOP(uint32_t instr, char *out, int bufsz);
int isaDisassembleOPIMM(uint32_t instr, char *out, int bufsz);
int isaDisassembleLOAD(uint32_t instr, char *out, int bufsz);
int isaDisassembleSTORE(uint32_t instr, char *out, int bufsz);
int isaDisassembleBRANCH(uint32_t instr, char *out, int bufsz);

int isaDisassemble(uint32_t instr, char *out, int bufsz)
{
   int rd, rs1;
   int32_t imm;

   switch (ISA_INST_OPCODE(instr)) {
      case ISA_INST_OPCODE_OP:
         return isaDisassembleOP(instr, out, bufsz);
      case ISA_INST_OPCODE_OPIMM:
         return isaDisassembleOPIMM(instr, out, bufsz);
      case ISA_INST_OPCODE_LOAD:
         return isaDisassembleLOAD(instr, out, bufsz);
      case ISA_INST_OPCODE_STORE:
         return isaDisassembleSTORE(instr, out, bufsz);
      case ISA_INST_OPCODE_BRANCH:
         return isaDisassembleBRANCH(instr, out, bufsz);
      case ISA_INST_OPCODE_JAL:
         rd = ISA_INST_RD(instr);
         imm = ISA_INST_J_IMM21_SEXT(instr);
         return snprintf(out, bufsz, "JAL x%d, *%+"PRId32, rd, imm);
      case ISA_INST_OPCODE_JALR:
         if (ISA_INST_FUNCT3(instr) != 0)
            return isaDisassembleIllegal(instr, out, bufsz);
         rd = ISA_INST_RD(instr);
         rs1 = ISA_INST_RS1(instr);
         imm = ISA_INST_I_IMM12_SEXT(instr);
         return snprintf(out, bufsz, "JALR x%d, x%d, *%+"PRId32, rd, rs1, imm);
      case ISA_INST_OPCODE_LUI:
         rd = ISA_INST_RD(instr);
         imm = ISA_INST_U_IMM20_SEXT(instr);
         return snprintf(out, bufsz, "LUI x%d, 0x%08"PRIx32, rd, imm << 12);
      case ISA_INST_OPCODE_AUIPC:
         rd = ISA_INST_RD(instr);
         imm = ISA_INST_U_IMM20_SEXT(instr);
         return snprintf(out, bufsz, "AUIPC x%d, 0x%08"PRIx32, rd, imm << 12);
   }

   return isaDisassembleIllegal(instr, out, bufsz);
}

int isaDisassembleIllegal(uint32_t instr, char *out, int bufsz)
{
   return snprintf(out, bufsz, "<illegal>");
}

int isaDisassembleOP(uint32_t instr, char *out, int bufsz)
{
   int rd = ISA_INST_RD(instr);
   int rs1 = ISA_INST_RS1(instr);
   int rs2 = ISA_INST_RS2(instr);
   const char *mnems[] = {
         "ADD", "SLL", "SLT", "SLTU", "XOR", "SRL", "OR", "AND"};
   const char *mnem;

   if (ISA_INST_FUNCT7(instr) == 0x20) {
      if (ISA_INST_FUNCT3(instr) == 0)
         mnem = "SUB";
      else if (ISA_INST_FUNCT3(instr) == 5)
         mnem = "SRA";
      else
         return isaDisassembleIllegal(instr, out, bufsz);
   } else if (ISA_INST_FUNCT7(instr) == 0x00) {
      mnem = mnems[ISA_INST_FUNCT3(instr)];
   } else
      return isaDisassembleIllegal(instr, out, bufsz);

   return snprintf(out, bufsz, "%s x%d, x%d, x%d", mnem, rd, rs1, rs2);
}

int isaDisassembleOPIMM(uint32_t instr, char *out, int bufsz)
{
   int rd = ISA_INST_RD(instr);
   int rs1 = ISA_INST_RS1(instr);
   int32_t imm = ISA_INST_I_IMM12_SEXT(instr);
   const char *mnems[] = {
         "ADDI", "SLLI", "SLTI", "SLTIU", "XORI", "SRLI", "ORI", "ANDI"};
   const char *mnem;

   mnem = mnems[ISA_INST_FUNCT3(instr)];

   if (ISA_INST_FUNCT3(instr) == 3) {
      imm &= 0x7FF;
   } else if (ISA_INST_FUNCT3(instr) == 1) {
      if (ISA_INST_FUNCT7(instr) != 0)
         return isaDisassembleIllegal(instr, out, bufsz);
   } else if (ISA_INST_FUNCT3(instr) == 5) {
      if (ISA_INST_FUNCT7(instr) == 0x20) {
         mnem = "SRAI";
         imm &= 0x1F;
      } else if (ISA_INST_FUNCT7(instr) != 0)
         return isaDisassembleIllegal(instr, out, bufsz);
   }

   return snprintf(out, bufsz, "%s x%d, x%d, %"PRId32, mnem, rd, rs1, imm);
}

int isaDisassembleLOAD(uint32_t instr, char *out, int bufsz)
{
   int rd = ISA_INST_RD(instr);
   int rs1 = ISA_INST_RS1(instr);
   int32_t imm = ISA_INST_I_IMM12_SEXT(instr);
   const char *mnems[] = {
         "LB", "LH", "LW", NULL, "LBU", "LHU", NULL, NULL};
   const char *mnem;

   mnem = mnems[ISA_INST_FUNCT3(instr)];
   if (mnem == NULL)
      return isaDisassembleIllegal(instr, out, bufsz);

   return snprintf(out, bufsz, "%s x%d, x%d, %"PRId32, mnem, rd, rs1, imm);
}

int isaDisassembleSTORE(uint32_t instr, char *out, int bufsz)
{
   int rs1 = ISA_INST_RS1(instr);
   int rs2 = ISA_INST_RS2(instr);
   int32_t imm = ISA_INST_S_IMM12_SEXT(instr);
   const char *mnems[] = {
         "SB", "SH", "SW", NULL, NULL, NULL, NULL, NULL};
   const char *mnem;

   mnem = mnems[ISA_INST_FUNCT3(instr)];
   if (mnem == NULL)
      return isaDisassembleIllegal(instr, out, bufsz);

   return snprintf(out, bufsz, "%s x%d, x%d, %"PRId32, mnem, rs1, rs2, imm);
}

int isaDisassembleBRANCH(uint32_t instr, char *out, int bufsz)
{
   int rs1 = ISA_INST_RS1(instr);
   int rs2 = ISA_INST_RS2(instr);
   int32_t imm = ISA_INST_B_IMM13_SEXT(instr);
   const char *mnems[] = {
         "BEQ", "BNE", NULL, NULL, "BLT", "BGE", "BLTU", "BGEU"};
   const char *mnem;

   mnem = mnems[ISA_INST_FUNCT3(instr)];
   if (mnem == NULL)
      return isaDisassembleIllegal(instr, out, bufsz);

   return snprintf(out, bufsz, "%s x%d, x%d, *%+"PRId32, mnem, rs1, rs2, imm);
}

