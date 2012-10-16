/* 
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework. 
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

#include "mica.h"

void init_memstackdist();
VOID instrument_memstackdist(INS ins, VOID* v);
VOID fini_memstackdist(INT32 code, VOID* v);

VOID memstackdist_memRead(ADDRINT effMemAddr, ADDRINT size);
VOID memstackdist_instr_interval_output();
VOID memstackdist_instr_interval_reset();
