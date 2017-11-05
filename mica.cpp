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
 * implementation by Kenneth Hoste (Ghent University, Belgium), December 2006 - March 2011
 * based on code written by Lieven Eeckhout (for ATOM on Alpha)
 *
 * PLEASE DO NOT REDISTRIBUTE THIS CODE WITHOUT INFORMING THE AUTHORS.
 *
 * contact: kenneth.hoste@ugent.be , lieven.eeckhout@elis.ugent.be
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
#include "mica_memstackdist.h"

#include <sstream>
#include <iostream>
#include <iomanip>
#include <unistd.h>
using namespace std;

/* *** Variables *** */

/* global */
INT64 interval_size; // interval size chosen
INT64 interval_ins_count;
INT64 interval_ins_count_for_hpc_alignment;
INT64 total_ins_count;
INT64 total_ins_count_for_hpc_alignment;

ins_buffer_entry* ins_buffer[MAX_MEM_TABLE_ENTRIES];

/* ILP */
UINT32 _ilp_win_size;
char* _itypes_spec_file;

/* ILP, MEMFOOTPRINT, MEMSTACKDIST */
UINT32 _block_size;

/* MEMFOOTPRINT */
UINT32 _page_size;

/* for multiprocess binaries */
int append_pid;

/* helper */
int thread_count = 0;

/**********************************************
 *                    MAIN                    *
 **********************************************/

//FILE* _log;
ofstream _log;


/* append <pid>_pin.out to name if necessary */
const char *mkfilename(const char *name)
{
    char retx[100];
	if (append_pid){
        sprintf(retx,"%s_%d_pin.out",name,getpid());
    }
	else{
        sprintf(retx,"%s_pin.out",name);
    }
    char * x = (char*)malloc(sizeof(const char)*100);
    strcpy(x,retx);
	return (const char*)x;
}

// find buffer entry for instruction at given address in a hash table
ins_buffer_entry* findInsBufferEntry(ADDRINT a){

	ins_buffer_entry* e;
	INT64 key = a % MAX_MEM_TABLE_ENTRIES;

	e = ins_buffer[key];

	if(e != NULL){
		do{
			if(e->insAddr == a)
				break;
			e = e->next;
		} while(e->next != (ins_buffer_entry*)NULL);

		/* ins address not found, installing */
		if(e == NULL){
			e = (ins_buffer_entry*)checked_malloc(sizeof(ins_buffer_entry));
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
		e = (ins_buffer_entry*)checked_malloc(sizeof(ins_buffer_entry));
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
	if(interval_size == -1)	{
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_always, IARG_END);
	}
	else{
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_always, IARG_END);
	}

	ADDRINT insAddr = INS_Address(ins);
	ins_buffer_entry* e = findInsBufferEntry(insAddr);

	//instrument_ilp_all(ins, e);
	//instrument_itypes(ins, v);
	//instrument_ppm(ins, v);
	//instrument_reg(ins, e);
	//instrument_stride(ins, v);
	//instrument_memfootprint(ins, v);
	//instrument_memstackdist(ins, v);
	instrument_all(ins, v, e);
}

VOID Fini_all(INT32 code, VOID* v){
	fini_ilp_all(code, v);
	fini_itypes(code, v);
	fini_ppm(code, v);
	fini_reg(code, v);
	fini_stride(code, v);
	fini_memfootprint(code, v);
	fini_memstackdist(code, v);
}

/* ILP */
VOID Instruction_ilp_all_only(INS ins, VOID* v){
	if(interval_size == -1){
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_always, IARG_END);
	}
	else{
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_always, IARG_END);
	}

	ADDRINT insAddr = INS_Address(ins);

	ins_buffer_entry* e = findInsBufferEntry(insAddr);
	instrument_ilp_all(ins, e);
}

VOID Fini_ilp_all_only(INT32 code, VOID* v){
	fini_ilp_all(code, v);
}

/* ILP_ONE */
VOID Instruction_ilp_one_only(INS ins, VOID* v){
	if(interval_size == -1){
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_always, IARG_END);
	}
	else{
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_always, IARG_END);
	}

	ADDRINT insAddr = INS_Address(ins);

	ins_buffer_entry* e = findInsBufferEntry(insAddr);
	instrument_ilp_one(ins, e);
}

VOID Fini_ilp_one_only(INT32 code, VOID* v){
	fini_ilp_one(code, v);
}

/* ITYPES */
VOID Instruction_itypes_only(INS ins, VOID* v){
	if(interval_size == -1){
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_always, IARG_END);
	}
	else{
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_always, IARG_END);
	}

	instrument_itypes(ins, v);
}

VOID Fini_itypes_only(INT32 code, VOID* v){
	fini_itypes(code, v);
}

/* PPM */
VOID Instruction_ppm_only(INS ins, VOID* v){
	if(interval_size == -1){
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_always, IARG_END);
	}
	else{
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_always, IARG_END);
	}

	instrument_ppm(ins, v);
}

VOID Fini_ppm_only(INT32 code, VOID* v){
	fini_ppm(code, v);
}

/* REG */
VOID Instruction_reg_only(INS ins, VOID* v){
	if(interval_size == -1){
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_always, IARG_END);
	}
	else{
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_always, IARG_END);
	}

	ADDRINT insAddr = INS_Address(ins);

	ins_buffer_entry* e = findInsBufferEntry(insAddr);

	instrument_reg(ins, e);
}

VOID Fini_reg_only(INT32 code, VOID* v){
	fini_reg(code, v);
}

/* STRIDE */
VOID Instruction_stride_only(INS ins, VOID* v){
	if(interval_size == -1){
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_always, IARG_END);
	}
	else{
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_always, IARG_END);
	}

	instrument_stride(ins, v);
}

VOID Fini_stride_only(INT32 code, VOID* v){
	fini_stride(code, v);
}

/* MEMFOOTPRINT */
VOID Instruction_memfootprint_only(INS ins, VOID* v){
	if(interval_size == -1){
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_always, IARG_END);
	}
	else{
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_always, IARG_END);
	}

	instrument_memfootprint(ins, v);
}

VOID Fini_memfootprint_only(INT32 code, VOID* v){
	fini_memfootprint(code, v);
}

/* MEMSTACKDIST */
VOID Instruction_memstackdist_only(INS ins, VOID* v){
	if(interval_size == -1){
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_always, IARG_END);
	}
	else{
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_always, IARG_END);
	}

	instrument_memstackdist(ins, v);
}

VOID Fini_memstackdist_only(INT32 code, VOID* v){
	fini_memstackdist(code, v);
}

/* MY TYPE */
VOID Instruction_custom(INS ins, VOID* v){

	if(interval_size == -1){
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full_count_always, IARG_END);
	}
	else{
		if(INS_HasRealRep(ins)){
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
			INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_with_rep, IARG_REG_VALUE, INS_RepCountRegister(ins), IARG_END);
		}
		else{
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_for_hpc_alignment_no_rep, IARG_END);
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals_count_always, IARG_END);
	}

	cerr << "Please choose a subset of characteristics you want to use, and remove this message (along with the exit call)" << endl;
	exit(1);
	// Choose subset of characteristics, and make the same adjustments in Fini_custom and init_custom below

	//ADDRINT insAddr = INS_Address(ins);
	//ins_buffer_entry* e = findInsBufferEntry(insAddr);

	//instrument_ilp_all(ins, e);
	//instrument_ilp_one(ins, e);
	//instrument_itypes(ins, v);
	//instrument_ppm(ins, v);
	//instrument_reg(ins, e);
	//instrument_stride(ins, v);
	//instrument_memfootprint(ins, v);
	//instrument_memstackdist(ins, v);
}

VOID Fini_custom(INT32 code, VOID* v){
	//fini_ilp_all(code, v);
	//fini_ilp_one(code, v);
	//fini_itypes(code, v);
	//fini_ppm(code, v);
	//fini_reg(code, v);
	//fini_stride(code, v);
	//fini_memfootprint(code, v);
	//fini_memstackdist(code, v);
}

void init_custom(){
	//init_ilp_all();
	//init_ilp_one();
	//init_itypes();
	//init_ppm();
	//init_reg();
	//init_stride();
	//init_memfootprint();
	//init_memstackdist();
}


VOID ThreadStart(THREADID id, CONTEXT *context, INT32 flags, VOID *data)
{
	if (__sync_fetch_and_add(&thread_count, 1))
	{
		LOG_MSG("WARNING: Thread creation detected, results can be corrupted!\n");
		WARNING_MSG("Thread creation detected, results can be corrupted!");
	}
}


/************
 *   MAIN   *
 ************/
int main(int argc, char* argv[]){

	int i;
	MODE mode;

	setup_mica_log(&_log);

	read_config(&_log, &interval_size, &mode, &_ilp_win_size, &_block_size, &_page_size, &_itypes_spec_file, &append_pid);

	cerr << "interval_size: " << interval_size << ", mode: " << mode << endl;

	interval_ins_count = 0;
	interval_ins_count_for_hpc_alignment = 0;
	total_ins_count = 0;
	total_ins_count_for_hpc_alignment = 0;

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
		case MODE_MEMSTACKDIST:
			init_memstackdist();
			PIN_Init(argc, argv);
			INS_AddInstrumentFunction(Instruction_memstackdist_only, 0);
			PIN_AddFiniFunction(Fini_memstackdist_only, 0);
			break;
		case MODE_CUSTOM:
			init_custom();
			PIN_Init(argc, argv);
			INS_AddInstrumentFunction(Instruction_custom, 0);
			PIN_AddFiniFunction(Fini_custom, 0);
			break;
		default:
			cerr << "FATAL ERROR: Unknown mode while trying to allocate memory for Pin tool!" << endl;
			_log << "FATAL ERROR: Unknown mode while trying to allocate memory for Pin tool!" << endl;
			exit(1);
	}

	// The tool does not handle multithreaded programs.
	// Since results might be bogus, we print a warning
	// when presence of multiple threads is detected by PIN.
	PIN_AddThreadStartFunction(ThreadStart, NULL);

	// starts program, never returns
	PIN_StartProgram();
}
