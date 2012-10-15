/* 
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework. 
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

#include "mica.h"

void init_memreusedist2();
VOID instrument_memreusedist2(INS ins, VOID* v);
VOID fini_memreusedist2(INT32 code, VOID* v);

VOID memreusedist2_memRead(ADDRINT effMemAddr, ADDRINT size);
VOID memreusedist2_instr_interval_output();
VOID memreusedist2_instr_interval_reset();
