#include <stdio.h>
#include "loader.h"


t_ldrError ldrLoadBinary(const char *path, t_memAddress baseAddr)
{
   size_t size;
   uint8_t *buf;
   FILE *fp = fopen(path, "rb");
   if (fp == NULL)
      return LDR_FILE_ERROR;
   
   if (fseek(fp, 0, SEEK_END) < 0)
      return LDR_FILE_ERROR;
   size = ftello(fp);
   if (size < 0) {
      fclose(fp);
      return LDR_FILE_ERROR;
   }
   if (fseek(fp, 0, SEEK_SET) < 0) {
      fclose(fp);
      return LDR_FILE_ERROR;
   }
   
   if (memMapArea(baseAddr, size, &buf) != MEM_NO_ERROR) {
      fclose(fp);
      return LDR_MEMORY_ERROR;
   }
   if (fread(buf, size, 1, fp) < 1) {
      fclose(fp);
      return LDR_FILE_ERROR;
   }

   cpuReset(baseAddr);

   fclose(fp);
   return LDR_NO_ERROR;
}

