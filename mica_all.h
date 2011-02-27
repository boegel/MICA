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
ADDRINT returnArg(BOOL arg);
VOID all_instr_full_count_always();
VOID all_instr_full_count_for_hpc_alignment_no_rep();
VOID all_instr_full_count_for_hpc_alignment_with_rep(UINT32 repCnt);
VOID all_instr_intervals_count_always();
VOID all_instr_intervals_count_for_hpc_alignment_no_rep();
VOID all_instr_intervals_count_for_hpc_alignment_with_rep(UINT32 repCnt);
VOID instrument_all(INS ins, VOID* v, ins_buffer_entry* e);
