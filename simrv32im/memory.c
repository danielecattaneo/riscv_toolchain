#include "memory.h"


typedef struct memArea {
   struct memArea *next;
   t_memAddress baseAddress;
   t_memSize extent;
   uint8_t *buffer;
} t_memArea;

t_memArea *memAreas = NULL;


static t_memAddress memAreaEnd(t_memArea *area)
{
   return area->baseAddress + area->extent;
}


static t_memArea *memFindArea(t_memAddress addr, t_memSize extent)
{
   t_memArea *curArea = memAreas;
   while (curArea) {
      if (curArea->baseAddress <= addr && addr < memAreaEnd(curArea)) {
         if ((addr + extent) <= memAreaEnd(curArea))
            return curArea;
         else
            return NULL;
      }
      curArea = curArea->next;
   }
   return NULL;
}


int memMapArea(t_memAddress base, t_memSize extent)
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
   newArea->next = nextArea;
   if (prevArea)
      prevArea->next = newArea;
   else
      memAreas = newArea;
   
   return MEM_NO_ERROR;
}


int memRead8(t_memAddress addr, uint8_t *out)
{
   t_memArea *area = memFindArea(addr, 1);
   if (!area)
      return MEM_MAPPING_ERROR;
   uint8_t *bufBasePtr = area->buffer + (size_t)(addr - area->baseAddress);
   *out = bufBasePtr[0];
   return MEM_NO_ERROR;
}

int memRead16(t_memAddress addr, uint16_t *out)
{
   t_memArea *area = memFindArea(addr, 2);
   if (!area)
      return MEM_MAPPING_ERROR;
   uint8_t *bufBasePtr = area->buffer + (size_t)(addr - area->baseAddress);
   *out = bufBasePtr[0] + (bufBasePtr[1] << 8);
   return MEM_NO_ERROR;
}

int memRead32(t_memAddress addr, uint32_t *out)
{
   t_memArea *area = memFindArea(addr, 4);
   if (!area)
      return MEM_MAPPING_ERROR;
   uint8_t *bufBasePtr = area->buffer + (size_t)(addr - area->baseAddress);
   *out = bufBasePtr[0] + (bufBasePtr[1] << 8) + 
         (bufBasePtr[2] << 16) + (bufBasePtr[3] << 24);
   return MEM_NO_ERROR;
}


t_memError memWrite8(t_memAddress addr, uint8_t in)
{
   t_memArea *area = memFindArea(addr, 1);
   if (!area)
      return MEM_MAPPING_ERROR;
   uint8_t *bufBasePtr = area->buffer + (size_t)(addr - area->baseAddress);
   bufBasePtr[0] = in;
   return MEM_NO_ERROR;
}

t_memError memWrite16(t_memAddress addr, uint16_t in)
{
   t_memArea *area = memFindArea(addr, 2);
   if (!area)
      return MEM_MAPPING_ERROR;
   uint8_t *bufBasePtr = area->buffer + (size_t)(addr - area->baseAddress);
   bufBasePtr[0] = in & 0xFF;
   bufBasePtr[1] = (in >> 8) & 0xFF;
   return MEM_NO_ERROR;
}

t_memError memWrite32(t_memAddress addr, uint32_t in)
{
   t_memArea *area = memFindArea(addr, 4);
   if (!area)
      return MEM_MAPPING_ERROR;
   uint8_t *bufBasePtr = area->buffer + (size_t)(addr - area->baseAddress);
   bufBasePtr[0] = in & 0xFF;
   bufBasePtr[1] = (in >> 8) & 0xFF;
   bufBasePtr[2] = (in >> 16) & 0xFF;
   bufBasePtr[3] = (in >> 24) & 0xFF;
   return MEM_NO_ERROR;
}
