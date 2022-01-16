#ifndef CPU_H
#define CPU_H

#include <stdint.h>


typedef uint32_t t_cpuXSize;
typedef t_cpuXSize t_cpuRegValue;

typedef int t_cpuRegID;
enum {
   CPU_REG_X0 = 0,
   CPU_REG_X1,
   CPU_REG_X2,
   CPU_REG_X3,
   CPU_REG_X4,
   CPU_REG_X5,
   CPU_REG_X6,
   CPU_REG_X7,
   CPU_REG_X8,
   CPU_REG_X9,
   CPU_REG_X10,
   CPU_REG_X11,
   CPU_REG_X12,
   CPU_REG_X13,
   CPU_REG_X14,
   CPU_REG_X15,
   CPU_REG_X16,
   CPU_REG_X17,
   CPU_REG_X18,
   CPU_REG_X19,
   CPU_REG_X20,
   CPU_REG_X21,
   CPU_REG_X22,
   CPU_REG_X23,
   CPU_REG_X24,
   CPU_REG_X25,
   CPU_REG_X26,
   CPU_REG_X27,
   CPU_REG_X28,
   CPU_REG_X29,
   CPU_REG_X30,
   CPU_REG_X31,
   CPU_REG_PC
};

typedef int t_cpuStatus;
enum {
   CPU_STATUS_OK = 0,
   CPU_STATUS_MEMORY_FAULT = -1,
   CPU_STATUS_ILL_INST_FAULT = -2
};

t_cpuRegValue cpuGetRegister(t_cpuRegID reg);
void cpuSetRegister(t_cpuRegID reg, t_cpuRegValue value);

void cpuReset(t_cpuRegValue pcValue);
t_cpuStatus cpuTick(void);

#endif
