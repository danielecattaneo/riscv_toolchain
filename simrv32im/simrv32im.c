
#include <stdlib.h>
#include <stdio.h>
#include "cpu.h"
#include "memory.h"
#include "loader.h"
#include "supervisor.h"


int main(int argc, char *argv[])
{
   t_svStatus status;

   ldrLoadBinary(argv[1], 0);

   status = svInit();
   while (status == SV_STATUS_RUNNING) {
      status = svVMTick();
   }
   if (status == SV_STATUS_MEMORY_FAULT) {
      fprintf(stderr, "Memory fault at address 0x%08x, execution stopped.\n",
            memGetLastFaultAddress());
   } else if (status == SV_STATUS_ILL_INST_FAULT) {
      fprintf(stderr, "Illegal instruction at address 0x%08x\n",
            cpuGetRegister(CPU_REG_PC));
   }
   return 0;
}
