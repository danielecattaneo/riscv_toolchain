
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include "cpu.h"
#include "memory.h"
#include "loader.h"
#include "supervisor.h"
#include "debugger.h"


void usage(const char *name)
{
   puts("ACSE RISC-V RV32IM simulator, (c) 2022 Politecnico di Milano");
   printf("usage: %s [options] executable\n\n", name);
   puts("Options:");
   puts("  -d, --debug           Enters debug mode before starting execution");
   puts("  -e, --entry=ADDR      Force the entry point to ADDR");
   puts("  -l, --load-addr=ADDR  Sets the executable loading address (only");
   puts("                          for executables in raw binary format)");
   puts("  -h, --help            Displays available options");
}


int main(int argc, char *argv[])
{
   t_svStatus status;
   int ch, debug, entryIsSet;
   char *tmpStr, *name;
   t_memAddress entry, load;
   t_ldrFileType excType;
   t_ldrError ldrErr;
   static const struct option options[] = {
      { "debug",     no_argument,       NULL, 'd' },
      { "entry",     required_argument, NULL, 'e' },
      { "help",      no_argument,       NULL, 'h' },
      { "load-addr", required_argument, NULL, 'l' },
   };

   name = argv[0];
   debug = 0;
   entry = entryIsSet = 0;
   load = 0;

   while ((ch = getopt_long(argc, argv, "de:hl:", options, NULL)) != -1) {
      switch (ch) {
         case 'd':
            debug = 1;
            break;
         case 'e':
            entryIsSet = 1;
            entry = (t_memAddress)strtoul(optarg, &tmpStr, 0);
            if (tmpStr == optarg) {
               fprintf(stderr, "Invalid entry address\n");
               return 1;
            }
            break;
         case 'l':
            load = (t_memAddress)strtoul(optarg, &tmpStr, 0);
            if (tmpStr == optarg) {
               fprintf(stderr, "Invalid load address\n");
               return 1;
            }
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
      return 2;
   } else if (argc > 1) {
      fprintf(stderr, "Cannot load more than one file, exiting.\n");
      return 2;
   }

   if (debug)
      dbgEnable();
   
   excType = ldrDetectExecType(argv[0]);
   if (excType == LDR_FORMAT_BINARY) {
      if (!entryIsSet)
         entry = load;
      ldrErr = ldrLoadBinary(argv[0], load, entry);
   } else if (excType == LDR_FORMAT_ELF) {
      ldrErr = ldrLoadELF(argv[0]);
      if (entryIsSet)
         cpuSetRegister(CPU_REG_PC, entry);
   } else {
      fprintf(stderr, "Could not open executable, exiting.\n");
      return 3;
   }

   if (ldrErr == LDR_INVALID_ARCH) {
      fprintf(stderr, "Not a valid RISC-V executable, exiting.\n");
      return 4;
   } else if (ldrErr == LDR_INVALID_FORMAT) {
      fprintf(stderr, "Unsupported executable, exiting.\n");
      return 5;
   } else if (ldrErr != LDR_NO_ERROR) {
      fprintf(stderr, "Error during executable loading, exiting.\n");
      return 6;
   }

   status = svInit();

   if (debug)
      dbgRequestEnter();

   while (status == SV_STATUS_RUNNING) {
      status = svVMTick();
   }

   if (status == SV_STATUS_MEMORY_FAULT) {
      fprintf(stderr, "Memory fault at address 0x%08x, execution stopped.\n",
            memGetLastFaultAddress());
      return 100;
   } else if (status == SV_STATUS_ILL_INST_FAULT) {
      fprintf(stderr, "Illegal instruction at address 0x%08x\n",
            cpuGetRegister(CPU_REG_PC));
      return 101;
   }
   return 0;
}
