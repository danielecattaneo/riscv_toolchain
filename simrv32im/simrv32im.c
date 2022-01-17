
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
   while (status == CPU_STATUS_OK) {
      status = svVMTick();
   }
   return 0;
}
