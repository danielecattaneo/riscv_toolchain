
#include <stdlib.h>
#include <stdio.h>
#include "cpu.h"
#include "memory.h"


int main(int argc, char *argv[])
{
   FILE *fpIn;
   size_t size, szp;
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
   for (i = 0; i < 10; i++) {
      t_cpuStatus status = cpuTick();
      printf("status = %d, pc = %08x\n", status, cpuGetRegister(CPU_REG_PC));
   }
   return 0;
}
