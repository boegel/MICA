/* 
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework. 
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

#include "mica.h"

void init_memreusedist();
VOID instrument_memreusedist(INS ins, VOID* v);
VOID fini_memreusedist(INT32 code, VOID* v);

VOID memreusedist_memRead(ADDRINT effMemAddr, ADDRINT size);
VOID memreusedist_instr_interval_output();
VOID memreusedist_instr_interval_reset();
