#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "parser.h"


typedef int t_parserError;
enum {
   P_ACCEPT = 1,
   P_REJECT = 0,
   P_SYN_ERROR = -1
};

typedef struct t_parserState {
   t_lexer *lex;
   t_object *object;
   t_objSection *curSection;
   int numErrors;
   int hasLastToken;
   t_tokenID lastToken;
} t_parserState;


static void parserEmitError(t_parserState *state, const char *msg)
{
   int r, c;

   if (!msg)
      msg = "unexpected token";
   r = lexGetLastTokenRow(state->lex) + 1;
   c = lexGetLastTokenColumn(state->lex);
   fprintf(stderr, "error at %d,%d: %s\n", r, c, msg);
   state->numErrors++;
}


static t_tokenID parserPeekToken(t_parserState *state)
{
   if (state->hasLastToken)
      return state->lastToken;
   state->lastToken = lexNextToken(state->lex);
   state->hasLastToken = 1;
   return state->lastToken;
}

static t_tokenID parserConsumeToken(t_parserState *state)
{
   t_tokenID res;

   if (!state->hasLastToken) {
      res = lexNextToken(state->lex);
   } else {
      res = state->lastToken;
   }
   state->hasLastToken = 0;
   return res;
}


static t_parserError parserAccept(t_parserState *state, t_tokenID tok)
{
   t_tokenID nextTok;

   nextTok = parserPeekToken(state);
   if (nextTok == tok) {
      parserConsumeToken(state);
      return P_ACCEPT;
   }
   return P_REJECT;
}

static t_parserError parserExpect(t_parserState *state, t_tokenID tok, const char *msg)
{
   int res;

   res = parserAccept(state, tok);
   if (res == P_ACCEPT)
      return res;
   parserEmitError(state, msg);
   return P_SYN_ERROR;
}


static t_parserError expectRegister(t_parserState *state, t_instrRegID *res, int last)
{
   if (parserExpect(state, TOK_REGISTER, "expected a register") != P_ACCEPT)
      return P_SYN_ERROR;
   *res = lexGetLastRegisterID(state->lex);
   if (!last && parserExpect(state, TOK_COMMA, "expected a comma") != P_ACCEPT)
      return P_SYN_ERROR;
   return P_ACCEPT;
}

static t_parserError expectNumber(t_parserState *state, int32_t *res, int32_t min, int32_t max)
{
   int32_t tmp;

   if (parserExpect(state, TOK_NUMBER, "expected a constant") != P_ACCEPT)
      return P_SYN_ERROR;
   
   tmp = lexGetLastNumberValue(state->lex);
   if (tmp < min || tmp > max) {
      parserEmitError(state, "numeric constant out of bounds");
      return P_SYN_ERROR;
   }
   *res = tmp;
   return P_ACCEPT;
}

static t_parserError expectLabel(t_parserState *state, t_instruction *instr)
{
   char *tmp;
   
   if (parserExpect(state, TOK_ID, "expected a label identifier") != P_ACCEPT)
      return P_SYN_ERROR;
   
   tmp = lexGetLastTokenText(state->lex);
   instr->label = objGetLabel(state->object, tmp);
   free(tmp);
   return P_ACCEPT;
}

typedef int t_immSizeClass;
enum {
   IMM_SIZE_5,
   IMM_SIZE_12,
   IMM_SIZE_20
};

static t_parserError expectImmediate(t_parserState *state, t_instruction *instr, t_immSizeClass size)
{
   int32_t min, max;

   if (parserAccept(state, TOK_LO) == P_ACCEPT) {
      instr->immMode = INSTR_IMM_LBL_LO12;
   } else if (parserAccept(state, TOK_HI) == P_ACCEPT) {
      instr->immMode = INSTR_IMM_LBL_HI20;
   } else if (parserAccept(state, TOK_PCREL_LO) == P_ACCEPT) {
      instr->immMode = INSTR_IMM_LBL_PCREL_LO12;
   } else if (parserAccept(state, TOK_PCREL_HI) == P_ACCEPT) {
      instr->immMode = INSTR_IMM_LBL_PCREL_HI20;
   } else {
      instr->immMode = INSTR_IMM_CONST;
   }

   if (instr->immMode == INSTR_IMM_CONST) {
      if (size == IMM_SIZE_5) {
         min = 0;
         max = 31;
      } else if (size == IMM_SIZE_12) {
         min = -0x800;
         max = 0x7FF;
      } else if (size == IMM_SIZE_20) {
         min = -0x80000;
         max = 0x7FFFF;
      } else
         assert("invalid immediate size");
      
      return expectNumber(state, &instr->constant, min, max);
   }
   
   if (size < IMM_SIZE_12) {
      parserEmitError(state, "immediate too large");
      return P_SYN_ERROR;
   }
   if (instr->immMode == INSTR_IMM_LBL_HI20 || instr->immMode == INSTR_IMM_LBL_PCREL_HI20) {
      if (size < IMM_SIZE_20) {
         parserEmitError(state, "immediate too large");
         return P_SYN_ERROR;
      }
   }
   
   if (parserExpect(state, TOK_LPAR, "expected left parenthesis") != P_ACCEPT)
      return P_SYN_ERROR;
   if (expectLabel(state, instr) != P_ACCEPT)
      return P_SYN_ERROR;
   if (parserExpect(state, TOK_RPAR, "expected right parenthesis") != P_ACCEPT)
      return P_SYN_ERROR;
   return P_ACCEPT;
}


typedef int t_instrFormat;
enum {
   FORMAT_OP,         /* mnemonic rd, rs1, rs2    */
   FORMAT_OPIMM,      /* mnemonic rd, rs1, imm    */
   FORMAT_LOAD,       /* mnemonic rd, imm(rs1)    */
   FORMAT_STORE,      /* mnemonic rs2, imm(rs1)   */
   FORMAT_LI,         /* mnemonic rd, imm         */
   FORMAT_JAL,        /* mnemonic rd, label       */
   FORMAT_JALR,       /* mnemonic rs1, rs2, imm   */
   FORMAT_SYSTEM,     /* mnemonic                 */

   FORMAT_STORE_GL,   /* mnemonic rs2, label, rs1 */
   FORMAT_BRANCH,     /* mnemonic rs1, rs2, label */
   FORMAT_JUMP,       /* mnemonic label           */
   FORMAT_LA          /* mnemonic rd, label       */
};

static t_instrFormat instrOpcodeToFormat(t_instrOpcode opcode)
{
   switch (opcode) {
      case INSTR_OPC_ADD:
      case INSTR_OPC_SUB:
      case INSTR_OPC_AND:
      case INSTR_OPC_OR :
      case INSTR_OPC_XOR:
      /*
      case INSTR_OPC_MUL:
      case INSTR_OPC_DIV:
      */
      case INSTR_OPC_SLL:
      case INSTR_OPC_SRL:
      case INSTR_OPC_SRA:
      case INSTR_OPC_SLT :
      case INSTR_OPC_SLTU:
         return FORMAT_OP;
      case INSTR_OPC_ADDI:
      case INSTR_OPC_ANDI:
      case INSTR_OPC_ORI :
      case INSTR_OPC_XORI:
      case INSTR_OPC_SLLI:
      case INSTR_OPC_SRLI:
      case INSTR_OPC_SRAI:
      case INSTR_OPC_SLTI :
      case INSTR_OPC_SLTIU:
         return FORMAT_OPIMM;
      /*
      case OPC_J:
         return FORMAT_JUMP;
      case OPC_BEQ :
      case OPC_BNE :
      case OPC_BLT :
      case OPC_BLTU:
      case OPC_BGE :
      case OPC_BGEU:
      case OPC_BGT :
      case OPC_BGTU:
      case OPC_BLE :
      case OPC_BLEU:
         return FORMAT_BRANCH;
      */
      case INSTR_OPC_LB:
      case INSTR_OPC_LH:
      case INSTR_OPC_LW:
      case INSTR_OPC_LBU:
      case INSTR_OPC_LHU:
         return FORMAT_LOAD;
      /*
      case OPC_LW_G:
         return FORMAT_LOAD_GL;
      */
      case INSTR_OPC_SB:
      case INSTR_OPC_SH:
      case INSTR_OPC_SW:
         return FORMAT_STORE;
      /*
      case OPC_SW_G:
         return FORMAT_STORE_GL;
      case OPC_LI:
      */
      case INSTR_OPC_LUI:
      case INSTR_OPC_AUIPC:
         return FORMAT_LI;
      /*
      case OPC_LA:
         return FORMAT_LA;
      */
      case INSTR_OPC_JAL:
         return FORMAT_JAL;
      case INSTR_OPC_JALR:
         return FORMAT_JALR;
      case INSTR_OPC_NOP:
      case INSTR_OPC_ECALL:
      case INSTR_OPC_EBREAK:
         return FORMAT_SYSTEM;
   }
   return -1;
}


static t_parserError expectInstruction(t_parserState *state, t_tokenID lastToken)
{
   t_instrFormat format;
   t_immSizeClass immSize;
   t_parserError err;
   t_instruction instr = { 0 };

   if (lastToken != TOK_MNEMONIC)
      return P_SYN_ERROR;
   instr.opcode = lexGetLastMnemonicOpcode(state->lex);

   format = instrOpcodeToFormat(instr.opcode);
   switch (format) {
      case FORMAT_OP:
         if (expectRegister(state, &instr.dest, 0) != P_ACCEPT)
            return P_SYN_ERROR;
         if (expectRegister(state, &instr.src1, 0) != P_ACCEPT)
            return P_SYN_ERROR;
         if (expectRegister(state, &instr.src2, 1) != P_ACCEPT)
            return P_SYN_ERROR;
         break;
      
      case FORMAT_OPIMM:
         if (expectRegister(state, &instr.dest, 0) != P_ACCEPT)
            return P_SYN_ERROR;
         if (expectRegister(state, &instr.src1, 0) != P_ACCEPT)
            return P_SYN_ERROR;
         if (instr.opcode == INSTR_OPC_SLLI || instr.opcode == INSTR_OPC_SRLI ||
               instr.opcode == INSTR_OPC_SRAI) {
            immSize = IMM_SIZE_5;
         } else {
            immSize = IMM_SIZE_12;
         }
         if (expectImmediate(state, &instr, immSize) != P_ACCEPT)
            return P_SYN_ERROR;
         break;

      case FORMAT_LOAD:
      case FORMAT_STORE:
         if (format == FORMAT_LOAD)
            err = expectRegister(state, &instr.dest, 0);
         else
            err = expectRegister(state, &instr.src2, 0);
         if (err != P_ACCEPT)
            return P_SYN_ERROR;
         if (expectImmediate(state, &instr, IMM_SIZE_12) != P_ACCEPT)
            return P_SYN_ERROR;
         if (parserExpect(state, TOK_LPAR, "expected parenthesis") != P_ACCEPT)
            return P_SYN_ERROR;
         if (expectRegister(state, &instr.src1, 1) != P_ACCEPT)
            return P_SYN_ERROR;
         if (parserExpect(state, TOK_RPAR, "expected parenthesis") != P_ACCEPT)
            return P_SYN_ERROR;
         break;

      case FORMAT_LI:
         if (expectRegister(state, &instr.dest, 0) != P_ACCEPT)
            return P_SYN_ERROR;
         if (expectImmediate(state, &instr, IMM_SIZE_20) != P_ACCEPT)
            return P_SYN_ERROR;
         break;

      case FORMAT_JAL:
         if (expectRegister(state, &instr.dest, 0) != P_ACCEPT)
            return P_SYN_ERROR;
         if (expectLabel(state, &instr) != P_ACCEPT)
            return P_SYN_ERROR;
         instr.immMode = INSTR_IMM_LBL;
         break;

      case FORMAT_JALR:
         if (expectRegister(state, &instr.dest, 0) != P_ACCEPT)
            return P_SYN_ERROR;
         if (expectImmediate(state, &instr, IMM_SIZE_12) != P_ACCEPT)
            return P_SYN_ERROR;
         if (parserExpect(state, TOK_LPAR, "expected parenthesis") != P_ACCEPT)
            return P_SYN_ERROR;
         if (expectRegister(state, &instr.src1, 1) != P_ACCEPT)
            return P_SYN_ERROR;
         if (parserExpect(state, TOK_RPAR, "expected parenthesis") != P_ACCEPT)
            return P_SYN_ERROR;
         break;

      case FORMAT_SYSTEM:
         break;
         
      default:
         return P_SYN_ERROR;
   }
   
   objSecAppendInstruction(state->curSection, instr);
   return P_ACCEPT;
}


static t_parserError expectData(t_parserState *state, t_tokenID lastToken)
{
   int32_t temp;
   t_data data = { 0 };

   if (lastToken == TOK_SPACE) {
      if (!parserExpect(state, TOK_NUMBER, "expected number after \".space\""))
         return P_SYN_ERROR;
      data.dataSize = lexGetLastNumberValue(state->lex);
      data.initialized = 0;
      objSecAppendData(state->curSection, data);
      return P_ACCEPT;
   }

   if (lastToken == TOK_WORD) {
      do {
         if (!parserExpect(state, TOK_NUMBER, "expected number"))
            return P_SYN_ERROR;
         temp = lexGetLastNumberValue(state->lex);
         data.dataSize = sizeof(int32_t);
         data.initialized = 1;
         data.data[0] = temp & 0xFF;
         data.data[1] = (temp >> 8) & 0xFF;
         data.data[2] = (temp >> 16) & 0xFF;
         data.data[3] = (temp >> 24) & 0xFF;
         objSecAppendData(state->curSection, data);
      } while (parserAccept(state, TOK_COMMA));
      return P_ACCEPT;
   }

   return P_SYN_ERROR;
}


static t_parserError expectLineContent(t_parserState *state)
{
   if (parserAccept(state, TOK_SPACE) == P_ACCEPT)
      return expectData(state, TOK_SPACE);
   if (parserAccept(state, TOK_WORD) == P_ACCEPT)
      return expectData(state, TOK_WORD);
   if (parserAccept(state, TOK_MNEMONIC) == P_ACCEPT)
      return expectInstruction(state, TOK_MNEMONIC);
   parserEmitError(state, "expected a data directive or an instruction");
   return P_SYN_ERROR;
}


static t_parserError expectLine(t_parserState *state)
{
   char *temp;
   t_objLabel *label;

   if (parserAccept(state, TOK_NEWLINE) == P_ACCEPT)
      return P_ACCEPT;

   if (parserAccept(state, TOK_TEXT) == P_ACCEPT) {
      state->curSection = objGetSection(state->object, OBJ_SECTION_TEXT);
      return parserExpect(state, TOK_NEWLINE, "expected end of the line");
   } else if (parserAccept(state, TOK_DATA) == P_ACCEPT) {
      state->curSection = objGetSection(state->object, OBJ_SECTION_DATA);
      return parserExpect(state, TOK_NEWLINE, "expected end of the line");
   }

   if (parserAccept(state, TOK_GLOBAL) == P_ACCEPT) {
      if (parserExpect(state, TOK_ID, "expected label identifier") != P_ACCEPT)
         return P_SYN_ERROR;
      return parserExpect(state, TOK_NEWLINE, "expected end of the line");
   }

   if (parserAccept(state, TOK_ID) == P_ACCEPT) {
      temp = lexGetLastTokenText(state->lex);
      if (parserExpect(state, TOK_COLON, "expected label or valid mnemonic") != P_ACCEPT)
         return P_SYN_ERROR;
      label = objGetLabel(state->object, temp);
      if (!objSecDeclareLabel(state->curSection, label))
         parserEmitError(state, "label already declared");
      free(temp);
   }

   if (parserAccept(state, TOK_NEWLINE) == P_ACCEPT)
      return P_ACCEPT;
   if (expectLineContent(state) != P_ACCEPT)
      return P_SYN_ERROR;
   return parserExpect(state, TOK_NEWLINE, "expected end of the line");   
}


t_object *parseObject(t_lexer *lex)
{
   t_parserError err;
   t_parserState state;

   state.lex = lex;
   state.object = newObject();
   if (!state.object)
      goto error;
   state.curSection = objGetSection(state.object, OBJ_SECTION_TEXT);
   state.numErrors = 0;
   state.hasLastToken = 0;

   while (parserAccept(&state, TOK_EOF) != P_ACCEPT) {
      err = expectLine(&state);
      if (err != P_ACCEPT) {
         t_tokenID tmp;

         if (state.numErrors > 10) {
            fprintf(stderr, "too many errors, aborting...\n");
            break;
         }

         /* try to ignore the error and advance to the next line */
         tmp = parserPeekToken(&state);
         while (tmp != TOK_NEWLINE && tmp != TOK_EOF) {
            parserConsumeToken(&state);
            tmp = parserPeekToken(&state);
         }
      }
   }

   if (state.numErrors > 0)
      goto error;

   return state.object;
error:
   deleteObject(state.object);
   return NULL;
}
