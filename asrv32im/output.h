#ifndef OUTPUT_H
#define OUTPUT_H

#include "object.h"

typedef int t_outError;
enum {
   OUT_NO_ERROR,
   OUT_FILE_ERROR,
   OUT_INVALID_BINARY,
   OUT_MEMORY_ERROR
};

t_outError outputToELF(t_object *obj, const char *fname);

#endif
