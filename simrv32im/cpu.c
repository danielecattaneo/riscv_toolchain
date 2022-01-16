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
   if (reg != CPU_REG_X0)
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


#define SRA(x, amt) (((x)>>(amt))|((amt)>0?(((1<<(amt))-1)<<(32-(amt))):0))

#define BITS(x, a, b)      (((x) >> (a)) & (((uint32_t)1 << ((b) - (a))) - 1))

#define CPU_INST_OPCODE(x)       BITS(x,  0,  7)
#define CPU_INST_RD(x)           BITS(x,  7, 12)
#define CPU_INST_FUNCT3(x)       BITS(x, 12, 15)
#define CPU_INST_RS1(x)          BITS(x, 15, 20)
#define CPU_INST_RS2(x)          BITS(x, 20, 25)
#define CPU_INST_FUNCT7(x)       BITS(x, 25, 32)
#define CPU_INST_IMM12(x)        BITS(x, 20, 32)
#define CPU_INST_IMM12_SEXT(x)   (CPU_INST_IMM12(x) | (CPU_INST_IMM12(x) & \
                                    (1<<12)) ? (((uint32_t)-1)<<12) : 0)

#define CPU_INST_OPCODE_CODE(x) (((x) << 2) | 3)

t_cpuStatus cpuExecute04OpcInstr(uint32_t instr);
t_cpuStatus cpuExecute0COpcInstr(uint32_t instr);

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
   printf("%08x\n", nextInst);
   
   switch (CPU_INST_OPCODE(nextInst)) {
      case CPU_INST_OPCODE_CODE(0x04):
         lastStatus = cpuExecute04OpcInstr(nextInst);
         break;
      case CPU_INST_OPCODE_CODE(0x0C):
         lastStatus = cpuExecute0COpcInstr(nextInst);
         break;
      default:
         lastStatus = CPU_STATUS_ILL_INST_FAULT;
   }
   return lastStatus;
}

t_cpuStatus cpuExecute04OpcInstr(uint32_t instr)
{
   int rd = CPU_INST_RD(instr);
   int rs1 = CPU_INST_RS1(instr);
   if (rd == CPU_REG_X0)
      return CPU_STATUS_ILL_INST_FAULT;

   switch (CPU_INST_FUNCT3(instr)) {
      case 0: /* ADDI */
         cpuRegs[rd] = cpuRegs[rs1] + CPU_INST_IMM12_SEXT(instr);
         break;
      case 1: /* SLLI */
         if (CPU_INST_FUNCT7(instr) == 0x00)
            cpuRegs[rd] = cpuRegs[rs1] << (CPU_INST_IMM12(instr) & 0x1F);
         else
            return CPU_STATUS_ILL_INST_FAULT;
         break;
      case 2: /* SLTI */
         cpuRegs[rd] = ((int32_t)cpuRegs[rs1]) < CPU_INST_IMM12_SEXT(instr);
         break;
      case 3: /* SLTIU */
         cpuRegs[rd] = cpuRegs[rs1] < CPU_INST_IMM12(instr);
         break;
      case 4: /* XORI */
         cpuRegs[rd] = cpuRegs[rs1] ^ CPU_INST_IMM12_SEXT(instr);
         break;
      case 5: /* SRLI / SRAI */
         if (CPU_INST_FUNCT7(instr) == 0x00)
            cpuRegs[rd] = cpuRegs[rs1] >> (CPU_INST_IMM12(instr) & 0x1F);
         else if (CPU_INST_FUNCT7(instr) == 0x20)
            cpuRegs[rd] = SRA(cpuRegs[rs1], (CPU_INST_IMM12(instr) & 0x1F));
         else
            return CPU_STATUS_ILL_INST_FAULT;
         break;
      case 6: /* ORI */
         cpuRegs[rd] = cpuRegs[rs1] | CPU_INST_IMM12_SEXT(instr);
         break;
      case 7: /* ANDI */
         cpuRegs[rd] = cpuRegs[rs1] & CPU_INST_IMM12_SEXT(instr);
         break;
   }

   cpuPC += 4;
   return CPU_STATUS_OK;
}

t_cpuStatus cpuExecute0COpcInstr(uint32_t instr)
{
   int rd = CPU_INST_RD(instr);
   int rs1 = CPU_INST_RS1(instr);
   int rs2 = CPU_INST_RS2(instr);
   if (rd == CPU_REG_X0)
      return CPU_STATUS_ILL_INST_FAULT;

   switch (CPU_INST_FUNCT3(instr)) {
      case 0: /* ADD / SUB */
         if (CPU_INST_FUNCT7(instr) == 0x00)
            cpuRegs[rd] = cpuRegs[rs1] + cpuRegs[rs2];
         else if (CPU_INST_FUNCT7(instr) == 0x20)
            cpuRegs[rd] = cpuRegs[rs1] - cpuRegs[rs2];
         else
            return CPU_STATUS_ILL_INST_FAULT;
         break;
      case 1: /* SLL */
         if (CPU_INST_FUNCT7(instr) == 0x00)
            cpuRegs[rd] = cpuRegs[rs1] << (cpuRegs[rs2] & 0x1F);
         else
            return CPU_STATUS_ILL_INST_FAULT;
         break;
      case 2: /* SLT */
         if (CPU_INST_FUNCT7(instr) == 0x00)
            cpuRegs[rd] = ((int32_t)cpuRegs[rs1]) < ((int32_t)cpuRegs[rs2]);
         else
            return CPU_STATUS_ILL_INST_FAULT;
         break;
      case 3: /* SLTU */
         if (CPU_INST_FUNCT7(instr) == 0x00)
            cpuRegs[rd] = cpuRegs[rs1] < cpuRegs[rs2];
         else
            return CPU_STATUS_ILL_INST_FAULT;
         break;
      case 4: /* XOR */
         if (CPU_INST_FUNCT7(instr) == 0x00)
            cpuRegs[rd] = cpuRegs[rs1] ^ cpuRegs[rs2];
         else
            return CPU_STATUS_ILL_INST_FAULT;
         break;
      case 5: /* SRL / SRA */
         if (CPU_INST_FUNCT7(instr) == 0x00)
            cpuRegs[rd] = cpuRegs[rs1] >> (cpuRegs[rs2] & 0x1F);
         else if (CPU_INST_FUNCT7(instr) == 0x20)
            cpuRegs[rd] = SRA(cpuRegs[rs1], (cpuRegs[rs2] & 0x1F));
         else
            return CPU_STATUS_ILL_INST_FAULT;
         break;
      case 6: /* OR */
         if (CPU_INST_FUNCT7(instr) == 0x00)
            cpuRegs[rd] = cpuRegs[rs1] | cpuRegs[rs2];
         else
            return CPU_STATUS_ILL_INST_FAULT;
         break;
      case 7: /* AND */
         if (CPU_INST_FUNCT7(instr) == 0x00)
            cpuRegs[rd] = cpuRegs[rs1] & cpuRegs[rs2];
         else
            return CPU_STATUS_ILL_INST_FAULT;
         break;
   }

   cpuPC += 4;
   return CPU_STATUS_OK;
}


