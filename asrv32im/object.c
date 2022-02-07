#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "object.h"
#include "encode.h"


struct t_objLabel {
   struct t_objLabel *next;
   char *name;
   t_objSecItem *pointer;
};

struct t_objSection {
   t_objSecItem *items;
   t_objSecItem *lastItem;
   uint32_t start;
   uint32_t size;
};

struct t_object {
   t_objSection *data;
   t_objSection *text;
   t_objLabel *labelList;
};


static t_objSection *newSection(void)
{
   t_objSection *sec;

   sec = malloc(sizeof(t_objSection));
   if (!sec)
      return NULL;
   sec->items = NULL;
   sec->lastItem = NULL;
   sec->start = 0;
   sec->size = 0;
   return sec;
}


static void deleteSection(t_objSection *sec)
{
   t_objSecItem *item, *nextItm;

   if (!sec)
      return;

   for (item = sec->items; item != NULL; item = nextItm) {
      nextItm = item->next;
      free(item);
   }

   free(sec);
}


t_object *newObject(void)
{
   t_object *obj;

   obj = malloc(sizeof(t_object));
   if (!obj)
      return NULL;
   obj->data = newSection();
   obj->text = newSection();
   obj->labelList = NULL;
   if (!obj->data || !obj->text) {
      deleteObject(obj);
      return NULL;
   }
   return obj;
}


void deleteObject(t_object *obj)
{
   t_objLabel *lbl, *nextLbl;

   if (!obj)
      return;

   deleteSection(obj->data);
   deleteSection(obj->text);

   for (lbl = obj->labelList; lbl != NULL; lbl = nextLbl) {
      nextLbl = lbl->next;
      free(lbl->name);
      free(lbl);
   }
}


t_objLabel *objGetLabel(t_object *obj, const char *name)
{
   t_objLabel *lbl;

   for (lbl = obj->labelList; lbl != NULL; lbl = lbl->next) {
      if (strcmp(lbl->name, name) == 0)
         return lbl;
   }
   
   lbl = malloc(sizeof(t_objLabel));
   if (!lbl)
      return NULL;
   lbl->name = strdup(name);
   lbl->next = obj->labelList;
   lbl->pointer = NULL;
   obj->labelList = lbl;
   return lbl;
}


t_objSection *objGetSection(t_object *obj, t_objSectionID id)
{
   if (id == OBJ_SECTION_TEXT)
      return obj->text;
   if (id == OBJ_SECTION_DATA)
      return obj->data;
   return NULL;
}


static void objSecAppend(t_objSection *sec, t_objSecItem *item)
{
   t_objSecItem *prev;

   item->next = NULL;
   prev = sec->lastItem;
   if (prev) {
      prev->next = item;
      sec->lastItem = item;
   } else {
      sec->lastItem = sec->items = item;
   }
}

void objSecAppendData(t_objSection *sec, t_data data)
{
   t_objSecItem *itm;

   itm = malloc(sizeof(t_objSecItem));
   if (!itm)
      return;
   itm->address = 0;
   itm->class = OBJ_SEC_ITM_CLASS_DATA;
   itm->body.data = data;
   objSecAppend(sec, itm);
}

void objSecAppendInstruction(t_objSection *sec, t_instruction instr)
{
   t_objSecItem *itm;

   itm = malloc(sizeof(t_objSecItem));
   if (!itm)
      return;
   itm->address = 0;
   itm->class = OBJ_SEC_ITM_CLASS_INSTR;
   itm->body.instr = instr;
   objSecAppend(sec, itm);
}

int objSecDeclareLabel(t_objSection *sec, t_objLabel *label)
{
   t_objSecItem *itm;

   if (label->pointer)
      return 0;

   itm = malloc(sizeof(t_objSecItem));
   if (!itm)
      return 0;
   itm->address = 0;
   itm->class = OBJ_SEC_ITM_CLASS_VOID;
   objSecAppend(sec, itm);

   label->pointer = itm;
   return 1;
}


t_objSecItem *objSecGetItemList(t_objSection *sec)
{
   return sec->items;
}

uint32_t objSecGetStart(t_objSection *sec)
{
   return sec->start;
}

uint32_t objSecGetSize(t_objSection *sec)
{
   return sec->size;
}


uint32_t objLabelGetPointer(t_objLabel *lbl)
{
   return lbl->pointer->address;
}


static uint32_t objSecMaterializeAddresses(t_objSection *sec, uint32_t baseAddr)
{
   uint32_t curAddr = baseAddr;
   uint32_t thisSize;
   t_objSecItem *itm;

   sec->start = baseAddr;
   sec->size = 0;
   for (itm = sec->items; itm != NULL; itm = itm->next) {
      itm->address = curAddr;
      thisSize = 0;
      switch (itm->class) {
         case OBJ_SEC_ITM_CLASS_DATA:
            thisSize = itm->body.data.dataSize;
            break;
         case OBJ_SEC_ITM_CLASS_INSTR:
            thisSize = encGetInstrLength(itm->body.instr);
            break;
      }
      sec->size += thisSize;
      curAddr += thisSize;
   }
   return curAddr;
}

void objMaterializeAddresses(t_object *obj)
{
   uint32_t curAddr = 0x1000;
   curAddr = objSecMaterializeAddresses(obj->text, curAddr);
   objSecMaterializeAddresses(obj->data, curAddr);
}


void objSecMaterializeInstructions(t_objSection *sec)
{
   t_data tmp;
   t_objSecItem *itm;

   for (itm = sec->items; itm != NULL; itm = itm->next) {
      if (itm->class != OBJ_SEC_ITM_CLASS_INSTR)
         continue;
      encodeInstruction(itm->body.instr, &tmp);
      itm->class = OBJ_SEC_ITM_CLASS_DATA;
      itm->body.data = tmp;
   }
}

void objMaterializeInstructions(t_object *obj)
{
   objSecMaterializeInstructions(obj->text);
   objSecMaterializeInstructions(obj->data);
}


static void objSecDump(t_objSection *sec)
{
   t_objSecItem *itm;
   int i;

   printf("{\n");
   printf("  Start = 0x%08x\n", sec->start);
   printf("  Size = 0x%08x\n", sec->size);
   for (itm = sec->items; itm != NULL; itm = itm->next) {
      printf("  %p = {\n", (void *)itm);
      printf("    Address = 0x%08x,\n", itm->address);
      printf("    Class = %d,\n", itm->class);
      if (itm->class == OBJ_SEC_ITM_CLASS_INSTR) {
         printf("    Opcode = %d,\n", itm->body.instr.opcode);
         printf("    Dest = %d,\n", itm->body.instr.dest);
         printf("    Src1 = %d,\n", itm->body.instr.src1);
         printf("    Src2 = %d,\n", itm->body.instr.src2);
         printf("    Immediate = %d,\n", itm->body.instr.immediate);
         printf("    Label = %p,\n", (void *)itm->body.instr.label);
      } else if (itm->class == OBJ_SEC_ITM_CLASS_DATA) {
         printf("    DataSize = %ld,\n", itm->body.data.dataSize);
         printf("    Initialized = %d,\n", itm->body.data.initialized);
         if (itm->body.data.initialized) {
            printf("    Data = { ");
            for (i=0; i<DATA_MAX; i++)
               printf("%02x ", itm->body.data.data[i]);
            printf("}\n");
         }
      } else if (itm->class == OBJ_SEC_ITM_CLASS_VOID) {
         printf("    (null contents)\n");
      } else {
         printf("    (class is invalid!)\n");
      }
      printf("  },\n");
   }
   printf("}\n");
}

void objDump(t_object *obj)
{
   t_objLabel *label;

   printf("Labels: {\n");
   for (label = obj->labelList; label != NULL; label = label->next) {
      printf("  %p = {Name = \"%s\", Pointer = %p},\n", (void *)label, label->name, (void *)label->pointer);
   }
   printf("}\n");

   printf("Data section: ");
   objSecDump(obj->data);

   printf("Text section: ");
   objSecDump(obj->text);
}
