#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "memory.h"


typedef int t_dbgResult;
enum {
   DBG_RESULT_CONTINUE,
   DBG_RESULT_EXIT
};

typedef int t_dbgBreakpointId;
#define DBG_BREAKPOINT_INVALID ((t_dbgBreakpointId)-1)

typedef void *t_dbgEnumBreakpointState;
#define DBG_ENUM_BREAKPOINT_START ((t_dbgEnumBreakpointState)NULL)


int dbgEnable(void);
int dbgGetEnabled(void);
int dbgDisable(void);
void dbgRequestEnter(void);

t_dbgBreakpointId dbgAddBreakpoint(t_memAddress address);
void dbgRemoveBreakpoint(t_dbgBreakpointId brkId);
t_memAddress dbgGetBreakpoint(t_dbgBreakpointId brkId);
t_dbgEnumBreakpointState dbgEnumerateBreakpoints(t_dbgEnumBreakpointState state,
      t_dbgBreakpointId *outId, t_memAddress *outAddress);

t_dbgResult dbgTick(void);


#endif
