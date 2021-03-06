#include <stdio.h>
#include <inttypes.h>
#include "supervisor.h"
#include "memory.h"
#include "debugger.h"


const t_memAddress svStackTop = 0x80000000;
t_memAddress svStackBottom;
t_isaInt svExitCode;


t_svError svInit(void)
{
   t_memError merr;

   svStackBottom = svStackTop - SV_STACK_PAGE_SIZE;
   merr = memMapArea(svStackBottom, SV_STACK_PAGE_SIZE, NULL);
   if (merr != MEM_NO_ERROR)
      return SV_MEMORY_ERROR;
   cpuSetRegister(CPU_REG_SP, svStackTop - 4);
   return SV_NO_ERROR;
}


int svExpandStack(void)
{
   t_memAddress faultAddr = memGetLastFaultAddress();
   if (faultAddr < svStackBottom && 
         faultAddr >= (svStackBottom - SV_STACK_PAGE_SIZE)) {
      svStackBottom -= SV_STACK_PAGE_SIZE;
      memMapArea(svStackBottom, SV_STACK_PAGE_SIZE, NULL);
      return 0;
   }
   return -1;
}


enum {
   SV_SYSCALL_EXIT    = 93,
   SV_SYSCALL_PUTCHAR = 2000,
   SV_SYSCALL_GETCHAR = 2001,
   SV_SYSCALL_PUTINT  = 2002,
   SV_SYSCALL_GETINT  = 2003
};

t_svStatus svHandleEnvCall(void)
{
   uint32_t syscallId = cpuGetRegister(CPU_REG_A0);
   int32_t ret;

   switch (syscallId) {
      case SV_SYSCALL_EXIT:
         svExitCode = cpuGetRegister(CPU_REG_A1);
         return SV_STATUS_TERMINATED;
      case SV_SYSCALL_PUTCHAR:
         ret = putchar(cpuGetRegister(CPU_REG_A1));
         cpuSetRegister(CPU_REG_A0, ret);
         break;
      case SV_SYSCALL_GETCHAR:
         ret = getchar();
         cpuSetRegister(CPU_REG_A0, ret);
         break;
      case SV_SYSCALL_PUTINT:
         fprintf(stdout, "%d\n", cpuGetRegister(CPU_REG_A1));
         break;
      case SV_SYSCALL_GETINT:
         fputs("int value? >", stdout);
         fscanf(stdin, "%"PRId32, &ret);
         cpuSetRegister(CPU_REG_A0, ret);
         break;
      default:
         return SV_STATUS_INVALID_SYSCALL;
   }

   return SV_STATUS_RUNNING;
}


t_isaInt svGetExitCode(void)
{
   return svExitCode;
}


t_svStatus svVMTick(void)
{
   int stop;
   t_cpuStatus cpuStatus;
   t_dbgResult dbgRes;
   t_svStatus status = SV_STATUS_RUNNING;

   dbgRes = dbgTick();
   if (dbgRes == DBG_RESULT_EXIT) {
      status = SV_STATUS_KILLED;
   } else {
      cpuStatus = cpuTick();
      if (cpuStatus == CPU_STATUS_MEMORY_FAULT) {
         svExpandStack();
         cpuClearLastFault();
         cpuStatus = cpuTick();
      }

      if (cpuStatus == CPU_STATUS_ECALL_TRAP) {
         status = svHandleEnvCall();
         if (status == SV_STATUS_RUNNING)
            cpuClearLastFault();
      } else if (cpuStatus == CPU_STATUS_EBREAK_TRAP) {
         if (dbgGetEnabled()) {
            dbgRequestEnter();
            cpuClearLastFault();
         }
      } else if (cpuStatus == CPU_STATUS_ILL_INST_FAULT)
         status = SV_STATUS_ILL_INST_FAULT;
      else if (cpuStatus == CPU_STATUS_MEMORY_FAULT)
         status = SV_STATUS_MEMORY_FAULT;
   }

   return status;
}

