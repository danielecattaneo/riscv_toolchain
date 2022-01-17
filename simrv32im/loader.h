#ifndef LOADER_H
#define LOADER_H

#include "memory.h"


typedef int t_ldrError;
enum {
   LDR_NO_ERROR = 0,
   LDR_FILE_ERROR = -1,
   LDR_MEMORY_ERROR = -2
};


t_ldrError ldrLoadBinary(const char *path, t_memAddress baseAddr);


#endif
