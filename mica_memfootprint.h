/* 
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework. 
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

#include "mica.h"

void init_memfootprint();
VOID instrument_memfootprint(INS ins, VOID* v);
VOID fini_memfootprint(INT32 code, VOID* v);

VOID memOp(ADDRINT effMemAddr, ADDRINT size);
VOID instrMem(ADDRINT instrAddr, ADDRINT size);

VOID memfootprint_instr_interval_output();
VOID memfootprint_instr_interval_reset();
