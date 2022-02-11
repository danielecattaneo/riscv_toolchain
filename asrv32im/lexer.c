#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include "lexer.h"


struct t_lexer {
   FILE *fp;
   int row, col;
   int tokRow, tokCol;
   int32_t tokLastData;
   char *buf;
   off_t bufWritePtr;
   off_t bufReadPtr;
   size_t bufSize;
};


t_lexer *newLexer(FILE *fp)
{
   t_lexer *lex;
   int temp;

   lex = calloc(1, sizeof(t_lexer));
   if (!lex)
      return NULL;

   lex->fp = fp;
   lex->row = lex->col = 0;
   lex->tokRow = lex->tokCol = 0;

   lex->bufSize = 80;
   lex->bufWritePtr = lex->bufReadPtr = 0;
   lex->buf = malloc(lex->bufSize * sizeof(char));
   if (!lex->buf) {
      free(lex);
      return NULL;
   }
   
   return lex;
}


void deleteLexer(t_lexer *lex)
{
   if (lex == NULL)
      return;
   fclose(lex->fp);
   free(lex->buf);
   free(lex);
}


static int lexExpandBufferIfFull(t_lexer *lex, int space)
{
   size_t newSize;

   if (lex->bufWritePtr + space < lex->bufSize)
      return 0;
   
   newSize = lex->bufSize * 2;
   lex->buf = realloc(lex->buf, newSize);
   if (!lex->buf)
      return 1;
   lex->bufSize = newSize;
   return 0;
}


static int lexGetChar(t_lexer *lex)
{
   int res;

   if (lex->bufReadPtr < lex->bufWritePtr)
      return lex->buf[lex->bufReadPtr++];
   
   if (lexExpandBufferIfFull(lex, 1) != 0)
      return 0;

   res = fgetc(lex->fp);
   if (res == -1)
      res = 0;
   else if (res == '\n') {
      lex->col = 0;
      lex->row++;
   } else
      lex->col++;

   lex->buf[lex->bufWritePtr++] = res;
   lex->bufReadPtr = lex->bufWritePtr;
   return res;
}


static void lexPutBack(t_lexer *lex, int n)
{
   assert(n <= lex->bufReadPtr);
   lex->bufReadPtr -= n;
}


static void lexFlushBuffer(t_lexer *lex)
{
   size_t size;
   
   size = lex->bufWritePtr - lex->bufReadPtr;
   if (size == 0) {
      lex->buf[0] = '\0';
      lex->bufReadPtr = lex->bufWritePtr = 0;
   } else {
      memmove(lex->buf, lex->buf + lex->bufReadPtr, size);
      lex->bufReadPtr = 0;
      lex->bufWritePtr = size;
   }
}


static int isnewline(char c)
{
   return c == '\n' || c == '\r';
}


static void lexSkipWhitespaceAndComments(t_lexer *lex)
{
   char next;
   int state;
   
   state = 0;
   while (state != -1) {
      next = lexGetChar(lex);

      if (state == 0) {
         /* normal whitespace */
         if (next == '/') {
            /* check for beginning of a comment */
            next = lexGetChar(lex);
            if (next == '*') {
               state = 1;
            } else if (next == '/') {
               state = 2;
            } else {
               lexPutBack(lex, 2);
               state = -1;
            }
         } else if (!isblank(next)) {
            lexPutBack(lex, 1);
            state = -1;
         }

      } else if (state == 1) {
         /* in a block comment */
         if (next == '*') {
            /* check if we are at the end of the comment */
            next = lexGetChar(lex);
            if (next == '/') {
               state = 0;
            } else {
               lexPutBack(lex, 1);
            }
         }

      } else if (state == 2) {
         /* in a line comment */
         if (isnewline(next))
            state = -1;
      }
   }
}


static t_tokenID lexConsumeNumber(t_lexer *lex, char firstChar)
{
   char next, *temp;

   if (firstChar == '-')
      firstChar = lexGetChar(lex);

   if (firstChar == '0') {
      next = lexGetChar(lex);

      if (next == 'x') {
         next = lexGetChar(lex);
         if (!isxdigit(next))
            return TOK_UNRECOGNIZED;
         while (isxdigit(next))
            next = lexGetChar(lex);

      } else if (next == 'b') {
         next = lexGetChar(lex);
         if (next != '0' && next != '1')
            return TOK_UNRECOGNIZED;
         while (next == '0' || next == '1')
            next = lexGetChar(lex);
         
      } else {
         while ('0' <= next && next <= '7')
            next = lexGetChar(lex);
      }
   } else {
      next = firstChar;
      while (isdigit(next))
         next = lexGetChar(lex);
   }

   lexPutBack(lex, 1);
   temp = lexGetLastTokenText(lex);
   lex->tokLastData = (int32_t)strtol(temp, NULL, 0);
   free(temp);
   return TOK_NUMBER;
}


static t_tokenID lexConsumeDirective(t_lexer *lex, char firstChar)
{
#define DIRECTIVE_MAX 10
   char kwbuf[DIRECTIVE_MAX+1];
   int i;
   char next;

   next = lexGetChar(lex);
   i = 0;
   while (isalnum(next) && i < DIRECTIVE_MAX) {
      kwbuf[i++] = next;
      next = lexGetChar(lex);
   }
   kwbuf[i] = '\0';
   lexPutBack(lex, 1);

   if (i >= DIRECTIVE_MAX)
      return TOK_UNRECOGNIZED;
   
   if (strcasecmp("text", kwbuf) == 0)
      return TOK_TEXT;
   if (strcasecmp("data", kwbuf) == 0)
      return TOK_DATA;
   if (strcasecmp("space", kwbuf) == 0)
      return TOK_SPACE;
   if (strcasecmp("word", kwbuf) == 0)
      return TOK_WORD;
   if (strcasecmp("global", kwbuf) == 0)
      return TOK_GLOBAL;
   return TOK_UNRECOGNIZED;
}


static t_tokenID lexConsumeAddressing(t_lexer *lex, char firstChar)
{
#define DIRECTIVE_MAX 10
   char kwbuf[DIRECTIVE_MAX+1];
   int i;
   char next;

   next = lexGetChar(lex);
   i = 0;
   while ((isalnum(next) || next == '_') && i < DIRECTIVE_MAX) {
      kwbuf[i++] = next;
      next = lexGetChar(lex);
   }
   kwbuf[i] = '\0';
   lexPutBack(lex, 1);

   if (i >= DIRECTIVE_MAX)
      return TOK_UNRECOGNIZED;
   
   if (strcasecmp("hi", kwbuf) == 0)
      return TOK_HI;
   if (strcasecmp("lo", kwbuf) == 0)
      return TOK_LO;
   if (strcasecmp("pcrel_hi", kwbuf) == 0)
      return TOK_PCREL_HI;
   if (strcasecmp("pcrel_lo", kwbuf) == 0)
      return TOK_PCREL_LO;
   return TOK_UNRECOGNIZED;
}


typedef struct t_keywordData {
   const char *text;
   t_tokenID id;
   int32_t info;
} t_keywordData;

static t_tokenID lexConsumeIdentifierOrKeyword(t_lexer *lex, char firstChar)
{
   static const t_keywordData kwdata[] = {
      {  "zero", TOK_REGISTER,  0},
      {    "ra", TOK_REGISTER,  1}, 
      {    "sp", TOK_REGISTER,  2}, 
      {    "gp", TOK_REGISTER,  3},
      {    "tp", TOK_REGISTER,  4}, 
      {    "t0", TOK_REGISTER,  5}, 
      {    "t1", TOK_REGISTER,  6},
      {    "t2", TOK_REGISTER,  7}, 
      {    "s0", TOK_REGISTER,  8}, 
      {    "fp", TOK_REGISTER,  8},
      {    "s1", TOK_REGISTER,  9}, 
      {    "a0", TOK_REGISTER, 10}, 
      {    "a1", TOK_REGISTER, 11},
      {    "a2", TOK_REGISTER, 12}, 
      {    "a3", TOK_REGISTER, 13}, 
      {    "a4", TOK_REGISTER, 14},
      {    "a5", TOK_REGISTER, 15}, 
      {    "a6", TOK_REGISTER, 16}, 
      {    "a7", TOK_REGISTER, 17},
      {    "s2", TOK_REGISTER, 18},
      {    "s3", TOK_REGISTER, 19},
      {    "s4", TOK_REGISTER, 20},
      {    "s5", TOK_REGISTER, 21},
      {    "s6", TOK_REGISTER, 22}, 
      {    "s7", TOK_REGISTER, 23},
      {    "s8", TOK_REGISTER, 24}, 
      {    "s9", TOK_REGISTER, 25}, 
      {   "s10", TOK_REGISTER, 26},
      {   "s11", TOK_REGISTER, 27}, 
      {    "t3", TOK_REGISTER, 28}, 
      {    "t4", TOK_REGISTER, 29}, 
      {    "t5", TOK_REGISTER, 30}, 
      {    "t6", TOK_REGISTER, 31}, 
      {   "add", TOK_MNEMONIC, INSTR_OPC_ADD}, 
      {   "sub", TOK_MNEMONIC, INSTR_OPC_SUB},
      {   "xor", TOK_MNEMONIC, INSTR_OPC_XOR}, 
      {    "or", TOK_MNEMONIC, INSTR_OPC_OR}, 
      {   "and", TOK_MNEMONIC, INSTR_OPC_AND}, 
      {   "sll", TOK_MNEMONIC, INSTR_OPC_SLL},
      {   "srl", TOK_MNEMONIC, INSTR_OPC_SRL}, 
      {   "sra", TOK_MNEMONIC, INSTR_OPC_SRA}, 
      {   "slt", TOK_MNEMONIC, INSTR_OPC_SLT},
      {  "sltu", TOK_MNEMONIC, INSTR_OPC_SLTU},
      {   "mul", TOK_MNEMONIC, INSTR_OPC_MUL},
      {  "mulh", TOK_MNEMONIC, INSTR_OPC_MULH},
      {"mulhsu", TOK_MNEMONIC, INSTR_OPC_MULHSU},
      { "mulhu", TOK_MNEMONIC, INSTR_OPC_MULHU},
      {   "div", TOK_MNEMONIC, INSTR_OPC_DIV},
      {  "divu", TOK_MNEMONIC, INSTR_OPC_DIVU},
      {   "rem", TOK_MNEMONIC, INSTR_OPC_REM},
      {  "remu", TOK_MNEMONIC, INSTR_OPC_REMU},
      {  "addi", TOK_MNEMONIC, INSTR_OPC_ADDI}, 
      {  "xori", TOK_MNEMONIC, INSTR_OPC_XORI}, 
      {   "ori", TOK_MNEMONIC, INSTR_OPC_ORI}, 
      {  "andi", TOK_MNEMONIC, INSTR_OPC_ANDI}, 
      {  "slli", TOK_MNEMONIC, INSTR_OPC_SLLI}, 
      {  "srli", TOK_MNEMONIC, INSTR_OPC_SRLI}, 
      {  "srai", TOK_MNEMONIC, INSTR_OPC_SRAI}, 
      {  "slti", TOK_MNEMONIC, INSTR_OPC_SLTI}, 
      { "sltiu", TOK_MNEMONIC, INSTR_OPC_SLTIU}, 
      {    "lb", TOK_MNEMONIC, INSTR_OPC_LB}, 
      {    "lh", TOK_MNEMONIC, INSTR_OPC_LH}, 
      {    "lw", TOK_MNEMONIC, INSTR_OPC_LW}, 
      {   "lbu", TOK_MNEMONIC, INSTR_OPC_LBU}, 
      {   "lhu", TOK_MNEMONIC, INSTR_OPC_LHU}, 
      {    "sb", TOK_MNEMONIC, INSTR_OPC_SB}, 
      {    "sh", TOK_MNEMONIC, INSTR_OPC_SH}, 
      {    "sw", TOK_MNEMONIC, INSTR_OPC_SW}, 
      {   "nop", TOK_MNEMONIC, INSTR_OPC_NOP}, 
      { "ecall", TOK_MNEMONIC, INSTR_OPC_ECALL}, 
      {"ebreak", TOK_MNEMONIC, INSTR_OPC_EBREAK},
      {   "lui", TOK_MNEMONIC, INSTR_OPC_LUI},
      { "auipc", TOK_MNEMONIC, INSTR_OPC_AUIPC},
      {   "jal", TOK_MNEMONIC, INSTR_OPC_JAL},
      {  "jalr", TOK_MNEMONIC, INSTR_OPC_JALR},
      {   "beq", TOK_MNEMONIC, INSTR_OPC_BEQ},
      {   "bne", TOK_MNEMONIC, INSTR_OPC_BNE},
      {   "blt", TOK_MNEMONIC, INSTR_OPC_BLT},
      {   "bge", TOK_MNEMONIC, INSTR_OPC_BGE},
      {  "bltu", TOK_MNEMONIC, INSTR_OPC_BLTU},
      {  "bgeu", TOK_MNEMONIC, INSTR_OPC_BGEU},
      {    "li", TOK_MNEMONIC, INSTR_OPC_LI},
      {    "la", TOK_MNEMONIC, INSTR_OPC_LA},
      {     "j", TOK_MNEMONIC, INSTR_OPC_J},
      {   "bgt", TOK_MNEMONIC, INSTR_OPC_BGT},
      {   "ble", TOK_MNEMONIC, INSTR_OPC_BLE},
      {  "bgtu", TOK_MNEMONIC, INSTR_OPC_BGTU},
      {  "bleu", TOK_MNEMONIC, INSTR_OPC_BLEU},
      {    NULL, TOK_UNRECOGNIZED}
   };
#define KEYWORD_MAX 10
   char kwbuf[KEYWORD_MAX+1];
   int kwbufi, i, tmp;
   char next;

   kwbufi = 0;
   kwbuf[kwbufi++] = firstChar;

   next = lexGetChar(lex);
   while (isalnum(next) || next == '_') {
      if (kwbufi < KEYWORD_MAX)
         kwbuf[kwbufi++] = next;
      next = lexGetChar(lex);
   }
   lexPutBack(lex, 1);

   if (kwbufi >= KEYWORD_MAX)
      return TOK_ID;
   
   kwbuf[kwbufi++] = '\0';

   if (kwbuf[0] == 'x') {
      for (i = 1; kwbuf[i] != '\0'; i++)
         if (!isdigit(kwbuf[i]))
            break;
      if (kwbuf[i] == '\0') {
         tmp = atoi(kwbuf+1);
         if (tmp < 32) {
            lex->tokLastData = tmp;
            return TOK_REGISTER;
         }
      }
   }

   for (i = 0; kwdata[i].text != NULL; i++) {
      if (strcasecmp(kwdata[i].text, kwbuf) == 0) {
         lex->tokLastData = kwdata[i].info;
         return kwdata[i].id;
      }
   }

   return TOK_ID;
}


t_tokenID lexNextToken(t_lexer *lex)
{
   char next;

   lexSkipWhitespaceAndComments(lex);
   lexFlushBuffer(lex);
   lex->tokCol = lex->col;
   lex->tokRow = lex->row;

   next = lexGetChar(lex);

   if (next == '\0')
      return TOK_EOF;

   if (next == '\n') {
      return TOK_NEWLINE;
   } else if (next == '\r') {
      next = lexGetChar(lex);
      if (next != '\n')
         lexPutBack(lex, 1);
      return TOK_NEWLINE;
   }

   if (next == ',')
      return TOK_COMMA;
   if (next == ':')
      return TOK_COLON;
   if (next == '(')
      return TOK_LPAR;
   if (next == ')')
      return TOK_RPAR;

   if (isdigit(next) || next == '-') {
      return lexConsumeNumber(lex, next);
   }
   if (next == '.') {
      return lexConsumeDirective(lex, next);
   }
   if (next == '%') {
      return lexConsumeAddressing(lex, next);
   }
   if (isalnum(next) || next == '_') {
      return lexConsumeIdentifierOrKeyword(lex, next);
   }

   return TOK_UNRECOGNIZED;
}


char *lexGetLastTokenText(t_lexer *lex)
{
   char *buf;
   size_t sz;
   
   sz = lex->bufReadPtr;
   buf = malloc((sz + 1) * sizeof(char));
   if (!buf)
      return NULL;
   memcpy(buf, lex->buf, sz);
   buf[sz] = '\0';
   return buf;
}


int lexGetLastTokenRow(t_lexer *lex)
{
   return lex->tokRow;
}

int lexGetLastTokenColumn(t_lexer *lex)
{
   return lex->tokCol;
}


t_instrOpcode lexGetLastMnemonicOpcode(t_lexer *lex)
{
   return (t_instrOpcode)lex->tokLastData;
}

t_instrRegID lexGetLastRegisterID(t_lexer *lex)
{
   return (t_instrOpcode)lex->tokLastData;
}

int32_t lexGetLastNumberValue(t_lexer *lex)
{
   return (t_instrOpcode)lex->tokLastData;
}
