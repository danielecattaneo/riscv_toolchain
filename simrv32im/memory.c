#include "memory.h"


typedef struct memArea {
   struct memArea *next;
   t_memAddress baseAddress;
   t_memSize extent;
   uint8_t *buffer;
} t_memArea;

t_memArea *memAreas = NULL;

t_memAddress memLastFaultAddress = 0;


static t_memAddress memAreaEnd(t_memArea *area)
{
   return area->baseAddress + area->extent;
}


static t_memArea *memFindArea(t_memAddress addr, t_memSize extent, int isDbg)
{
   t_memArea *curArea = memAreas;
   while (curArea) {
      if (curArea->baseAddress <= addr && addr < memAreaEnd(curArea)) {
         if ((addr + extent) <= memAreaEnd(curArea))
            return curArea;
         else
            goto fail;
      }
      curArea = curArea->next;
   }

fail:
   if (!isDbg)
      memLastFaultAddress = addr;
   return NULL;
}


t_memError memMapArea(t_memAddress base, t_memSize extent, uint8_t **outBuffer)
{
   t_memArea *prevArea = NULL;
   t_memArea *nextArea = memAreas;

   if (extent == 0)
      return MEM_NO_ERROR;

   while (nextArea) {
      if ((base + extent) <= nextArea->baseAddress)
         break;
      prevArea = nextArea;
      nextArea = nextArea->next;
   }
   if (prevArea) {
      if (!(base >= memAreaEnd(prevArea)))
         return MEM_EXTENT_MAPPED;
   }

   t_memArea *newArea = calloc(1, sizeof(t_memArea) + (size_t)extent);
   if (!newArea)
      return MEM_OUT_OF_MEMORY;
   newArea->baseAddress = base;
   newArea->extent = extent;
   newArea->buffer = (uint8_t *)((void *)newArea) + sizeof(t_memArea);
   if (outBuffer)
      *outBuffer = newArea->buffer;
   newArea->next = nextArea;
   if (prevArea)
      prevArea->next = newArea;
   else
      memAreas = newArea;
   
   return MEM_NO_ERROR;
}


t_memError memRead8(t_memAddress addr, uint8_t *out)
{
   t_memArea *area = memFindArea(addr, 1, 0);
   if (!area)
      return MEM_MAPPING_ERROR;
   uint8_t *bufBasePtr = area->buffer + (size_t)(addr - area->baseAddress);
   *out = bufBasePtr[0];
   return MEM_NO_ERROR;
}

t_memError memRead16(t_memAddress addr, uint16_t *out)
{
   t_memArea *area = memFindArea(addr, 2, 0);
   if (!area)
      return MEM_MAPPING_ERROR;
   uint8_t *bufBasePtr = area->buffer + (size_t)(addr - area->baseAddress);
   *out = bufBasePtr[0] + (bufBasePtr[1] << 8);
   return MEM_NO_ERROR;
}

t_memError memRead32(t_memAddress addr, uint32_t *out)
{
   t_memArea *area = memFindArea(addr, 4, 0);
   if (!area)
      return MEM_MAPPING_ERROR;
   uint8_t *bufBasePtr = area->buffer + (size_t)(addr - area->baseAddress);
   *out = bufBasePtr[0] + (bufBasePtr[1] << 8) + 
         (bufBasePtr[2] << 16) + (bufBasePtr[3] << 24);
   return MEM_NO_ERROR;
}


uint8_t memDebugRead8(t_memAddress addr, int *mapped)
{
   t_memArea *area = memFindArea(addr, 1, 1);
   if (!area) {
      if (mapped)
         *mapped = 0;
      return 0xFF;
   }
   uint8_t *bufBasePtr = area->buffer + (size_t)(addr - area->baseAddress);
   if (mapped)
      *mapped = 1;
   return bufBasePtr[0];
}

uint16_t memDebugRead16(t_memAddress addr, int *mapped)
{
   t_memArea *area = memFindArea(addr, 2, 1);
   if (!area) {
      if (mapped)
         *mapped = 0;
      return 0xFFFF;
   }
   uint8_t *bufBasePtr = area->buffer + (size_t)(addr - area->baseAddress);
   if (mapped)
      *mapped = 1;
   return bufBasePtr[0] + (bufBasePtr[1] << 8);
}

uint32_t memDebugRead32(t_memAddress addr, int *mapped)
{
   t_memArea *area = memFindArea(addr, 4, 1);
   if (!area) {
      if (mapped)
         *mapped = 0;
      return 0xFFFFFFFF;
   }
   uint8_t *bufBasePtr = area->buffer + (size_t)(addr - area->baseAddress);
   if (mapped)
      *mapped = 1;
   return bufBasePtr[0] + (bufBasePtr[1] << 8) + 
         (bufBasePtr[2] << 16) + (bufBasePtr[3] << 24);
}


t_memError memWrite8(t_memAddress addr, uint8_t in)
{
   t_memArea *area = memFindArea(addr, 1, 0);
   if (!area)
      return MEM_MAPPING_ERROR;
   uint8_t *bufBasePtr = area->buffer + (size_t)(addr - area->baseAddress);
   bufBasePtr[0] = in;
   return MEM_NO_ERROR;
}

t_memError memWrite16(t_memAddress addr, uint16_t in)
{
   t_memArea *area = memFindArea(addr, 2, 0);
   if (!area)
      return MEM_MAPPING_ERROR;
   uint8_t *bufBasePtr = area->buffer + (size_t)(addr - area->baseAddress);
   bufBasePtr[0] = in & 0xFF;
   bufBasePtr[1] = (in >> 8) & 0xFF;
   return MEM_NO_ERROR;
}

t_memError memWrite32(t_memAddress addr, uint32_t in)
{
   t_memArea *area = memFindArea(addr, 4, 0);
   if (!area)
      return MEM_MAPPING_ERROR;
   uint8_t *bufBasePtr = area->buffer + (size_t)(addr - area->baseAddress);
   bufBasePtr[0] = in & 0xFF;
   bufBasePtr[1] = (in >> 8) & 0xFF;
   bufBasePtr[2] = (in >> 16) & 0xFF;
   bufBasePtr[3] = (in >> 24) & 0xFF;
   return MEM_NO_ERROR;
}


t_memAddress memGetLastFaultAddress(void)
{
   return memLastFaultAddress;
}
