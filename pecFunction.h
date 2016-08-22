#ifdef PEC_FUNCTION_H
#define PEC_FUNCTION_H
#else
#include "include/functions.h"
#include "cir.h"
#define UNUSED 1
DefFunction(PECFunction,UNUSED,UNUSED,0)
	CirHelper c;
	return c._objective(xs);
EndDef
#endif

