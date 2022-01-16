
#include <stdlib.h>
#include <stdio.h>
#include "cpu.h"
#include "memory.h"


int main(int argc, char *argv[])
{
   FILE *fpIn;
   size_t size, szp;
   t_cpuStatus status;
   int i;

   fpIn = fopen(argv[1], "rb");
   fseeko(fpIn, 0, SEEK_END);
   size = ftello(fpIn);
   fseeko(fpIn, 0, SEEK_SET);

   memMapArea(0, size);
   for (szp = 0; szp < size; szp++) {
      uint8_t v;
      fread(&v, 1, 1, fpIn);
      memWrite8(szp, v);
   }

   cpuReset(0);
   status = CPU_STATUS_OK;
   while (status == CPU_STATUS_OK) {
      status = cpuTick();
      printf("status = %d, pc = %08x\n", status, cpuGetRegister(CPU_REG_PC));

   }
   return 0;
}
