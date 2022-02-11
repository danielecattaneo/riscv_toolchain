#ifndef ENCODE_H
#define ENCODE_H

#include "object.h"

#define MAX_EXP_FACTOR 2

size_t encGetInstrLength(t_instruction instr);
int encExpandPseudoInstruction(t_instruction instr, t_instruction res[MAX_EXP_FACTOR]);
int encResolveImmediates(t_instruction *instr, uint32_t pc);
int encPhysicalInstruction(t_instruction instr, uint32_t pc, t_data *out);

#endif
