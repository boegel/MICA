/*
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework.
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

#include "pin.H"

/* MICA includes */
#include "mica_utils.h"
#include "mica_ilp.h"

#include <sstream>
#include <iostream>
using namespace std;

#define ILP_WIN_SIZE_CNT 4

const UINT32 win_sizes[ILP_WIN_SIZE_CNT] = {32, 64, 128, 256};

extern UINT32 _ilp_win_size;
UINT32 win_size;

extern UINT32 _block_size;
UINT32 ilp_block_size;

/* buffer settings */

//#define ILP_BUFFER_SIZE 256
#define ILP_BUFFER_SIZE 200

/* buffer variables */

typedef struct ilp_buffer_entry_type{

	ins_buffer_entry* e;

	ADDRINT mem_read1_addr;
	ADDRINT mem_read2_addr;
	ADDRINT mem_read_size;

	ADDRINT mem_write_addr;
	ADDRINT mem_write_size;

} ilp_buffer_entry;

ilp_buffer_entry* ilp_buffer[ILP_BUFFER_SIZE];
UINT32 ilp_buffer_index;

void init_ilp_buffering();
VOID fini_ilp_buffering_all();
VOID fini_ilp_buffering_one();

/* Global variables */

extern INT64 interval_size;
extern INT64 interval_ins_count;
extern INT64 interval_ins_count_for_hpc_alignment;
extern INT64 total_ins_count;
extern INT64 total_ins_count_for_hpc_alignment;
ofstream output_file_ilp_one;
ofstream output_file_ilp_all;

INT32 size_pow_all_times_all;
INT64 index_all_times_all;
UINT64* all_times_all[ILP_WIN_SIZE_CNT];

INT32 size_pow_times;
INT64 index_all_times;
UINT64* all_times;

INT64 cpuClock_interval_all[ILP_WIN_SIZE_CNT];
UINT64 timeAvailable_all[ILP_WIN_SIZE_CNT][MAX_NUM_REGS];
nlist* memAddressesTable_all[MAX_MEM_TABLE_ENTRIES];
UINT32 windowHead_all[ILP_WIN_SIZE_CNT];
UINT32 windowTail_all[ILP_WIN_SIZE_CNT];
UINT64 cpuClock_all[ILP_WIN_SIZE_CNT];
UINT64* executionProfile_all[ILP_WIN_SIZE_CNT];
UINT64 issueTime_all[ILP_WIN_SIZE_CNT];

INT64 cpuClock_interval;
UINT64 timeAvailable[MAX_NUM_REGS];
nlist* memAddressesTable[MAX_MEM_TABLE_ENTRIES];
UINT32 windowHead;
UINT32 windowTail;
UINT64 cpuClock;
UINT64* executionProfile;
UINT64 issueTime;

/*************************
      ILP (COMMON)
**************************/

/* initializing */
void init_ilp_common(){
	/* initializing total instruction counts is done in mica.cpp */
}

/************************************
     ILP (one given window size)
*************************************/

/* initializing */
void init_ilp_one(){

	UINT32 i;

	init_ilp_common();
	init_ilp_buffering();

	win_size = _ilp_win_size;
	ilp_block_size = _block_size;

	size_pow_times = 10;
	all_times = (UINT64*)checked_malloc((1 << size_pow_times) * sizeof(UINT64));
	index_all_times = 1; // don't use first element of all_times

	windowHead = 0;
	windowTail = 0;
	cpuClock = 0;
	cpuClock_interval = 0;
	for(i = 0; i < MAX_NUM_REGS; i++){
		timeAvailable[i] = 0;
	}

	executionProfile = (UINT64*)checked_malloc(win_size*sizeof(UINT64));

	for(i = 0; i < win_size; i++){
		executionProfile[i] = 0;
	}
	issueTime = 0;

	if(interval_size != -1){
		if(interval_size % ILP_BUFFER_SIZE != 0){
			cerr << "ERROR! Interval size is not a multiple of ILP buffer size. (" << interval_size << " vs " << ILP_BUFFER_SIZE << ")" << endl;
			exit(-1);
		}
		char filename[100];
        sprintf(filename, "ilp-win%d_phases_int", win_size);
		output_file_ilp_one.open(mkfilename(filename), ios::out|ios::trunc);
		output_file_ilp_one.close();
	}
}

/* support */
void increase_size_all_times_one(){
	UINT64* ptr;

	size_pow_times++;

	ptr = (UINT64*)realloc(all_times, (1 << size_pow_times)*sizeof(UINT64));
	if(ptr == (UINT64*)NULL){
		cerr << "Could not allocate memory (realloc)!" << endl;
		exit(1);
	}
	all_times = ptr;
}

/* per-instruction stuff */
VOID ilp_instr_one(){

	const UINT32 win_size_const = win_size;
	UINT32 reordered;

	/* set issue time for tail of instruction window */
	executionProfile[windowTail] = issueTime;
	windowTail = (windowTail + 1) % win_size_const;

	/* if instruction window (issue buffer) full */
	if(windowHead == windowTail){
		cpuClock++;
		cpuClock_interval++;
		reordered = 0;
		/* remove all instructions which are done from beginning of window,
		 * until an instruction comes along which is not ready yet:
		 * -> check executionProfile to see which instructions are done
		 * -> commit maximum win_size instructions (i.e. stop when issue buffer is empty)
		 */
		while((executionProfile[windowHead] < cpuClock) && (reordered < win_size_const)) {
			windowHead = (windowHead + 1) % win_size_const;
			reordered++;
		}
		//assert(reordered != 0);
	}

	/* reset issue times */
	issueTime = 0;
}

VOID ilp_instr_full_one(){

	/* counting instructions is done in all_instr_full() */

	ilp_instr_one();
}

VOID ilp_instr_intervals_one(){

	int i;

	/* counting instructions is done in all_instr_intervals() */

	ilp_instr_one();

	if(interval_ins_count_for_hpc_alignment == interval_size){

        char filename[100];
        sprintf(filename, "ilp-win%d_phases_int", win_size);

		output_file_ilp_one.open(mkfilename(filename), ios::out|ios::app);

		output_file_ilp_one << interval_size << " " << cpuClock_interval << endl;

		/* reset */
		interval_ins_count = 0;
		interval_ins_count_for_hpc_alignment = 0;

		cpuClock_interval = 0;

		/* clean up memory used, to avoid memory problems for long (CPU2006) benchmarks */
		size_pow_times = 10;

		free(all_times);
		all_times = (UINT64*)checked_malloc((1 << size_pow_times) * sizeof(UINT64));
		index_all_times = 1;

		nlist* np;
		nlist* np_rm;
		for(i=0; i < MAX_MEM_TABLE_ENTRIES; i++){
			np = memAddressesTable[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm->mem);
				free(np_rm);
			}
			memAddressesTable[i] = (nlist*) NULL;
		}

		output_file_ilp_one.close();
	}
}

VOID checkIssueTime_one(){

	if(cpuClock > issueTime)
		issueTime = cpuClock;
}

/* register stuff */
VOID readRegOp_ilp_one(UINT32 regId){

	if(timeAvailable[regId] > issueTime)
		issueTime = timeAvailable[regId];
}

VOID readRegOp_ilp_one_fast(VOID* _e){

	ins_buffer_entry* e = (ins_buffer_entry*)_e;

	INT32 i;

	UINT32 regId;

	for(i=0; i < e->regReadCnt; i++){
		regId = (UINT32)e->regsRead[i];
		if(timeAvailable[regId] > issueTime)
			issueTime = timeAvailable[regId];
	}
}

VOID writeRegOp_ilp_one(UINT32 regId){

	timeAvailable[regId] = issueTime + 1;
}

VOID writeRegOp_ilp_one_fast(VOID* _e){

	ins_buffer_entry* e = (ins_buffer_entry*)_e;

	INT32 i;

	for(i=0; i < e->regWriteCnt; i++)
		timeAvailable[(UINT32)e->regsWritten[i]] = issueTime + 1;
}

/* memory access stuff */
VOID readMem_ilp_one(ADDRINT effAddr, ADDRINT size){


	ADDRINT a;
	ADDRINT upperMemAddr, indexInChunk;
	memNode* chunk = (memNode*)NULL;
	ADDRINT shiftedAddr = effAddr >> ilp_block_size;
	ADDRINT shiftedEndAddr = (effAddr + size - 1) >> ilp_block_size;

	if(size > 0){
		for(a = shiftedAddr; a <= shiftedEndAddr; a++){
			upperMemAddr = a >> LOG_MAX_MEM_ENTRIES;
			indexInChunk = a ^ (upperMemAddr << LOG_MAX_MEM_ENTRIES);

			chunk = lookup(memAddressesTable, upperMemAddr);
			if(chunk == (memNode*)NULL)
				chunk = install(memAddressesTable, upperMemAddr);

			//assert(indexInChunk < MAX_MEM_ENTRIES);
			//assert(chunk->timeAvailable[indexInChunk] < (1 << size_pow_times));
			if(all_times[chunk->timeAvailable[indexInChunk]] > issueTime)
				issueTime = all_times[chunk->timeAvailable[indexInChunk]];
		}
	}
}

VOID writeMem_ilp_one(ADDRINT effAddr, ADDRINT size){

	ADDRINT a;
	ADDRINT upperMemAddr, indexInChunk;
	memNode* chunk = (memNode*)NULL;
	ADDRINT shiftedAddr = effAddr >> ilp_block_size;
	ADDRINT shiftedEndAddr = (effAddr + size - 1) >> ilp_block_size;

	if(size > 0){
		for(a = shiftedAddr; a <= shiftedEndAddr; a++){
			upperMemAddr = a >> LOG_MAX_MEM_ENTRIES;
			indexInChunk = a ^ (upperMemAddr << LOG_MAX_MEM_ENTRIES);

			chunk = lookup(memAddressesTable,upperMemAddr);
			if(chunk == (memNode*)NULL)
				chunk = install(memAddressesTable,upperMemAddr);

			//assert(indexInChunk < MAX_MEM_ENTRIES);
			if(chunk->timeAvailable[indexInChunk] == 0){
				index_all_times++;
				if(index_all_times >= (1 << size_pow_times))
					increase_size_all_times_one();
				chunk->timeAvailable[indexInChunk] = index_all_times;
			}
			//assert(chunk->timeAvailable[indexInChunk] < (1 << size_pow_times));
			all_times[chunk->timeAvailable[indexInChunk]] = issueTime + 1;
		}
	}
}

/* instrumenting (instruction level) */
/*VOID instrument_ilp_one(INS ins, VOID* v){

	UINT32 i;
	UINT32 maxNumRegsProd, maxNumRegsCons;
	REG reg;

	// register reads and memory reads determine the issue time
	maxNumRegsCons = INS_MaxNumRRegs(ins);

	for(i=0; i < maxNumRegsCons; i++){

		reg = INS_RegR(ins, i);

		assert((UINT32)reg < MAX_NUM_REGS);
		// only consider valid general-purpose registers (any bit-width) and floating-point registers,
		// i.e. exlude branch, segment and pin registers, among others
		if(REG_valid(reg) && (REG_is_fr(reg) || REG_is_mm(reg) || REG_is_xmm(reg) || REG_is_gr(reg) || REG_is_gr8(reg) || REG_is_gr16(reg) || REG_is_gr32(reg) || REG_is_gr64(reg))){
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readRegOp_ilp_one, IARG_UINT32, reg, IARG_END);
		}
	}

	if(INS_IsMemoryRead(ins)){

		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem_ilp_one, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);

		if(INS_HasMemoryRead2(ins)){

			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem_ilp_one, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_END);
		}
	}

	INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)checkIssueTime_one, IARG_END);

	// register writes and memory writes determine the time when these locations are available

	maxNumRegsProd = INS_MaxNumWRegs(ins);
	for(i=0; i < maxNumRegsProd; i++){

		reg = INS_RegW(ins, i);

		assert((UINT32)reg < MAX_NUM_REGS);
		// only consider valid general-purpose registers (any bit-width) and floating-point registers,
		// i.e. exlude branch, segment and pin registers, among others
		if(REG_valid(reg) && (REG_is_fr(reg) || REG_is_mm(reg) || REG_is_xmm(reg) || REG_is_gr(reg) || REG_is_gr8(reg) || REG_is_gr16(reg) || REG_is_gr32(reg) || REG_is_gr64(reg))){
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeRegOp_ilp_one, IARG_UINT32, reg, IARG_END);
		}
	}

	if(INS_IsMemoryWrite(ins)){

		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeMem_ilp_one, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
	}

	// count instructions
	if(interval_size == -1)
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ilp_instr_full_one, IARG_END);
	else
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ilp_instr_intervals_one, IARG_END);

}*/

/* finishing... */
VOID fini_ilp_one(INT32 code, VOID* v){

    char filename[100];

	fini_ilp_buffering_one();

	if(interval_size == -1){
        sprintf(filename, "ilp-win%d_full_int", win_size);

        output_file_ilp_one.open(mkfilename(filename), ios::out|ios::trunc);
		output_file_ilp_one << total_ins_count;
	}
	else{
        sprintf(filename, "ilp-win%d_phases_int", win_size);
        output_file_ilp_one.open(mkfilename(filename), ios::out|ios::app);
		output_file_ilp_one << interval_ins_count;
	}
	output_file_ilp_one << " " << cpuClock_interval << endl;

	output_file_ilp_one << "number of instructions: " << total_ins_count_for_hpc_alignment << endl;
	output_file_ilp_one.close();
}

/***************************************
     ILP (all 4 hardcoded window sizes)
****************************************/

/* initializing */
void init_ilp_all(){

	int i,j;

	init_ilp_common();
	init_ilp_buffering();

	size_pow_all_times_all = 10;
	for(i=0; i < ILP_WIN_SIZE_CNT; i++){
		all_times_all[i] = (UINT64*)checked_malloc((1 << size_pow_all_times_all) * sizeof(UINT64));
	}
	index_all_times_all = 1; // don't use first element of all_times_all

	ilp_block_size = _block_size;

	for(j=0; j < ILP_WIN_SIZE_CNT; j++){
		windowHead_all[j] = 0;
		windowTail_all[j] = 0;
		cpuClock_all[j] = 0;
		cpuClock_interval_all[j] = 0;
		for(i = 0; i < MAX_NUM_REGS; i++){
			timeAvailable_all[j][i] = 0;
		}

		executionProfile_all[j] = (UINT64*)checked_malloc(win_sizes[j]*sizeof(UINT64));

		for(i = 0; i < (int)win_sizes[j]; i++){
			executionProfile_all[j][i] = 0;
		}
		issueTime_all[j] = 0;
	}

	if(interval_size != -1){
		if(interval_size % ILP_BUFFER_SIZE != 0){
			cerr << "ERROR! Interval size is not a multiple of ILP buffer size. (" << interval_size << " vs " << ILP_BUFFER_SIZE << ")" << endl;
			exit(-1);
		}
		output_file_ilp_all.open(mkfilename("ilp_phases_int"), ios::out|ios::trunc);
		output_file_ilp_all.close();
	}
}

/* support */
void increase_size_all_times_all(){
	int i;
	UINT64* ptr;
	size_pow_all_times_all++;

	for(i=0; i < ILP_WIN_SIZE_CNT; i++){
		ptr = (UINT64*)realloc(all_times_all[i],(1 << size_pow_all_times_all)*sizeof(UINT64));
		if(ptr == (UINT64*)NULL){
			cerr << "Could not allocate memory (realloc)!" << endl;
			exit(1);
		}
		all_times_all[i] = ptr;
	}
}

/* per-instruction stuff */
VOID ilp_instr_all(){

	int i;
	UINT32 reordered;


	for(i=0; i < ILP_WIN_SIZE_CNT; i++){

		/* set issue time for tail of instruction window */
		executionProfile_all[i][windowTail_all[i]] = issueTime_all[i];
		windowTail_all[i] = (windowTail_all[i] + 1) % win_sizes[i];

		/* if instruction window (issue buffer) full */
		if(windowHead_all[i] == windowTail_all[i]){
			cpuClock_all[i]++;
			cpuClock_interval_all[i]++;
			reordered = 0;
			/* remove all instructions which are done from beginning of window,
			 * until an instruction comes along which is not ready yet:
			 * -> check executionProfile_all to see which instructions are done
			 * -> commit maximum win_size instructions (i.e. stop when issue buffer is empty)
			 */
			while((executionProfile_all[i][windowHead_all[i]] < cpuClock_all[i]) && (reordered < win_sizes[i])) {
				windowHead_all[i] = (windowHead_all[i] + 1) % win_sizes[i];
				reordered++;
			}
			//assert(reordered != 0);
		}

		/* reset issue times */
		issueTime_all[i] = 0;

	}

}

VOID ilp_instr_full_all(){

	/* counting instructions is done in all_instr_full() */

	ilp_instr_all();
}

VOID ilp_instr_intervals_all(){

	int i;

	/* counting instructions is done in all_instr_intervals() */

	if(interval_ins_count_for_hpc_alignment == interval_size){

		output_file_ilp_all.open(mkfilename("ilp_phases_int"), ios::out|ios::app);

		output_file_ilp_all << interval_ins_count;
		for(i = 0; i < ILP_WIN_SIZE_CNT; i++)
			output_file_ilp_all << " " << cpuClock_interval_all[i];
		output_file_ilp_all << endl;

		/* reset */
		interval_ins_count = 0;
		interval_ins_count_for_hpc_alignment = 0;

		for(i = 0; i < ILP_WIN_SIZE_CNT; i++)
			cpuClock_interval_all[i] = 0;

		/* clean up memory used, to avoid memory problems for long (CPU2006) benchmarks */
		size_pow_all_times_all = 10;
		for(i = 0; i < ILP_WIN_SIZE_CNT; i++){
			free(all_times_all[i]);
			all_times_all[i] = (UINT64*)checked_malloc((1 << size_pow_all_times_all) * sizeof(UINT64));
		}
		index_all_times_all = 1;

		nlist* np;
		nlist* np_rm;
		for(i=0; i < MAX_MEM_TABLE_ENTRIES; i++){
			np = memAddressesTable_all[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm->mem);
				free(np_rm);
			}
			memAddressesTable_all[i] = (nlist*) NULL;
		}

		output_file_ilp_all.close();
	}

	ilp_instr_all();
}

VOID checkIssueTime_all(){
	int i;

	for(i=0; i < ILP_WIN_SIZE_CNT; i++){
		if(cpuClock_all[i] > issueTime_all[i])
			issueTime_all[i] = cpuClock_all[i];
	}
}

/* register stuff */
VOID readRegOp_ilp_all(UINT32 regId){
	int i;

	for(i=0; i < ILP_WIN_SIZE_CNT; i++){

		if(timeAvailable_all[i][regId] > issueTime_all[i])
			issueTime_all[i] = timeAvailable_all[i][regId];
	}
}

VOID writeRegOp_ilp_all(UINT32 regId){
	int i;

	for(i=0; i < ILP_WIN_SIZE_CNT; i++){
		timeAvailable_all[i][regId] = issueTime_all[i] + 1;
	 }
}

/* memory access stuff */
VOID readMem_ilp_all(ADDRINT effAddr, ADDRINT size){

	int i;

	ADDRINT a;
	ADDRINT upperMemAddr, indexInChunk;
	memNode* chunk = (memNode*)NULL;
	ADDRINT shiftedAddr = effAddr >> ilp_block_size;
	ADDRINT shiftedEndAddr = (effAddr + size - 1) >> ilp_block_size;

	if(size > 0){
		for(a = shiftedAddr; a <= shiftedEndAddr; a++){
			upperMemAddr = a >> LOG_MAX_MEM_ENTRIES;
			indexInChunk = a ^ (upperMemAddr << LOG_MAX_MEM_ENTRIES);

			chunk = lookup(memAddressesTable_all,upperMemAddr);
			if(chunk == (memNode*)NULL)
				chunk = install(memAddressesTable_all,upperMemAddr);

			//assert(indexInChunk < MAX_MEM_ENTRIES);
			for(i=0; i < ILP_WIN_SIZE_CNT; i++){

				if(all_times_all[i][chunk->timeAvailable[indexInChunk]] > issueTime_all[i])
					issueTime_all[i] = all_times_all[i][chunk->timeAvailable[indexInChunk]];
			}
		}
	}
}

VOID writeMem_ilp_all(ADDRINT effAddr, ADDRINT size){
	int i;

	ADDRINT a;
	ADDRINT upperMemAddr, indexInChunk;
	memNode* chunk = (memNode*)NULL;
	ADDRINT shiftedAddr = effAddr >> ilp_block_size;
	ADDRINT shiftedEndAddr = (effAddr + size - 1) >> ilp_block_size;

	if(size > 0){
		for(a = shiftedAddr; a <= shiftedEndAddr; a++){
			upperMemAddr = a >> LOG_MAX_MEM_ENTRIES;
			indexInChunk = a ^ (upperMemAddr << LOG_MAX_MEM_ENTRIES);

			chunk = lookup(memAddressesTable_all,upperMemAddr);
			if(chunk == (memNode*)NULL)
				chunk = install(memAddressesTable_all,upperMemAddr);

			//assert(indexInChunk < MAX_MEM_ENTRIES);
			if(chunk->timeAvailable[indexInChunk] == 0){
				index_all_times_all++;
				if(index_all_times_all >= (1 << size_pow_all_times_all))
					increase_size_all_times_all();
				chunk->timeAvailable[indexInChunk] = index_all_times_all;
			}
			for(i=0; i < ILP_WIN_SIZE_CNT; i++){
				all_times_all[i][chunk->timeAvailable[indexInChunk]] = issueTime_all[i] + 1;
			}
		}
	}
}

/* instrumenting (instruction level) */
/*VOID instrument_ilp_all(INS ins, VOID* v){

	UINT32 i;
	UINT32 maxNumRegsProd, maxNumRegsCons;
	REG reg;


	// register reads and memory reads determine the issue time
	maxNumRegsCons = INS_MaxNumRRegs(ins);

	for(i=0; i < maxNumRegsCons; i++){

		reg = INS_RegR(ins, i);

		// only consider valid general-purpose registers (any bit-width) and floating-point registers,
		// i.e. exlude branch, segment and pin registers, among others
		if(REG_valid(reg) && (REG_is_fr(reg) || REG_is_mm(reg) || REG_is_xmm(reg) || REG_is_gr(reg) || REG_is_gr8(reg) || REG_is_gr16(reg) || REG_is_gr32(reg) || REG_is_gr64(reg))){
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readRegOp_ilp_all, IARG_UINT32, reg, IARG_END);
		}
	}

	if(INS_IsMemoryRead(ins)){

		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem_ilp_all, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);

		if(INS_HasMemoryRead2(ins)){

			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem_ilp_all, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_END);
		}
	}

	INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)checkIssueTime_all, IARG_END);

	// register writes and memory writes determine the time when these locations are available

	maxNumRegsProd = INS_MaxNumWRegs(ins);
	for(i=0; i < maxNumRegsProd; i++){

		reg = INS_RegW(ins, i);

		// only consider valid general-purpose registers (any bit-width) and floating-point registers,
		// i.e. exlude branch, segment and pin registers, among others
		if(REG_valid(reg) && (REG_is_fr(reg) || REG_is_mm(reg) || REG_is_xmm(reg) || REG_is_gr(reg) || REG_is_gr8(reg) || REG_is_gr16(reg) || REG_is_gr32(reg) || REG_is_gr64(reg))){
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeRegOp_ilp_all, IARG_UINT32, reg, IARG_END);
		}
	}

	if(INS_IsMemoryWrite(ins)){

		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeMem_ilp_all, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
	}

	// count instructions
	if(interval_size == -1)
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ilp_instr_full_all,IARG_END);
	else
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ilp_instr_intervals_all, IARG_END);
}*/

/* finishing... */
VOID fini_ilp_all(INT32 code, VOID* v){

	int i;

	fini_ilp_buffering_all();

	if(interval_size == -1){
		output_file_ilp_all.open(mkfilename("ilp_full_int"), ios::out|ios::trunc);
		output_file_ilp_all << total_ins_count;
	}
	else{
		output_file_ilp_all.open(mkfilename("ilp_phases_int"), ios::out|ios::app);
		output_file_ilp_all << interval_ins_count;
	}
	for(i = 0; i < ILP_WIN_SIZE_CNT; i++)
		output_file_ilp_all << " " << cpuClock_interval_all[i];

	output_file_ilp_all << endl;
	output_file_ilp_all << "number of instructions: " << total_ins_count_for_hpc_alignment << endl;
	output_file_ilp_all.close();
}

/**************************
     ILP (BUFFERING)
***************************/

/*
 * notes
 *
 * using PIN_FAST_ANALYSIS_CALL for buffering functions was tested
 * during the preparation of MICA v0.3, but showed to slightly slowdown
 * things instead of speeding them up, so it was dropped in the end
 */

/* initializing */
void init_ilp_buffering(){

	int i;

	ilp_buffer_index = 0;
	for(i=0; i < ILP_BUFFER_SIZE; i++){
		ilp_buffer[i] = (ilp_buffer_entry*)checked_malloc(sizeof(ilp_buffer_entry));
		ilp_buffer[i]->e = (ins_buffer_entry*)NULL;
		ilp_buffer[i]->mem_read1_addr = 0;
		ilp_buffer[i]->mem_read2_addr = 0;
		ilp_buffer[i]->mem_read_size = 0;
		ilp_buffer[i]->mem_write_addr = 0;
		ilp_buffer[i]->mem_write_size = 0;
	}
}

VOID ilp_buffer_instruction_only(void* _e){
	ilp_buffer[ilp_buffer_index]->e = (ins_buffer_entry*)_e;
}

VOID ilp_buffer_instruction_read(ADDRINT read1_addr, ADDRINT read_size){
	ilp_buffer[ilp_buffer_index]->mem_read1_addr = read1_addr;
	ilp_buffer[ilp_buffer_index]->mem_read_size = read_size;
}

VOID ilp_buffer_instruction_read2(ADDRINT read2_addr){
	ilp_buffer[ilp_buffer_index]->mem_read2_addr = read2_addr;
}

VOID ilp_buffer_instruction_write(ADDRINT write_addr, ADDRINT write_size){
	ilp_buffer[ilp_buffer_index]->mem_write_addr = write_addr;
	ilp_buffer[ilp_buffer_index]->mem_write_size = write_size;
}

ADDRINT ilp_buffer_instruction_next(){
	ilp_buffer_index++;
	return (ADDRINT)(ilp_buffer_index == ILP_BUFFER_SIZE || interval_ins_count_for_hpc_alignment == interval_size);
}

/* empty buffer for one given window size  */
VOID empty_buffer_one(){
	UINT32 i,j;

	for(i=0; i < ilp_buffer_index; i++){

		// register reads
		for(j=0; j < (UINT32)ilp_buffer[i]->e->regReadCnt; j++){
			readRegOp_ilp_one((UINT32)ilp_buffer[i]->e->regsRead[j]);
		}

		// memory reads
		if(ilp_buffer[i]->mem_read1_addr != 0){
			readMem_ilp_one(ilp_buffer[i]->mem_read1_addr, ilp_buffer[i]->mem_read_size);
			ilp_buffer[i]->mem_read1_addr = 0;

			if(ilp_buffer[i]->mem_read2_addr != 0){
				readMem_ilp_one(ilp_buffer[i]->mem_read2_addr, ilp_buffer[i]->mem_read_size);
				ilp_buffer[i]->mem_read2_addr = 0;
			}

			ilp_buffer[i]->mem_read_size = 0;
		}

		checkIssueTime_one();

		// register writes
		for(j=0; j < (UINT32)ilp_buffer[i]->e->regWriteCnt; j++){
			writeRegOp_ilp_one((UINT32)ilp_buffer[i]->e->regsWritten[j]);
		}

		// memory writes
		if(ilp_buffer[i]->mem_write_addr != 0){
			writeMem_ilp_one(ilp_buffer[i]->mem_write_addr, ilp_buffer[i]->mem_write_size);
			ilp_buffer[i]->mem_write_addr = 0;
			ilp_buffer[i]->mem_write_size = 0;
		}

		ilp_buffer[i]->e = (ins_buffer_entry*)NULL;

		if(interval_size == -1)
			ilp_instr_full_one();
		else
			ilp_instr_intervals_one();
	}

	ilp_buffer_index = 0;
}

/* empty buffer for all 4 (hardcoded) window sizes */
VOID empty_ilp_buffer_all(){
	UINT32 i,j;

	for(i=0; i < ilp_buffer_index; i++){

		// register reads
		for(j=0; j < (UINT32)ilp_buffer[i]->e->regReadCnt; j++){
			readRegOp_ilp_all((UINT32)ilp_buffer[i]->e->regsRead[j]);
		}

		// memory reads
		if(ilp_buffer[i]->mem_read1_addr != 0){
			readMem_ilp_all(ilp_buffer[i]->mem_read1_addr, ilp_buffer[i]->mem_read_size);
			ilp_buffer[i]->mem_read1_addr = 0;

			if(ilp_buffer[i]->mem_read2_addr != 0){
				readMem_ilp_all(ilp_buffer[i]->mem_read2_addr, ilp_buffer[i]->mem_read_size);
				ilp_buffer[i]->mem_read2_addr = 0;
			}

			ilp_buffer[i]->mem_read_size = 0;
		}

		checkIssueTime_all();

		// register writes
		for(j=0; j < (UINT32)ilp_buffer[i]->e->regWriteCnt; j++){
			writeRegOp_ilp_all((UINT32)ilp_buffer[i]->e->regsWritten[j]);
		}

		// memory writes
		if(ilp_buffer[i]->mem_write_addr != 0){
			writeMem_ilp_all(ilp_buffer[i]->mem_write_addr, ilp_buffer[i]->mem_write_size);
			ilp_buffer[i]->mem_write_addr = 0;
			ilp_buffer[i]->mem_write_size = 0;
		}

		ilp_buffer[i]->e = (ins_buffer_entry*)NULL;

		if(interval_size == -1)
			ilp_instr_full_all();
		else
			ilp_instr_intervals_all();
	}

	ilp_buffer_index = 0;
}

/* instrumenting (instruction level) */
VOID instrument_ilp_buffering_common(INS ins, ins_buffer_entry* e){

	UINT32 i, maxNumRegsProd, maxNumRegsCons, regReadCnt, regWriteCnt;
	REG reg;

	// buffer register reads per static instruction
	if(!e->setRead){


		// register reads and memory reads determine the issue time
		maxNumRegsCons = INS_MaxNumRRegs(ins);

		regReadCnt = 0;
		for(i=0; i < maxNumRegsCons; i++){
			reg = INS_RegR(ins, i);
			//assert((UINT32)reg < MAX_NUM_REGS);
			// only consider valid general-purpose registers (any bit-width) and floating-point registers,
			// i.e. exlude branch, segment and pin registers, among others
			if(REG_valid(reg) && (REG_is_fr(reg) || REG_is_mm(reg) || REG_is_xmm(reg) || REG_is_gr(reg) || REG_is_gr8(reg) || REG_is_gr16(reg) || REG_is_gr32(reg) || REG_is_gr64(reg))){
				regReadCnt++;
			}
		}

		e->regReadCnt = regReadCnt;
		e->regsRead = (REG*)checked_malloc(regReadCnt*sizeof(REG));

		regReadCnt = 0;
		for(i=0; i < maxNumRegsCons; i++){

			reg = INS_RegR(ins, i);

			//assert((UINT32)reg < MAX_NUM_REGS);
			// only consider valid general-purpose registers (any bit-width) and floating-point registers,
			// i.e. exlude branch, segment and pin registers, among others
			if(REG_valid(reg) && (REG_is_fr(reg) || REG_is_mm(reg) || REG_is_xmm(reg) || REG_is_gr(reg) || REG_is_gr8(reg) || REG_is_gr16(reg) || REG_is_gr32(reg) || REG_is_gr64(reg))){
				e->regsRead[regReadCnt++] = reg;
			}
		}

		e->setRead = true;

	}

	// buffer register writes per static instruction
	if(!e->setWritten){
		maxNumRegsProd = INS_MaxNumWRegs(ins);

		regWriteCnt = 0;
		for(i=0; i < maxNumRegsProd; i++){

			reg = INS_RegW(ins, i);

			//assert((UINT32)reg < MAX_NUM_REGS);
			// only consider valid general-purpose registers (any bit-width) and floating-point registers,
			// i.e. exlude branch, segment and pin registers, among others */
			if(REG_valid(reg) && (REG_is_fr(reg) || REG_is_mm(reg) || REG_is_xmm(reg) || REG_is_gr(reg) || REG_is_gr8(reg) || REG_is_gr16(reg) || REG_is_gr32(reg) || REG_is_gr64(reg))){
				regWriteCnt++;
			}
		}

		e->regWriteCnt = regWriteCnt;
		e->regsWritten = (REG*)checked_malloc(regWriteCnt*sizeof(REG));

		regWriteCnt = 0;
		for(i=0; i < maxNumRegsProd; i++){

			reg = INS_RegW(ins, i);

			//assert((UINT32)reg < MAX_NUM_REGS);
			// only consider valid general-purpose registers (any bit-width) and floating-point registers,
			// i.e. exlude branch, segment and pin registers, among others
			if(REG_valid(reg) && (REG_is_fr(reg) || REG_is_mm(reg) || REG_is_xmm(reg) || REG_is_gr(reg) || REG_is_gr8(reg) || REG_is_gr16(reg) || REG_is_gr32(reg) || REG_is_gr64(reg))){
				e->regsWritten[regWriteCnt++] = reg;
			}
		}

		e->setWritten = true;
	}

	// buffer memory operations (and instruction register buffer) with one single InsertCall
	INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ilp_buffer_instruction_only, IARG_PTR, (void*)e, IARG_END);

	if(INS_IsMemoryRead(ins)){

		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ilp_buffer_instruction_read, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);

		if(INS_HasMemoryRead2(ins)){
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ilp_buffer_instruction_read2, IARG_MEMORYREAD2_EA, IARG_END);
		}
	}

	if(INS_IsMemoryWrite(ins)){
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ilp_buffer_instruction_write, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
	}

	INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)ilp_buffer_instruction_next, IARG_END);

}

VOID instrument_ilp_one(INS ins, ins_buffer_entry* e){

	instrument_ilp_buffering_common(ins, e);
	// only called if buffer is full
	INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)empty_buffer_one, IARG_END);
}

VOID instrument_ilp_all(INS ins, ins_buffer_entry* e){

	instrument_ilp_buffering_common(ins, e);
	// only called if buffer is full
	INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)empty_ilp_buffer_all, IARG_END);
}

VOID fini_ilp_buffering_all(){

	if(ilp_buffer_index != 0)
		empty_ilp_buffer_all();
}

VOID fini_ilp_buffering_one(){

	if(ilp_buffer_index != 0)
		empty_buffer_one();
}

