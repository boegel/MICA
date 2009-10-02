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

VOID init_all();
VOID all_instr_full_count();
VOID all_instr_intervals_count();
VOID instrument_all(INS ins, VOID* v, ins_buffer_entry* e);
