#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include "cpu.h"
#include "debugger.h"


typedef struct dbgBreakpoint {
   struct dbgBreakpoint *next;
   t_dbgBreakpointId id;
   t_memAddress address;
} t_dbgBreakpoint;

t_dbgBreakpoint *dbgBreakpointList = NULL;

t_dbgBreakpointId dbgLastBreakpointID = 0;

int dbgEnabled = 0;
int dbgUserRequestsEnter = 0;
int dbgStepInEnabled = 0;
int dbgStepOverEnabled = 0;
t_memAddress dbgStepOverAddr;


int dbgEnable(void)
{
   int oldEnable = dbgEnabled;
   dbgEnabled = 1;
   return oldEnable;
}


int dbgGetEnabled(void)
{
   return dbgEnabled;
}


int dbgDisable(void)
{
   int oldEnable = dbgEnabled;
   dbgEnabled = 0;
   return oldEnable;
}


void dbgRequestEnter(void)
{
   dbgUserRequestsEnter = 1;
}


int dbgPrintf(const char *format, ...)
{
   va_list args;
   int res;

   if (!dbgEnabled)
      return 0;

   va_start(args, format);
   res = vfprintf(stderr, format, args);
   va_end(args);
   return res;
}


t_dbgBreakpointId dbgAddBreakpoint(t_memAddress address)
{
   t_dbgBreakpoint *bp = calloc(1, sizeof(t_dbgBreakpoint));
   bp->next = dbgBreakpointList;
   bp->id = dbgLastBreakpointID++;
   bp->address = address;
   dbgBreakpointList = bp;
   return bp->id;
}


int dbgRemoveBreakpoint(t_dbgBreakpointId brkId)
{
   t_dbgBreakpoint *prev = NULL;
   t_dbgBreakpoint *cur = dbgBreakpointList;
   while (cur && cur->id != brkId) {
      prev = cur;
      cur = cur->next;
   }
   if (!cur)
      return 1;
   if (prev) {
      prev->next = cur->next;
   } else {
      dbgBreakpointList = cur->next;
   }
   free(cur);
   return 0;
}


t_memAddress dbgGetBreakpoint(t_dbgBreakpointId brkId)
{
   t_dbgBreakpoint *cur = dbgBreakpointList;
   while (cur && cur->id != brkId)
      cur = cur->next;
   if (cur)
      return cur->address;
   return 0;
}


t_dbgEnumBreakpointState dbgEnumerateBreakpoints(t_dbgEnumBreakpointState state,
      t_dbgBreakpointId *outId, t_memAddress *outAddress)
{
   t_dbgBreakpoint *xstate = (t_dbgBreakpoint *)state;
   t_dbgBreakpoint *cur;

   if (!xstate) {
      cur = dbgBreakpointList;
   } else {
      cur = xstate->next;
   }
   
   if (cur) {
      if (outId)
         *outId = cur->id;
      if (outAddress)
         *outAddress = cur->address;
   }
   
   return (t_dbgEnumBreakpointState)cur;
}


typedef int t_dbgTrigType;
enum {
   DBG_TRIG_NONE = 0,
   DBG_TRIG_TYPE_BREAKP,
   DBG_TRIG_TYPE_STEPIN,
   DBG_TRIG_TYPE_STEPOVER,
   DBG_TRIG_TYPE_USER
};

t_dbgTrigType dbgCheckTrigger(t_dbgBreakpointId *outId)
{
   t_memAddress curPc;
   t_memAddress bpAddr;
   t_dbgEnumBreakpointState state;

   if (!dbgEnabled)
      return DBG_TRIG_NONE;

   if (dbgUserRequestsEnter)
      return DBG_TRIG_TYPE_USER;
   
   if (dbgStepInEnabled)
      return DBG_TRIG_TYPE_STEPIN;

   curPc = cpuGetRegister(CPU_REG_PC);
   if (dbgStepOverEnabled && dbgStepOverAddr == curPc)
      return DBG_TRIG_TYPE_STEPOVER;
   
   state = dbgEnumerateBreakpoints(DBG_ENUM_BREAKPOINT_START, outId, &bpAddr);
   while (state != DBG_ENUM_BREAKPOINT_STOP) {
      if (bpAddr == curPc)
         break;
      state = dbgEnumerateBreakpoints(state, outId, &bpAddr);
   }

   if (state)
      return DBG_TRIG_TYPE_BREAKP;
   return DBG_TRIG_NONE;
}


void dbgParserSkipWhitespace(char **in)
{
   while (**in != '\0' && isspace(**in))
      (*in)++;
}

int dbgParserAcceptKeyword(const char *word, char **in)
{
   char *p;
   dbgParserSkipWhitespace(in);
   
   p = *in;
   while (*word != '\0' && *word == *p) {
      word++;
      p++;
   }
   if (*word != '\0')
      return 0;
   *in = p;
   return 1;
}

typedef int t_dbgInterfaceStatus;
enum {
   DBG_IF_CONT_DEBUG,
   DBG_IF_STOP_DEBUG,
   DBG_IF_EXIT
};

void dbgCmdHelp(void);
void dbgCmdStepOver(void);
void dbgCmdAddBreakpoint(char *args);
void dbgCmdRemoveBreakpoint(char *args);
void dbgCmdPrintBreakpoints(void);
void dbgCmdPrintCpuStatus(void);
void dbgCmdDisassemble(char *args);
void dbgCmdMemDump(char *args);

t_dbgResult dbgInterface(void)
{
   char input[80];
   char *nextTok;

   fprintf(stderr, "debug> ");
   fflush(stderr);
   if (fgets(input, 80, stdin) == NULL)
      return DBG_IF_EXIT;
   
   nextTok = input;
   if (dbgParserAcceptKeyword("q", &nextTok)) {
      return DBG_IF_EXIT;
   } else if (dbgParserAcceptKeyword("c", &nextTok)) {
      return DBG_IF_STOP_DEBUG;
   } else if (dbgParserAcceptKeyword("s", &nextTok)) {
      dbgStepInEnabled = 1;
      return DBG_IF_STOP_DEBUG;
   } else if (dbgParserAcceptKeyword("n", &nextTok)) {
      dbgCmdStepOver();
      return DBG_IF_STOP_DEBUG;
   } else if (dbgParserAcceptKeyword("bl", &nextTok)) {
      dbgCmdPrintBreakpoints();
   } else if (dbgParserAcceptKeyword("br", &nextTok)) {
      dbgCmdRemoveBreakpoint(nextTok);
   } else if (dbgParserAcceptKeyword("b", &nextTok)) {
      dbgCmdAddBreakpoint(nextTok);
   } else if (dbgParserAcceptKeyword("v", &nextTok)) {
      dbgCmdPrintCpuStatus();
   } else if (dbgParserAcceptKeyword("u", &nextTok)) {
      dbgCmdDisassemble(nextTok);
   } else if (dbgParserAcceptKeyword("d", &nextTok)) {
      dbgCmdMemDump(nextTok);
   } else if (*nextTok != '\0') {
      dbgCmdHelp();
   }

   return DBG_IF_CONT_DEBUG;
}

void dbgCmdHelp(void)
{
   puts("Debugger commands:");
   puts("q               Exit the simulator");
   puts("c               Exit the debugger and continue (up to the next");
   puts("                  breakpoint if any)");
   puts("s               Step in");
   puts("n               Step over");
   puts("b <address>     Add a breakpoint at the specified address");
   puts("bl              List all breakpoints");
   puts("br <id>         Remove breakpoint number <id>");
   puts("v               Print current CPU state");
   puts("u <start> <len> Disassemble 'len' instructions from address 'start'");
   puts("d <start> <len> Dump 'len' bytes from address 'start'");
}

void dbgCmdStepOver(void)
{
   t_cpuRegValue pc;
   t_isaUInt inst;

   pc = cpuGetRegister(CPU_REG_PC);
   inst = memDebugRead32(pc, NULL);
   if ((ISA_INST_OPCODE(inst) == ISA_INST_OPCODE_JAL || 
         (ISA_INST_OPCODE(inst) == ISA_INST_OPCODE_JALR && 
            ISA_INST_FUNCT3(inst) == 0)) &&
         ISA_INST_RD(inst) == CPU_REG_RA) {
      /* the instruction is presumably a subroutine call */
      dbgStepOverEnabled = 1;
      dbgStepOverAddr = pc + 4;
   } else {
      dbgStepInEnabled = 1;
   }
}

void dbgCmdAddBreakpoint(char *args)
{
   unsigned long addr;
   char *arg2;
   t_dbgBreakpointId id;

   addr = strtoul(args, &arg2, 0);
   if (args == arg2) {
      fprintf(stderr, "First argument is not a valid number\n");
      return;
   }

   id = dbgAddBreakpoint((t_memAddress)addr);
   fprintf(stderr, "Added breakpoint %d at address 0x%08lx\n", id, addr);
}

void dbgCmdRemoveBreakpoint(char *args)
{
   unsigned long bpid;
   char *arg2;
   int error;

   bpid = strtoul(args, &arg2, 0);
   if (args == arg2) {
      fprintf(stderr, "First argument is not a valid number\n");
      return;
   }

   error = dbgRemoveBreakpoint((t_dbgBreakpointId)bpid);
   if (error == 0)
      fprintf(stderr, "Removed breakpoint %lu\n", bpid);
   else
      fprintf(stderr, "Breakpoint %lu not found\n", bpid);
}

void dbgCmdPrintBreakpoints(void)
{
   t_dbgEnumBreakpointState enumState;
   t_dbgBreakpointId id;
   t_memAddress addr;

   enumState = dbgEnumerateBreakpoints(DBG_ENUM_BREAKPOINT_START, &id, &addr);
   if (enumState == DBG_ENUM_BREAKPOINT_STOP) {
      fprintf(stderr, "No breakpoints defined\n");
   } else {
      while (enumState != DBG_ENUM_BREAKPOINT_STOP) {
         fprintf(stderr, "Breakpoint %-8d Address 0x%08x\n", id, addr);
         enumState = dbgEnumerateBreakpoints(enumState, &id, &addr);
      }
   }
}

void dbgCmdPrintCpuStatus(void)
{
   t_cpuRegID r;
   t_cpuRegValue pc;
   t_isaUInt inst;
   char buffer[80];

   pc = cpuGetRegister(CPU_REG_PC);
   inst = memDebugRead32(pc, NULL);
   isaDisassemble(inst, buffer, 80);
   fprintf(stderr, "PC : %08x: %08x %s\n", pc, inst, buffer);

   for (r=CPU_REG_X0; r<=CPU_REG_X31; r++) {
      fprintf(stderr, "X%-2d: %08x", r, cpuGetRegister(r));
      if ((r+1) % 4 == 0)
         fputc('\n', stderr);
      else
         fputc(' ', stderr);
   }
}

void dbgCmdDisassemble(char *args)
{
   unsigned long addr, len, i;
   char buffer[80];
   char *arg2, *arg3;

   addr = strtoul(args, &arg2, 0);
   if (args == arg2) {
      fprintf(stderr, "First argument is not a valid number\n");
      return;
   }
   len = strtoul(arg2, &arg3, 0);
   if (arg2 == arg3) {
      fprintf(stderr, "Second argument is not a valid number\n");
      return;
   }

   for (i=0; i<len; i++) {
      t_memAddress curaddr = addr + 4*i;
      uint32_t instr = memDebugRead32(curaddr, NULL);
      isaDisassemble(instr, buffer, 80);
      fprintf(stderr, "%08"PRIx32":  %08"PRIx32"  %s\n", 
            curaddr, instr, buffer);
   }

   return;
}

void dbgCmdMemDump(char *args)
{
   unsigned long addr, len, i;
   char buffer[80];
   char *arg2, *arg3;

   addr = strtoul(args, &arg2, 0);
   if (args == arg2) {
      fprintf(stderr, "First argument is not a valid number\n");
      return;
   }
   len = strtoul(arg2, &arg3, 0);
   if (arg2 == arg3) {
      fprintf(stderr, "Second argument is not a valid number\n");
      return;
   }

   if (len > 0) {
      fprintf(stderr, "%08"PRIx32": ", (t_memAddress)addr);
      for (i=0; i<len; i++) {
         t_memAddress curaddr = addr + i;
         uint8_t byte = memDebugRead8(curaddr, NULL);
         fprintf(stderr, "%02"PRIx8, byte);
         if ((i+1) % 16 == 0 || (i+1) == len)
            fputc('\n', stderr);
         else
            fputc(' ', stderr);
         if ((i+1) % 16 == 0 && (i+1) < len)
            fprintf(stderr, "%08"PRIx32": ", curaddr+1);
      }
   } else
      fprintf(stderr, "Length is zero\n");

   return;
}


t_dbgResult dbgTick(void)
{
   t_dbgBreakpointId bpId;
   t_dbgTrigType bpTrig;
   t_dbgResult dbgRes;

   bpTrig = dbgCheckTrigger(&bpId);
   if (bpTrig == DBG_TRIG_NONE)
      return DBG_RESULT_CONTINUE;
   
   if (bpTrig == DBG_TRIG_TYPE_BREAKP) {
      fprintf(stderr, "Stopped at breakpoint #%d (PC=0x%08x)\n", bpId, 
            dbgGetBreakpoint(bpId));
   }
   
   dbgStepInEnabled = 0;
   dbgStepOverEnabled = 0;
   dbgUserRequestsEnter = 0;

   dbgCmdPrintCpuStatus();

   do {
      dbgRes = dbgInterface();
   } while (dbgRes == DBG_IF_CONT_DEBUG);
   
   if (dbgRes == DBG_IF_STOP_DEBUG)
      return DBG_RESULT_CONTINUE;
   return DBG_RESULT_EXIT;
}
