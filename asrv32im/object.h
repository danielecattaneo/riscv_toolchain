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
   /* real instructions */
   INSTR_OPC_ADD,
   INSTR_OPC_SUB,
   INSTR_OPC_XOR,
   INSTR_OPC_OR,
   INSTR_OPC_AND,
   INSTR_OPC_SLL,
   INSTR_OPC_SRL,
   INSTR_OPC_SRA,
   INSTR_OPC_SLT,
   INSTR_OPC_SLTU,
   INSTR_OPC_MUL,
   INSTR_OPC_MULH,
   INSTR_OPC_MULHSU,
   INSTR_OPC_MULHU,
   INSTR_OPC_DIV,
   INSTR_OPC_DIVU,
   INSTR_OPC_REM,
   INSTR_OPC_REMU,
   INSTR_OPC_ADDI,
   INSTR_OPC_XORI,
   INSTR_OPC_ORI,
   INSTR_OPC_ANDI,
   INSTR_OPC_SLLI,
   INSTR_OPC_SRLI,
   INSTR_OPC_SRAI,
   INSTR_OPC_SLTI,
   INSTR_OPC_SLTIU,
   INSTR_OPC_LB,
   INSTR_OPC_LH,
   INSTR_OPC_LW,
   INSTR_OPC_LBU,
   INSTR_OPC_LHU,
   INSTR_OPC_SB,
   INSTR_OPC_SH,
   INSTR_OPC_SW,
   INSTR_OPC_ECALL,
   INSTR_OPC_EBREAK,
   INSTR_OPC_LUI,
   INSTR_OPC_AUIPC,
   INSTR_OPC_JAL,
   INSTR_OPC_JALR,
   INSTR_OPC_BEQ,
   INSTR_OPC_BNE,
   INSTR_OPC_BLT,
   INSTR_OPC_BGE,
   INSTR_OPC_BLTU,
   INSTR_OPC_BGEU,
   /* pseudo-instructions */
   INSTR_OPC_NOP,
   INSTR_OPC_LI,
   INSTR_OPC_LA
};

typedef int t_instrImmMode;
enum {
   INSTR_IMM_CONST,
   INSTR_IMM_LBL,
   INSTR_IMM_LBL_LO12,
   INSTR_IMM_LBL_HI20,
   INSTR_IMM_LBL_PCREL_LO12,
   INSTR_IMM_LBL_PCREL_LO12_DIRECT,
   INSTR_IMM_LBL_PCREL_HI20
};

typedef struct t_instruction {
   t_instrOpcode opcode;
   t_instrRegID dest;
   t_instrRegID src1;
   t_instrRegID src2;
   t_instrImmMode immMode;
   int32_t constant;
   t_objLabel *label;
} t_instruction;

#define DATA_MAX 16
typedef struct t_data {
   size_t dataSize;
   int initialized;
   uint8_t data[DATA_MAX];
} t_data;

typedef int t_objSecItemClass;
enum {
   OBJ_SEC_ITM_CLASS_INSTR,
   OBJ_SEC_ITM_CLASS_DATA,
   OBJ_SEC_ITM_CLASS_VOID
};

typedef struct t_objSecItem {
   struct t_objSecItem *next;
   uint32_t address;
   t_objSecItemClass class;
   union {
      t_instruction instr;
      t_data data;
   } body;
} t_objSecItem;


t_object *newObject(void);
void deleteObject(t_object *obj);

t_objLabel *objGetLabel(t_object *obj, const char *name);
void objDump(t_object *obj);

t_objSection *objGetSection(t_object *obj, t_objSectionID id);
void objSecAppendData(t_objSection *sec, t_data data);
void objSecAppendInstruction(t_objSection *sec, t_instruction instr);
int objSecDeclareLabel(t_objSection *sec, t_objLabel *label);

t_objSecItem *objSecGetItemList(t_objSection *sec);
uint32_t objSecGetStart(t_objSection *sec);
uint32_t objSecGetSize(t_objSection *sec);

t_objSecItem *objLabelGetPointedItem(t_objLabel *lbl);
const char *objLabelGetName(t_objLabel *lbl);
uint32_t objLabelGetPointer(t_objLabel *lbl);

int objMaterialize(t_object *obj);

#endif
