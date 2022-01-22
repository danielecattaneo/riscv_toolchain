
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
   printf("usage: %s [options] executable\n\n", name);
   puts("Options:");
   puts("  -d, --debug           Enters debug mode before starting execution");
   puts("  -e, --entry=ADDR      Force the entry point to ADDR");
   puts("  -l, --load-addr=ADDR  Sets the executable loading address");
   puts("  -h, --help            Displays available options");
}


int main(int argc, char *argv[])
{
   t_svStatus status;
   int ch, debug;
   char *tmpStr, *name;
   t_memAddress entry, load;
   static const struct option options[] = {
      { "debug",     no_argument,       NULL, 'd' },
      { "entry",     required_argument, NULL, 'e' },
      { "load-addr", required_argument, NULL, 'l' },
   };

   name = argv[0];
   debug = 0;
   entry = 0;
   load = 0;

   while ((ch = getopt_long(argc, argv, "de:l:", options, NULL)) != -1) {
      switch (ch) {
         case 'd':
            debug = 1;
            break;
         case 'e':
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
   ldrLoadBinary(argv[0], load);

   status = svInit();
   cpuSetRegister(CPU_REG_PC, entry);

   if (debug) {
      dbgEnable();
      dbgRequestEnter();
   }

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
