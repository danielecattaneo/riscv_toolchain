#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include "cpu.h"


#define SV_STACK_PAGE_SIZE 4096

typedef int t_svError;
enum {
   SV_NO_ERROR = 0,
   SV_MEMORY_ERROR = -1
};

typedef int t_svStatus;
enum {
   SV_STATUS_RUNNING = 0,
   SV_STATUS_TERMINATED = 1,
   SV_STATUS_MEMORY_FAULT = CPU_STATUS_MEMORY_FAULT,
   SV_STATUS_ILL_INST_FAULT = CPU_STATUS_ILL_INST_FAULT,
   SV_STATUS_INVALID_SYSCALL = -1000
};


t_svError svInit(void);
t_svStatus svVMTick(void);
t_isaInt svGetExitCode(void);


#endif
