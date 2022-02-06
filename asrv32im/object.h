#ifndef OBJECT_H
#define OBJECT_H

#include <stdint.h>
#include <ctype.h>


typedef int t_objSectionID;
enum {
   OBJ_SECTION_TEXT,
   OBJ_SECTION_DATA
};

typedef struct t_object t_object;
typedef struct t_objLabel t_objLabel;
typedef struct t_objSection t_objSection;


typedef int t_instrRegID;
typedef int t_instrOpcode;
enum {
   INSTR_OPC_ADD,
   INSTR_OPC_SUB,
   INSTR_OPC_XOR,
   INSTR_OPC_OR,
   INSTR_OPC_AND,
   INSTR_OPC_SLL,
   INSTR_OPC_SRL,
   INSTR_OPC_SRA,
   INSTR_OPC_SLT,
   INSTR_OPC_SLTU
};

typedef struct t_instruction {
   t_instrOpcode opcode;
   t_instrRegID dest;
   t_instrRegID src1;
   t_instrRegID src2;
   int32_t immediate;
   t_objLabel *label;
} t_instruction;

typedef struct t_data {
   uint8_t *data;
   size_t dataSize;
} t_data;


t_object *newObject(void);
void deleteObject(t_object *obj);

t_objLabel *objGetLabel(t_object *obj, const char *name);
void objDump(t_object *obj);

t_objSection *objGetSection(t_object *obj, t_objSectionID id);
void objSecAppendData(t_objSection *sec, t_data data);
void objSecAppendInstruction(t_objSection *sec, t_instruction instr);
int objSecDeclareLabel(t_objSection *sec, t_objLabel *label);


#endif
