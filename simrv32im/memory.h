#ifndef MEMORY_H
#define MEMORY_H

#include "cpu.h"
#include <stdlib.h>
#include <stdint.h>


typedef t_isaXSize t_memAddress;
typedef t_memAddress t_memSize;

typedef int t_memError;
enum {
   MEM_NO_ERROR = 0,
   MEM_OUT_OF_MEMORY = -1,
   MEM_EXTENT_MAPPED = -2,
   MEM_MAPPING_ERROR = -3,
};

t_memError memMapArea(t_memAddress base, t_memSize extent, uint8_t **outBuffer);

t_memError memRead8(t_memAddress addr, uint8_t *out);
t_memError memRead16(t_memAddress addr, uint16_t *out);
t_memError memRead32(t_memAddress addr, uint32_t *out);

uint8_t memDebugRead8(t_memAddress addr, int *mapped);
uint16_t memDebugRead16(t_memAddress addr, int *mapped);
uint32_t memDebugRead32(t_memAddress addr, int *mapped);

t_memError memWrite8(t_memAddress addr, uint8_t in);
t_memError memWrite16(t_memAddress addr, uint16_t in);
t_memError memWrite32(t_memAddress addr, uint32_t in);

t_memAddress memGetLastFaultAddress(void);


#endif
