#ifndef ENCODE_H
#define ENCODE_H

#include "object.h"


size_t encGetInstrLength(t_instruction instr);
int encodeInstruction(t_instruction instr, t_data *out);


#endif
