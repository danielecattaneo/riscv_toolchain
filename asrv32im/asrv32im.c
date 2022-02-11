#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "parser.h"
#include "output.h"


int main(int argc, char *argv[])
{
   FILE *fp;
   t_lexer *lex;
   t_tokenID tok;
   t_object *obj;
   char *buf;
   int tmp;

   if (argc < 3) {
      if (argc > 0)
         printf("usage: %s input.s output.elf\n", argv[0]);
      return 1;
   }
   
   fp = fopen(argv[1], "r");
   lex = newLexer(fp);
   obj = parseObject(lex);
   /*objDump(obj);*/
   if (obj) {
      if (!objMaterialize(obj))
         exit(1);
      /*objDump(obj);*/
      tmp = outputToELF(obj, argv[2]);
      printf("output error = %d\n", tmp);
      deleteObject(obj);
   }

#if 0
   tok = lexNextToken(lex);
   while (tok != TOK_UNRECOGNIZED && tok != TOK_EOF) {
      buf = lexGetLastTokenText(lex);
      printf("%3d %3d:%3d \"%s\" (%d)\n", tok, lexGetLastTokenRow(lex), lexGetLastTokenColumn(lex), buf, lexGetLastNumberValue(lex));
      free(buf);
      tok = lexNextToken(lex);
   }
   printf("%3d\n", tok);
#endif

   deleteLexer(lex);
   return 0;
}

