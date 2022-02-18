#include <stdio.h>
#include "cpu.h"
#include "memory.h"


#define CPU_N_REGS 32

t_cpuRegValue cpuRegs[CPU_N_REGS];
t_cpuRegValue cpuPC;
t_cpuStatus lastStatus;


t_cpuRegValue cpuGetRegister(t_cpuRegID reg)
{
   if (reg == CPU_REG_X0)
      return 0;
   if (reg == CPU_REG_PC)
      return cpuPC;
   return cpuRegs[reg];
}


void cpuSetRegister(t_cpuRegID reg, t_cpuRegValue value)
{
   if (reg == CPU_REG_PC)
      cpuPC = value;
   if (reg != CPU_REG_ZERO)
      cpuRegs[reg] = value;
}


void cpuReset(t_cpuRegValue pcValue)
{
   int i;

   lastStatus = CPU_STATUS_OK;
   cpuPC = pcValue;
   for (i = 0; i < CPU_N_REGS; i++) {
      cpuRegs[i] = 0;
   }
}


t_cpuStatus cpuClearLastFault(void)
{
   if (lastStatus == CPU_STATUS_ILL_INST_FAULT || 
         lastStatus == CPU_STATUS_EBREAK_TRAP || 
         lastStatus == CPU_STATUS_ECALL_TRAP)
      cpuPC += 4;
   lastStatus = CPU_STATUS_OK;
   return lastStatus;
}


t_cpuStatus cpuExecuteLOAD(uint32_t instr);
t_cpuStatus cpuExecuteOPIMM(uint32_t instr);
t_cpuStatus cpuExecuteAUIPC(uint32_t instr);
t_cpuStatus cpuExecuteSTORE(uint32_t instr);
t_cpuStatus cpuExecuteOP(uint32_t instr);
t_cpuStatus cpuExecuteLUI(uint32_t instr);
t_cpuStatus cpuExecuteBRANCH(uint32_t instr);
t_cpuStatus cpuExecuteJALR(uint32_t instr);
t_cpuStatus cpuExecuteJAL(uint32_t instr);
t_cpuStatus cpuExecuteSYSTEM(uint32_t instr);

t_cpuStatus cpuTick(void)
{
   uint32_t nextInst;
   t_memError fetchErr;

   if (lastStatus != CPU_STATUS_OK)
      return lastStatus;
   
   fetchErr = memRead32(cpuPC, &nextInst);
   if (fetchErr != MEM_NO_ERROR) {
      lastStatus = CPU_STATUS_MEMORY_FAULT;
      return lastStatus;
   }
   
   switch (ISA_INST_OPCODE(nextInst)) {
      case ISA_INST_OPCODE_LOAD:
         lastStatus = cpuExecuteLOAD(nextInst);
         break;
      case ISA_INST_OPCODE_OPIMM:
         lastStatus = cpuExecuteOPIMM(nextInst);
         break;
      case ISA_INST_OPCODE_AUIPC:
         lastStatus = cpuExecuteAUIPC(nextInst);
         break;
      case ISA_INST_OPCODE_STORE:
         lastStatus = cpuExecuteSTORE(nextInst);
         break;
      case ISA_INST_OPCODE_OP:
         lastStatus = cpuExecuteOP(nextInst);
         break;
      case ISA_INST_OPCODE_LUI:
         lastStatus = cpuExecuteLUI(nextInst);
         break;
      case ISA_INST_OPCODE_BRANCH:
         lastStatus = cpuExecuteBRANCH(nextInst);
         break;
      case ISA_INST_OPCODE_JALR:
         lastStatus = cpuExecuteJALR(nextInst);
         break;
      case ISA_INST_OPCODE_JAL:
         lastStatus = cpuExecuteJAL(nextInst);
         break;
      case ISA_INST_OPCODE_SYSTEM:
         lastStatus = cpuExecuteSYSTEM(nextInst);
         break;
      default:
         lastStatus = CPU_STATUS_ILL_INST_FAULT;
   }
   cpuRegs[CPU_REG_ZERO] = 0;
   return lastStatus;
}

t_cpuStatus cpuExecuteLOAD(uint32_t instr)
{
   t_memAddress addr;
   uint8_t tmp8;
   uint16_t tmp16;
   uint32_t tmp32;
   t_memError memStatus;
   int rd = ISA_INST_RD(instr);
   int rs1 = ISA_INST_RS1(instr);
   addr = cpuRegs[rs1] + ISA_INST_I_IMM12_SEXT(instr);

   switch (ISA_INST_FUNCT3(instr)) {
      case 0: /* LB */
         memStatus = memRead8(addr, &tmp8);
         if (memStatus != MEM_NO_ERROR)
            return CPU_STATUS_MEMORY_FAULT;
         cpuRegs[rd] = (int32_t)((int8_t)tmp8);
         break;
      case 1: /* LH */
         memStatus = memRead16(addr, &tmp16);
         if (memStatus != MEM_NO_ERROR)
            return CPU_STATUS_MEMORY_FAULT;
         cpuRegs[rd] = (int32_t)((int16_t)tmp16);
         break;
      case 2: /* LW */
         memStatus = memRead32(addr, &tmp32);
         if (memStatus != MEM_NO_ERROR)
            return CPU_STATUS_MEMORY_FAULT;
         cpuRegs[rd] = tmp32;
         break;
      case 4: /* LBU */
         memStatus = memRead8(addr, &tmp8);
         if (memStatus != MEM_NO_ERROR)
            return CPU_STATUS_MEMORY_FAULT;
         cpuRegs[rd] = (uint32_t)tmp8;
         break;
      case 5: /* LHU */
         memStatus = memRead16(addr, &tmp16);
         if (memStatus != MEM_NO_ERROR)
            return CPU_STATUS_MEMORY_FAULT;
         cpuRegs[rd] = (uint32_t)tmp16;
         break;
      default:
         return CPU_STATUS_ILL_INST_FAULT;
   }

   cpuPC += 4;
   return CPU_STATUS_OK;
}

t_cpuStatus cpuExecuteOPIMM(uint32_t instr)
{
   int rd = ISA_INST_RD(instr);
   int rs1 = ISA_INST_RS1(instr);

   switch (ISA_INST_FUNCT3(instr)) {
      case 0: /* ADDI */
         cpuRegs[rd] = cpuRegs[rs1] + ISA_INST_I_IMM12_SEXT(instr);
         break;
      case 1: /* SLLI */
         if (ISA_INST_FUNCT7(instr) == 0x00)
            cpuRegs[rd] = cpuRegs[rs1] << (ISA_INST_I_IMM12(instr) & 0x1F);
         else
            return CPU_STATUS_ILL_INST_FAULT;
         break;
      case 2: /* SLTI */
         cpuRegs[rd] = ((int32_t)cpuRegs[rs1]) < ((int32_t)ISA_INST_I_IMM12_SEXT(instr));
         break;
      case 3: /* SLTIU */
         cpuRegs[rd] = cpuRegs[rs1] < ISA_INST_I_IMM12(instr);
         break;
      case 4: /* XORI */
         cpuRegs[rd] = cpuRegs[rs1] ^ ISA_INST_I_IMM12_SEXT(instr);
         break;
      case 5: /* SRLI / SRAI */
         if (ISA_INST_FUNCT7(instr) == 0x00)
            cpuRegs[rd] = cpuRegs[rs1] >> (ISA_INST_I_IMM12(instr) & 0x1F);
         else if (ISA_INST_FUNCT7(instr) == 0x20)
            cpuRegs[rd] = SRA(cpuRegs[rs1], (ISA_INST_I_IMM12(instr) & 0x1F));
         else
            return CPU_STATUS_ILL_INST_FAULT;
         break;
      case 6: /* ORI */
         cpuRegs[rd] = cpuRegs[rs1] | ISA_INST_I_IMM12_SEXT(instr);
         break;
      case 7: /* ANDI */
         cpuRegs[rd] = cpuRegs[rs1] & ISA_INST_I_IMM12_SEXT(instr);
         break;
   }

   cpuPC += 4;
   return CPU_STATUS_OK;
}

t_cpuStatus cpuExecuteAUIPC(uint32_t instr)
{
   int rd = ISA_INST_RD(instr);
   cpuRegs[rd] = cpuPC + (ISA_INST_U_IMM20(instr) << 12);
   cpuPC += 4;
   return CPU_STATUS_OK;
}

t_cpuStatus cpuExecuteSTORE(uint32_t instr)
{
   t_memAddress addr;
   t_memError memStatus;
   int rs1 = ISA_INST_RS1(instr);
   int rs2 = ISA_INST_RS2(instr);
   addr = cpuRegs[rs1] + ISA_INST_S_IMM12_SEXT(instr);

   switch (ISA_INST_FUNCT3(instr)) {
      case 0: /* SB */
         memStatus = memWrite8(addr, cpuRegs[rs2] & 0xFF);
         if (memStatus != MEM_NO_ERROR)
            return CPU_STATUS_MEMORY_FAULT;
         break;
      case 1: /* SH */
         memStatus = memWrite16(addr, cpuRegs[rs2] & 0xFFFF);
         if (memStatus != MEM_NO_ERROR)
            return CPU_STATUS_MEMORY_FAULT;
         break;
      case 2: /* SW */
         memStatus = memWrite32(addr, cpuRegs[rs2]);
         if (memStatus != MEM_NO_ERROR)
            return CPU_STATUS_MEMORY_FAULT;
         break;
      default:
         return CPU_STATUS_ILL_INST_FAULT;
   }

   cpuPC += 4;
   return CPU_STATUS_OK;
}

t_cpuStatus cpuExecuteOP(uint32_t instr)
{
   int rd = ISA_INST_RD(instr);
   int rs1 = ISA_INST_RS1(instr);
   int rs2 = ISA_INST_RS2(instr);

   if (ISA_INST_FUNCT7(instr) == 0x00) {
      switch (ISA_INST_FUNCT3(instr)) {
         case 0: /* ADD */
            cpuRegs[rd] = cpuRegs[rs1] + cpuRegs[rs2];
            break;
         case 1: /* SLL */
            cpuRegs[rd] = cpuRegs[rs1] << (cpuRegs[rs2] & 0x1F);
            break;
         case 2: /* SLT */
            cpuRegs[rd] = ((int32_t)cpuRegs[rs1]) < ((int32_t)cpuRegs[rs2]);
            break;
         case 3: /* SLTU */
            cpuRegs[rd] = cpuRegs[rs1] < cpuRegs[rs2];
            break;
         case 4: /* XOR */
            cpuRegs[rd] = cpuRegs[rs1] ^ cpuRegs[rs2];
            break;
         case 5: /* SRL */
            cpuRegs[rd] = cpuRegs[rs1] >> (cpuRegs[rs2] & 0x1F);
            break;
         case 6: /* OR */
            cpuRegs[rd] = cpuRegs[rs1] | cpuRegs[rs2];
            break;
         case 7: /* AND */
            cpuRegs[rd] = cpuRegs[rs1] & cpuRegs[rs2];
            break;
      }
   } else if (ISA_INST_FUNCT7(instr) == 0x20) {
      switch (ISA_INST_FUNCT3(instr)) {
         case 0: /* SUB */
            cpuRegs[rd] = cpuRegs[rs1] - cpuRegs[rs2];
            break;
         case 1:
         case 2:
         case 3:
         case 4:
            return CPU_STATUS_ILL_INST_FAULT;
         case 5: /* SRA */
            cpuRegs[rd] = SRA(cpuRegs[rs1], (cpuRegs[rs2] & 0x1F));
            break;
         case 6:
         case 7:
            return CPU_STATUS_ILL_INST_FAULT;
      }
   } else if (ISA_INST_FUNCT7(instr) == 0x01) {
      switch (ISA_INST_FUNCT3(instr)) {
         case 0: /* MUL */
            cpuRegs[rd] = cpuRegs[rs1] * cpuRegs[rs2];
            break;
         case 1: /* MULH */
            cpuRegs[rd] = (uint32_t)(((int64_t)((int32_t)cpuRegs[rs1]) * 
                                      (int64_t)((int32_t)cpuRegs[rs2])) >> 32);
            break;
         case 2: /* MULHSU */
            cpuRegs[rd] = (uint32_t)(((int64_t)((int32_t)cpuRegs[rs1]) * 
                                      (int64_t)(cpuRegs[rs2])) >> 32);
            break;
         case 3: /* MULHU */
            cpuRegs[rd] = (uint32_t)(((uint64_t)(cpuRegs[rs1]) * 
                                      (uint64_t)(cpuRegs[rs2])) >> 32);
            break;
         case 4: /* DIV */
            if (cpuRegs[rs2] == 0)
               cpuRegs[rd] = 0xFFFFFFFF;
            else if (cpuRegs[rs1] == 0x80000000 && cpuRegs[rs2] == 0xFFFFFFFF)
               cpuRegs[rd] = 0x80000000;
            else
               cpuRegs[rd] = (int32_t)cpuRegs[rs1] / (int32_t)cpuRegs[rs2];
            break;
         case 5: /* DIVU */
            if (cpuRegs[rs2] == 0)
               cpuRegs[rd] = 0xFFFFFFFF;
            else
               cpuRegs[rd] = cpuRegs[rs1] / cpuRegs[rs2];
            break;
         case 6: /* REM */
            if (cpuRegs[rs2] == 0)
               cpuRegs[rd] = cpuRegs[rs1];
            else if (cpuRegs[rs1] == 0x80000000 && cpuRegs[rs2] == 0xFFFFFFFF)
               cpuRegs[rd] = 0;
            else
               cpuRegs[rd] = (int32_t)cpuRegs[rs1] % (int32_t)cpuRegs[rs2];
            break;
         case 7: /* REMU */
            if (cpuRegs[rs2] == 0)
               cpuRegs[rd] = cpuRegs[rs1];
            else
               cpuRegs[rd] = cpuRegs[rs1] % cpuRegs[rs2];
            break;
      }
   } else {
      return CPU_STATUS_ILL_INST_FAULT;
   }

   cpuPC += 4;
   return CPU_STATUS_OK;
}

t_cpuStatus cpuExecuteLUI(uint32_t instr)
{
   int rd = ISA_INST_RD(instr);
   cpuRegs[rd] = ISA_INST_U_IMM20(instr) << 12;
   cpuPC += 4;
   return CPU_STATUS_OK;
}

t_cpuStatus cpuExecuteBRANCH(uint32_t instr)
{
   int rs1 = ISA_INST_RS1(instr);
   int rs2 = ISA_INST_RS2(instr);
   int32_t offs = ISA_INST_B_IMM13_SEXT(instr);
   int taken = 0;

   switch (ISA_INST_FUNCT3(instr)) {
      case 0: /* BEQ */
         taken = cpuRegs[rs1] == cpuRegs[rs2];
         break;
      case 1: /* BNE */
         taken = cpuRegs[rs1] != cpuRegs[rs2];
         break;
      case 4: /* BLT */
         taken = (int32_t)cpuRegs[rs1] < (int32_t)cpuRegs[rs2];
         break;
      case 5: /* BGE */
         taken = (int32_t)cpuRegs[rs1] >= (int32_t)cpuRegs[rs2];
         break;
      case 6: /* BLTU */
         taken = cpuRegs[rs1] < cpuRegs[rs2];
         break;
      case 7: /* BGEU */
         taken = cpuRegs[rs1] >= cpuRegs[rs2];
         break;
      default:
         return CPU_STATUS_ILL_INST_FAULT;
   }

   cpuPC += taken ? offs : 4;
   return CPU_STATUS_OK;
}

t_cpuStatus cpuExecuteJALR(uint32_t instr)
{
   int32_t offs = ISA_INST_I_IMM12_SEXT(instr);
   int rd = ISA_INST_RD(instr);
   int rs1 = ISA_INST_RS1(instr);
   if (ISA_INST_FUNCT3(instr) != 0)
      return CPU_STATUS_ILL_INST_FAULT;
   cpuRegs[rd] = cpuPC + 4;
   cpuPC = (cpuRegs[rs1] + offs) & ~1; /* clear bit zero as suggested by the spec */
   return CPU_STATUS_OK;
}

t_cpuStatus cpuExecuteJAL(uint32_t instr)
{
   int32_t offs = ISA_INST_J_IMM21_SEXT(instr);
   int rd = ISA_INST_RD(instr);
   cpuRegs[rd] = cpuPC + 4;
   cpuPC += offs;
   return CPU_STATUS_OK;
}

t_cpuStatus cpuExecuteSYSTEM(uint32_t instr)
{
   if (ISA_INST_FUNCT3(instr) != 0)
      return CPU_STATUS_ILL_INST_FAULT;
   if (ISA_INST_I_IMM12(instr) == 0)
      return CPU_STATUS_ECALL_TRAP;
   if (ISA_INST_I_IMM12(instr) == 1)
      return CPU_STATUS_EBREAK_TRAP;
   return CPU_STATUS_ILL_INST_FAULT;
}
