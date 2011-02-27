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

void init_ilp_all();
void init_ilp_one();

VOID instrument_ilp_all(INS ins, ins_buffer_entry* e);
VOID instrument_ilp_one(INS ins, ins_buffer_entry* e);

VOID fini_ilp_all(INT32 code, VOID* v);
VOID fini_ilp_one(INT32 code, VOID* v);

/* support for fast instrumentation of all characteristics in a single run (avoid multiple InsertCalls!) */
//void ilp_buffer_instruction_only(void* _e);
VOID PIN_FAST_ANALYSIS_CALL ilp_buffer_instruction_only(void* _e);
//void ilp_buffer_instruction_read(ADDRINT read1_addr, ADDRINT read_size);
VOID PIN_FAST_ANALYSIS_CALL ilp_buffer_instruction_read(ADDRINT read1_addr, ADDRINT read_size);
//void ilp_buffer_instruction_read2(ADDRINT read2_addr);
VOID PIN_FAST_ANALYSIS_CALL ilp_buffer_instruction_read2(ADDRINT read2_addr);
//void ilp_buffer_instruction_write(ADDRINT write_addr, ADDRINT write_size);
VOID PIN_FAST_ANALYSIS_CALL ilp_buffer_instruction_write(ADDRINT write_addr, ADDRINT write_size);
ADDRINT ilp_buffer_instruction_next();
/*ADDRINT ilp_buffer_instruction_2reads_write(void* _e, ADDRINT read1_addr, ADDRINT read2_addr, ADDRINT read_size, ADDRINT write_addr, ADDRINT write_size);
ADDRINT ilp_buffer_instruction_read_write(void* _e, ADDRINT read1_addr, ADDRINT read_size, ADDRINT write_addr, ADDRINT write_size);
ADDRINT ilp_buffer_instruction_2reads(void* _e, ADDRINT read1_addr, ADDRINT read2_addr, ADDRINT read_size);
ADDRINT ilp_buffer_instruction_read(void* _e, ADDRINT read1_addr, ADDRINT read_size);
ADDRINT ilp_buffer_instruction_write(void* _e, ADDRINT write_addr, ADDRINT write_size);
ADDRINT ilp_buffer_instruction(void* _e);*/
VOID empty_ilp_buffer_all(); 
