#include <stdlib.h>
#include <stdio.h>
#include "parser.h"


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
   if (!msg)
      msg = "unexpected token";
   fprintf(stderr, "error at %d,%d: %s\n", lexGetLastTokenRow(state->lex), lexGetLastTokenColumn(state->lex), msg);
   state->numErrors++;
}


static t_tokenID parserGetNextToken(t_parserState *state)
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


static int parserAccept(t_parserState *state, t_tokenID tok)
{
   t_tokenID nextTok;

   nextTok = parserGetNextToken(state);
   if (nextTok == tok) {
      parserConsumeToken(state);
      return 1;
   }
   return 0;
}

static int parserExpect(t_parserState *state, t_tokenID tok, const char *msg)
{
   int res;

   res = parserAccept(state, tok);
   if (res)
      return 1;
   parserEmitError(state, msg);
   return 0;
}


int expectRegister(t_parserState *state, t_instrRegID *res, int last)
{
   if (!parserExpect(state, TOK_REGISTER, "expected a register"))
      return 0;
   *res = lexGetLastRegisterID(state->lex);
   if (!last && !parserExpect(state, TOK_COMMA, "expected a comma"))
      return 0;
   return 1;
}


int expectLineContent(t_parserState *state)
{
   int32_t temp;
   t_data data = { 0 };
   t_instruction instr = { 0 };

   if (parserAccept(state, TOK_SPACE)) {
      if (!parserExpect(state, TOK_NUMBER, "expected number after \".space\""))
         return 0;
      data.dataSize = lexGetLastNumberValue(state->lex);
      data.initialized = 0;
      objSecAppendData(state->curSection, data);
      return 1;
   }

   if (parserAccept(state, TOK_WORD)) {
      do {
         if (!parserExpect(state, TOK_NUMBER, "expected number"))
            return 0;
         temp = lexGetLastNumberValue(state->lex);
         data.dataSize = sizeof(int32_t);
         data.initialized = 1;
         data.data[0] = temp & 0xFF;
         data.data[1] = (temp >> 8) & 0xFF;
         data.data[2] = (temp >> 16) & 0xFF;
         data.data[3] = (temp >> 24) & 0xFF;
         objSecAppendData(state->curSection, data);
      } while (parserAccept(state, TOK_COMMA));
      return 1;
   }

   if (!parserExpect(state, TOK_MNEMONIC, "expected an instruction mnemonic"))
      return 0;
   instr.opcode = lexGetLastMnemonicOpcode(state->lex);

   if (!expectRegister(state, &instr.dest, 0))
      return 0;
   if (!expectRegister(state, &instr.src1, 0))
      return 0;
   if (!expectRegister(state, &instr.src2, 1))
      return 0;
   
   objSecAppendInstruction(state->curSection, instr);
   return 1;
}


int expectLine(t_parserState *state)
{
   char *temp;
   t_objLabel *label;

   if (parserAccept(state, TOK_NEWLINE))
      return 1;

   if (parserAccept(state, TOK_TEXT)) {
      state->curSection = objGetSection(state->object, OBJ_SECTION_TEXT);
      return parserExpect(state, TOK_NEWLINE, "expected end of the line");
   } else if (parserAccept(state, TOK_DATA)) {
      state->curSection = objGetSection(state->object, OBJ_SECTION_DATA);
      return parserExpect(state, TOK_NEWLINE, "expected end of the line");
   }

   if (parserAccept(state, TOK_GLOBAL)) {
      parserExpect(state, TOK_ID, "expected label identifier");
      return parserExpect(state, TOK_NEWLINE, "expected end of the line");
   }

   if (parserAccept(state, TOK_ID)) {
      temp = lexGetLastTokenText(state->lex);
      parserExpect(state, TOK_COLON, "expected semicolon after label");
      label = objGetLabel(state->object, temp);
      if (!objSecDeclareLabel(state->curSection, label))
         parserEmitError(state, "label already declared");
      free(temp);
   }

   if (!expectLineContent(state))
      return 0;
   return parserExpect(state, TOK_NEWLINE, "expected end of the line");   
}


t_object *parseObject(t_lexer *lex)
{
   t_parserState state;

   state.lex = lex;
   state.object = newObject();
   if (!state.object)
      goto error;
   state.curSection = objGetSection(state.object, OBJ_SECTION_TEXT);
   state.numErrors = 0;
   state.hasLastToken = 0;

   while (!parserAccept(&state, TOK_EOF)) {
      if (!expectLine(&state))
         break;
   }

   if (state.numErrors > 0)
      goto error;

   return state.object;
error:
   deleteObject(state.object);
   return NULL;
}
