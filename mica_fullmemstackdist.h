/* 
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework. 
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

#include "mica.h"

void init_fullmemstackdist();
VOID instrument_fullmemstackdist(INS ins, VOID* v);
VOID fini_fullmemstackdist(INT32 code, VOID* v);

VOID fullmemstackdist_memRead(ADDRINT effMemAddr, ADDRINT size);
VOID fullmemstackdist_instr_interval_output();
VOID fullmemstackdist_instr_interval_reset();
