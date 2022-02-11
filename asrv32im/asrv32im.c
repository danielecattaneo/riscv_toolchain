#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "lexer.h"
#include "parser.h"
#include "output.h"


void usage(const char *name)
{
   puts("ACSE RISC-V RV32IM assembler, (c) 2022 Politecnico di Milano");
   printf("usage: %s [options] input\n\n", name);
   puts("Options:");
   puts("  -o OBJFILE    Name the output OBJFILE (default output.o)");
   puts("  -h, --help    Displays available options");
}


int main(int argc, char *argv[])
{
   FILE *fp;
   t_lexer *lex = NULL;
   t_object *obj = NULL;
   char *name, *out;
   int ch, res = 0;
   static const struct option options[] = {
      { "help",      no_argument,       NULL, 'h' },
   };

   name = argv[0];
   out = "output.o";

   while ((ch = getopt_long(argc, argv, "ho:", options, NULL)) != -1) {
      switch (ch) {
         case 'o':
            out = optarg;
            break;
         case 'h':
            usage(name);
            return 0;
         default:
            usage(name);
            return 1;
      }
   }
   argc -= optind;
   argv += optind;

   if (argc < 1) {
      usage(name);
      return 1;
   } else if (argc > 1) {
      fprintf(stderr, "Cannot assemble more than one file, exiting.\n");
      return 1;
   }

   res = 1;
   fp = fopen(argv[0], "r");
   if (fp == NULL) {
      fprintf(stderr, "Could not open the input file, exiting\n");
      goto fail;
   }
   lex = newLexer(fp);
   if (lex == NULL) {
      fprintf(stderr, "Could not create a lexer object, exiting\n");
      goto fail;
   }
   obj = parseObject(lex);
   if (obj == NULL)
      goto fail;
   if (!objMaterialize(obj))
      goto fail;
   if (outputToELF(obj, out) != OUT_NO_ERROR) {
      fprintf(stderr, "Could not write to output file, exiting\n");
      goto fail;
   }
   
   res = 0;
fail:
   deleteLexer(lex);
   deleteObject(obj);
   return res;
}

