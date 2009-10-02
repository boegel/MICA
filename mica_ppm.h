/* 
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework. 
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

#include "mica.h"

void init_ppm();
VOID instrument_ppm(INS ins, VOID* v);
VOID fini_ppm(INT32 code, VOID* v);

VOID instrument_ppm_cond_br(INS ins);
VOID ppm_instr_interval_output();
VOID ppm_instr_interval_reset();
