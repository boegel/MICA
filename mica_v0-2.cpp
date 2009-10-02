/* Copyright (c) 2007, Kenneth Hoste and Lieven Eeckhout (Ghent University, Belgium)

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of the organization nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

/* 
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework. 
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

/*************************************************************************
 *                                                                       *
 *   MICA: Microarchitecture-Independent Characterization of Workloads   *
 *                                                                       *
 *************************************************************************
 *
 * implementation by Kenneth Hoste (Ghent University, Belgium), December 2006 - September 2007
 * based on code written by Lieven Eeckhout (for ATOM on Alpha)
 * 
 * PLEASE DO NOT REDISTRIBUTE THIS CODE WITHOUT INFORMING THE AUTHORS.
 *
 * contact: kenneth.hoste@elis.ugent.be , lieven.eeckhout@elis.ugent.be
 *
 */

/* MICA includes */
#include "mica.h"
#include "mica_init.h"
#include "mica_utils.h"

#include "mica_all.h"
#include "mica_ilp.h"
#include "mica_itypes.h"
#include "mica_ppm.h"
#include "mica_reg.h"
#include "mica_stride.h"
#include "mica_memfootprint.h"
#include "mica_memreusedist.h"

/* *** Variables *** */

/* global */
INT64 interval_size; // interval size chosen
INT64 interval_ins_count;
INT64 total_ins_count;

ins_buffer_entry* ins_buffer[MAX_MEM_TABLE_ENTRIES];

/* ILP */
UINT32 _win_size;

/**********************************************
 *                    MAIN                    *
 **********************************************/

FILE* log;

ins_buffer_entry* findInsBufferEntry(ADDRINT a){

	ins_buffer_entry* e;
	INT64 key = (INT32)(a % MAX_MEM_TABLE_ENTRIES);

	e = ins_buffer[key];

	if(e != (ins_buffer_entry*)NULL){
		do{
			if(e->insAddr == a)
				break;
			e = e->next;
		} while(e->next != (ins_buffer_entry*)NULL);

		/* ins address not found, installing */
		if(e == (ins_buffer_entry*)NULL){
			e = (ins_buffer_entry*)malloc(sizeof(ins_buffer_entry));
			e->insAddr = a;
			e->regReadCnt = 0;
			e->regsRead = NULL;
			e->regWriteCnt = 0;
			e->regsWritten = NULL;
			e->next = NULL;
			e->setRead = false;
			e->setWritten = false;
			e->setRegOpCnt = false;

			ins_buffer_entry* tmp = e = ins_buffer[key];
			while(tmp->next != (ins_buffer_entry*)NULL)
				tmp = tmp->next;
			tmp->next = e;
		}
	}
	else{
		/* new entry in hash table */
		e = (ins_buffer_entry*)malloc(sizeof(ins_buffer_entry));
		e->insAddr = a;
		e->regOpCnt = 0;
		e->regReadCnt = 0;
		e->regsRead = NULL;
		e->regWriteCnt = 0;
		e->regsWritten = NULL;
		e->next = NULL;
		e->setRead = false;
		e->setWritten = false;
	}
	
	return e;
}

/* ALL */
VOID Instruction_all(INS ins, VOID* v){
	if(interval_size == -1)	
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count,IARG_END);
	else
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count, IARG_END);

	ADDRINT insAddr = INS_Address(ins);
	ins_buffer_entry* e = findInsBufferEntry(insAddr);

	//instrument_ilp_all(ins, e);
	//instrument_itypes(ins, v);
	//instrument_ppm(ins, v);
	//instrument_reg(ins, e);
	//instrument_stride(ins, v);
	//instrument_memfootprint(ins, v);
	//instrument_memreusedist(ins, v);
	instrument_all(ins, v, e);
}

VOID Fini_all(INT32 code, VOID* v){
	fini_ilp_all(code, v);
	fini_itypes(code, v);
	fini_ppm(code, v);
	fini_reg(code, v);
	fini_stride(code, v);
	fini_memfootprint(code, v);
	fini_memreusedist(code, v);
}

/* ILP */
VOID Instruction_ilp_all_only(INS ins, VOID* v){
	if(interval_size == -1)	
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count, IARG_END);
	else
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count, IARG_END);
	ADDRINT insAddr = INS_Address(ins);

	ins_buffer_entry* e = findInsBufferEntry(insAddr);
	instrument_ilp_all(ins, e);
}

VOID Fini_ilp_all_only(INT32 code, VOID* v){
	fini_ilp_all(code, v);
}

/* ILP_ONE */
VOID Instruction_ilp_one_only(INS ins, VOID* v){
	if(interval_size == -1)	
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count, IARG_END);
	else
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count, IARG_END);

	ADDRINT insAddr = INS_Address(ins);

	ins_buffer_entry* e = findInsBufferEntry(insAddr);
	instrument_ilp_one(ins, e);
}

VOID Fini_ilp_one_only(INT32 code, VOID* v){
	fini_ilp_one(code, v);
}

/* ITYPES */
VOID Instruction_itypes_only(INS ins, VOID* v){
	if(interval_size == -1)	
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count, IARG_END);
	else
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count, IARG_END);
	instrument_itypes(ins, v);
}

VOID Fini_itypes_only(INT32 code, VOID* v){
	fini_itypes(code, v);
}

/* PPM */
VOID Instruction_ppm_only(INS ins, VOID* v){
	if(interval_size == -1)	
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count, IARG_END);
	else
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count, IARG_END);
	instrument_ppm(ins, v);
}

VOID Fini_ppm_only(INT32 code, VOID* v){
	fini_ppm(code, v);
}

/* REG */
VOID Instruction_reg_only(INS ins, VOID* v){
	if(interval_size == -1)	
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count, IARG_END);
	else
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count, IARG_END);

	ADDRINT insAddr = INS_Address(ins);

	ins_buffer_entry* e = findInsBufferEntry(insAddr);

	instrument_reg(ins, e);
}

VOID Fini_reg_only(INT32 code, VOID* v){
	fini_reg(code, v);
}

/* STRIDE */
VOID Instruction_stride_only(INS ins, VOID* v){
	if(interval_size == -1)	
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count, IARG_END);
	else
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count, IARG_END);
	instrument_stride(ins, v);
}

VOID Fini_stride_only(INT32 code, VOID* v){
	fini_stride(code, v);
}

/* MEMFOOTPRINT */
VOID Instruction_memfootprint_only(INS ins, VOID* v){
	if(interval_size == -1)	
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count, IARG_END);
	else
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count, IARG_END);
	instrument_memfootprint(ins, v);
}

VOID Fini_memfootprint_only(INT32 code, VOID* v){
	fini_memfootprint(code, v);
}

/* MEMREUSEDIST */
VOID Instruction_memreusedist_only(INS ins, VOID* v){
	if(interval_size == -1)	
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count, IARG_END);
	else
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count, IARG_END);
	instrument_memreusedist(ins, v);
}

VOID Fini_memreusedist_only(INT32 code, VOID* v){
	fini_memreusedist(code, v);
}

/* MY TYPE */
VOID Instruction_mytype(INS ins, VOID* v){

	if(interval_size == -1)	
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count, IARG_END);
	else
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count, IARG_END);

	//fprintf(stderr,"Please choose a subset of characteristics you want to use, and remove this message (along with the exit call)\n");
	//exit(1);
	// Choose subset of characteristics, and make the same adjustments in Fini_mytype and init_mytype below
	
	//ADDRINT insAddr = INS_Address(ins);
	//ins_buffer_entry* e = findInsBufferEntry(insAddr);

	//instrument_ilp_all(ins, e);
	//instrument_ilp_one(ins, e);
	//instrument_itypes(ins, v);
	//instrument_ppm(ins, v);
	//instrument_reg(ins, e);
	//instrument_stride(ins, v);
	//instrument_memfootprint(ins, v);
	//instrument_memreusedist(ins, v);
}

VOID Fini_mytype(INT32 code, VOID* v){
	//fini_ilp_all(code, v);
	//fini_ilp_one(code, v);
	//fini_itypes(code, v);
	//fini_ppm(code, v);
	//fini_reg(code, v);
	//fini_stride(code, v);
	//fini_memfootprint(code, v);
	//fini_memreusedist(code, v);
}

void init_mytype(){
	//init_ilp_all();
	//init_ilp_one();
	//init_itypes();
	//init_ppm();
	//init_reg();
	//init_stride();
	//init_memfootprint();
	//init_memreusedist();
}

/************
 *   MAIN   *
 ************/
int main(int argc, char* argv[]){

	int i;
	MODE mode;

	setup_mica_log(&log);

	read_config(log, &interval_size, &mode, &_win_size);

	DEBUG_MSG("interval_size: %lld, mode: %d\n", interval_size, mode);
	
	interval_ins_count = 0;
	total_ins_count = 0;

	for(i=0; i < MAX_MEM_TABLE_ENTRIES; i++){
		ins_buffer[i] = (ins_buffer_entry*)NULL;
	}

	switch(mode){
		case MODE_ALL:
			init_all();
			PIN_Init(argc, argv);
			INS_AddInstrumentFunction(Instruction_all, 0);
    			PIN_AddFiniFunction(Fini_all, 0);
			break;
		case MODE_ILP:
			init_ilp_all();
			PIN_Init(argc, argv);
			INS_AddInstrumentFunction(Instruction_ilp_all_only, 0);
    			PIN_AddFiniFunction(Fini_ilp_all_only, 0);
			break;
		case MODE_ILP_ONE:
			init_ilp_one();
			PIN_Init(argc, argv);
			INS_AddInstrumentFunction(Instruction_ilp_one_only, 0);
    			PIN_AddFiniFunction(Fini_ilp_one_only, 0);
			break;
		case MODE_ITYPES:
			init_itypes();
			PIN_Init(argc, argv);
			INS_AddInstrumentFunction(Instruction_itypes_only, 0);
    			PIN_AddFiniFunction(Fini_itypes_only, 0);
			break;
		case MODE_PPM:
			init_ppm();
			PIN_Init(argc, argv);
			INS_AddInstrumentFunction(Instruction_ppm_only, 0);
    			PIN_AddFiniFunction(Fini_ppm_only, 0);
			break;
		case MODE_REG:
			init_reg();
			PIN_Init(argc, argv);
			INS_AddInstrumentFunction(Instruction_reg_only, 0);
    			PIN_AddFiniFunction(Fini_reg_only, 0);
			break;
		case MODE_STRIDE:
			init_stride();
			PIN_Init(argc, argv);
			INS_AddInstrumentFunction(Instruction_stride_only, 0);
    			PIN_AddFiniFunction(Fini_stride_only, 0);
			break;
		case MODE_MEMFOOTPRINT:
			init_memfootprint();
			PIN_Init(argc, argv);
			INS_AddInstrumentFunction(Instruction_memfootprint_only, 0);
    			PIN_AddFiniFunction(Fini_memfootprint_only, 0);
			break;
		case MODE_MEMREUSEDIST:
			init_memreusedist();
			PIN_Init(argc, argv);
			INS_AddInstrumentFunction(Instruction_memreusedist_only, 0);
    			PIN_AddFiniFunction(Fini_memreusedist_only, 0);
			break;
		case MODE_MYTYPE:
			init_mytype();
			PIN_Init(argc, argv);
			INS_AddInstrumentFunction(Instruction_mytype, 0);
    			PIN_AddFiniFunction(Fini_mytype, 0);
			break;
		default:
			ERROR("FATAL ERROR: Unknown mode while trying to allocate memory for Pin tool!\n");
			LOG_MSG("FATAL ERROR: Unknown mode while trying to allocate memory for Pin tool!\n");
			exit(1);
	}

	// starts program, never returns
	PIN_StartProgram();
}
