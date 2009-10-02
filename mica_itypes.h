/* 
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework. 
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

#include "mica.h"

void init_itypes();
VOID instrument_itypes(INS ins, VOID* v);
VOID instrument_itypes_bbl(TRACE trace, VOID* v);
VOID fini_itypes(INT32 code, VOID* v);


VOID itypes_count_mem_read();
VOID itypes_count_mem_write();
VOID itypes_count_control();
VOID itypes_count_arith();
VOID itypes_count_fp();
VOID itypes_count_stack();
VOID itypes_count_shift();
VOID itypes_count_string();
VOID itypes_count_sse();
VOID itypes_count_other();
VOID itypes_count_nop();

VOID itypes_instr_interval_output();
VOID itypes_instr_interval_reset();
