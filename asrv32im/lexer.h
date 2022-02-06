#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include "object.h"


typedef int t_tokenID;
enum {
   TOK_UNRECOGNIZED = -1,
   TOK_EOF = 0,
   TOK_NEWLINE,
   TOK_ID,
   TOK_NUMBER,
   TOK_COMMA,
   TOK_COLON,
   TOK_LPAR,
   TOK_RPAR,
   TOK_REGISTER,
   TOK_TEXT,
   TOK_DATA,
   TOK_SPACE,
   TOK_WORD,
   TOK_GLOBAL,
   TOK_MNEMONIC
};

typedef struct t_lexer t_lexer;


t_lexer *newLexer(FILE *fp);
void deleteLexer(t_lexer *lex);

t_tokenID lexNextToken(t_lexer *lex);
char *lexGetLastTokenText(t_lexer *lex);
int lexGetLastTokenRow(t_lexer *lex);
int lexGetLastTokenColumn(t_lexer *lex);

t_instrOpcode lexGetLastMnemonicOpcode(t_lexer *lex);
t_instrRegID lexGetLastRegisterID(t_lexer *lex);
int32_t lexGetLastNumberValue(t_lexer *lex);


#endif
