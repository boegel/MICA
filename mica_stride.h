/* 
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework. 
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

#include "mica.h"

void init_stride();
VOID instrument_stride(INS ins, VOID* v);
VOID fini_stride(INT32 code, VOID* v);

UINT32 stride_index_memRead1(ADDRINT a);
UINT32 stride_index_memRead2(ADDRINT a);
UINT32 stride_index_memWrite(ADDRINT a);

VOID readMem_stride(UINT32 index, ADDRINT effAddr, ADDRINT size);
VOID writeMem_stride(UINT32 index, ADDRINT effAdrr, ADDRINT size);

VOID stride_instr_interval_output();
VOID stride_instr_interval_reset();
