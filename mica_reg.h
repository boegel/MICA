/* 
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework. 
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

#include "mica.h"
#include "mica_utils.h"

void init_reg();
VOID instrument_reg(INS ins, ins_buffer_entry* e);
VOID fini_reg(INT32 code, VOID* v);

VOID reg_instr_full(VOID* _e);
ADDRINT reg_instr_intervals(VOID* _e);
VOID reg_instr_interval_output();
VOID reg_instr_interval_reset();

