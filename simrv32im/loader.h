#ifndef LOADER_H
#define LOADER_H

#include "memory.h"


typedef int t_ldrError;
enum {
   LDR_NO_ERROR = 0,
   LDR_FILE_ERROR = -1,
   LDR_MEMORY_ERROR = -2,
   LDR_INVALID_FORMAT = -4,
   LDR_INVALID_ARCH = -3
};

typedef int t_ldrFileType;
enum {
   LDR_FORMAT_BINARY = 0,
   LDR_FORMAT_ELF = 1,
   LDR_FORMAT_DETECT_ERROR = -1
};


t_ldrError ldrLoadBinary(const char *path, t_memAddress baseAddr);
t_ldrError ldrLoadELF(const char *path);

t_ldrFileType ldrDetectExecType(const char *path);


#endif
