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
#include "mica_memfootprint.h"

/* Global variables */

extern INT64 interval_size;
extern INT64 interval_ins_count;
extern INT64 interval_ins_count_for_hpc_alignment;
extern INT64 total_ins_count;
extern INT64 total_ins_count_for_hpc_alignment;

extern UINT32 _block_size;
extern UINT32 _page_size;

static UINT32 memfootprint_block_size;
static UINT32 page_size;

static ofstream output_file_memfootprint;

static nlist* DmemCacheWorkingSetTable[MAX_MEM_TABLE_ENTRIES];
static nlist* DmemPageWorkingSetTable[MAX_MEM_TABLE_ENTRIES];
static nlist* ImemCacheWorkingSetTable[MAX_MEM_TABLE_ENTRIES];
static nlist* ImemPageWorkingSetTable[MAX_MEM_TABLE_ENTRIES];


static long long DmemCacheWSS() {
	long long DmemCacheWorkingSetSize = 0L;
	for (int i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
		for (nlist *np = DmemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
			for (ADDRINT j = 0; j < MAX_MEM_BLOCK; j++) {
				if ((np->mem)->numReferenced [j]) {
					DmemCacheWorkingSetSize++;
				}
			}
		}
	}
	return DmemCacheWorkingSetSize;
}

static long long ImemCacheWSS() {
	long long ImemCacheWorkingSetSize = 0L;
	for (int i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
		for (nlist *np = ImemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
			for (ADDRINT j = 0; j < MAX_MEM_BLOCK; j++) {
				if ((np->mem)->numReferenced [j]) {
					ImemCacheWorkingSetSize++;
				}
			}
		}
	}
	return ImemCacheWorkingSetSize;
}

static long long DmemPageWSS() {
	long long DmemPageWorkingSetSize = 0L;
	for (int i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
		for (nlist *np = DmemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
			for (ADDRINT j = 0; j < MAX_MEM_BLOCK; j++) {
				if ((np->mem)->numReferenced [j]) {
					DmemPageWorkingSetSize++;
				}
			}
		}
	}
	return DmemPageWorkingSetSize;
}

static long long ImemPageWSS() {
	long long ImemPageWorkingSetSize = 0L;
	for (int i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
		for (nlist *np = ImemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
			for (ADDRINT j = 0; j < MAX_MEM_BLOCK; j++) {
				if ((np->mem)->numReferenced [j]) {
					ImemPageWorkingSetSize++;
				}
			}
		}
	}
	return ImemPageWorkingSetSize;
}

/* initializing */
void init_memfootprint(){
	int i;

	for (i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
		DmemCacheWorkingSetTable[i] = (nlist*) NULL;
		DmemPageWorkingSetTable[i] = (nlist*) NULL;
		ImemCacheWorkingSetTable[i] = (nlist*) NULL;
		ImemPageWorkingSetTable[i] = (nlist*) NULL;
	}

	memfootprint_block_size = _block_size;
	page_size = _page_size;

	if(interval_size != -1){
		output_file_memfootprint.open(mkfilename("memfootprint_phases_int"), ios::out|ios::trunc);
		output_file_memfootprint.close();
	}
}

VOID memOp(ADDRINT effMemAddr, ADDRINT size){
	if(size > 0){
		ADDRINT a;
		ADDRINT addr, endAddr, upperAddr, indexInChunk;
		memNode* chunk;

		/* D-stream (64-byte) cache block memory footprint */

		addr = effMemAddr >> memfootprint_block_size;
		endAddr = (effMemAddr + size - 1) >> memfootprint_block_size;

		for(a = addr; a <= endAddr; a++){

			upperAddr = a >> LOG_MAX_MEM_BLOCK;
			indexInChunk = a ^ (upperAddr << LOG_MAX_MEM_BLOCK);

			chunk = lookup(DmemCacheWorkingSetTable, upperAddr);
			if(chunk == (memNode*)NULL)
				chunk = install(DmemCacheWorkingSetTable, upperAddr);

			//assert(indexInChunk >= 0 && indexInChunk < MAX_MEM_BLOCK);
			chunk->numReferenced[indexInChunk] = true;

		}

		/* D-stream (4KB) page block memory footprint */
	
		addr = effMemAddr >> page_size;
		endAddr = (effMemAddr + size - 1) >> page_size;

		for(a = addr; a <= endAddr; a++){

			upperAddr = a >> LOG_MAX_MEM_BLOCK;
			indexInChunk = a ^ (upperAddr << LOG_MAX_MEM_BLOCK);

			chunk = lookup(DmemPageWorkingSetTable, upperAddr);
			if(chunk == (memNode*)NULL)
				chunk = install(DmemPageWorkingSetTable, upperAddr);

			//assert(indexInChunk >= 0 && indexInChunk < MAX_MEM_BLOCK);
			chunk->numReferenced[indexInChunk] = true;

		}
	}
}

VOID instrMem(ADDRINT instrAddr, ADDRINT size){

	if(size > 0){
		ADDRINT a;
		ADDRINT addr, endAddr, upperAddr, indexInChunk;
		memNode* chunk;


		/* I-stream (64-byte) cache block memory footprint */

		addr = instrAddr >> memfootprint_block_size;
		endAddr = (instrAddr + size - 1) >> memfootprint_block_size;

		for(a = addr; a <= endAddr; a++){

			upperAddr = a >> LOG_MAX_MEM_BLOCK;
			indexInChunk = a ^ (upperAddr << LOG_MAX_MEM_BLOCK);

			chunk = lookup(ImemCacheWorkingSetTable, upperAddr);
			if(chunk == (memNode*)NULL)
				chunk = install(ImemCacheWorkingSetTable, upperAddr);

			//assert(indexInChunk >= 0 && indexInChunk < MAX_MEM_BLOCK);
			chunk->numReferenced[indexInChunk] = true;

		}

		/* I-stream (4KB) page block memory footprint */

		addr = instrAddr >> page_size;
		endAddr = (instrAddr + size - 1) >> page_size;

		for(a = addr; a <= endAddr; a++){

			upperAddr = a >> LOG_MAX_MEM_BLOCK;
			indexInChunk = a ^ (upperAddr << LOG_MAX_MEM_BLOCK);

			chunk = lookup(ImemPageWorkingSetTable, upperAddr);
			if(chunk == (memNode*)NULL)
				chunk = install(ImemPageWorkingSetTable, upperAddr);

			//assert(indexInChunk >= 0 && indexInChunk < MAX_MEM_BLOCK);
			chunk->numReferenced[indexInChunk] = true;
		}
	}
}

static VOID memfootprint_instr_full(ADDRINT instrAddr, ADDRINT size){

	/* counting instructions is done in all_instr_full() */

	instrMem(instrAddr, size);
}

static ADDRINT memfootprint_instr_intervals(ADDRINT instrAddr, ADDRINT size){

	/* counting instructions is done in all_instr_intervals() */

	instrMem(instrAddr, size);
	return (ADDRINT)(interval_ins_count_for_hpc_alignment == interval_size);
}

VOID memfootprint_instr_interval_output(){

	output_file_memfootprint.open(mkfilename("memfootprint_phases_int"), ios::out|ios::app);

	long long DmemCacheWorkingSetSize = DmemCacheWSS();
	long long DmemPageWorkingSetSize = DmemPageWSS();
	long long ImemCacheWorkingSetSize = ImemCacheWSS();
	long long ImemPageWorkingSetSize = ImemPageWSS();

	output_file_memfootprint << DmemCacheWorkingSetSize << " " << DmemPageWorkingSetSize << " " << ImemCacheWorkingSetSize << " " << ImemPageWorkingSetSize << endl;
	output_file_memfootprint.close();
}

VOID memfootprint_instr_interval_reset(){
	/* clean used memory, to avoid memory shortage for long (CPU2006) benchmarks */
	for(ADDRINT i=0; i < MAX_MEM_TABLE_ENTRIES; i++){
		free_nlist(DmemCacheWorkingSetTable[i]);
		free_nlist(DmemPageWorkingSetTable[i]);
		free_nlist(ImemCacheWorkingSetTable[i]);
		free_nlist(ImemPageWorkingSetTable[i]);
	}
}

static VOID memfootprint_instr_interval(){

	memfootprint_instr_interval_output();
	memfootprint_instr_interval_reset();
	interval_ins_count = 0;
	interval_ins_count_for_hpc_alignment = 0;
}

/* instrumenting (instruction level) */
VOID instrument_memfootprint(INS ins, VOID* v){

	if(INS_IsMemoryRead(ins)){

		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)memOp, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);

		if(INS_HasMemoryRead2(ins)){

			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)memOp, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_END);
		}
	}
	if(INS_IsMemoryWrite(ins)){

		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)memOp, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
	}

	if(interval_size == -1)
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)memfootprint_instr_full, IARG_ADDRINT, INS_Address(ins), IARG_ADDRINT, (ADDRINT)INS_Size(ins), IARG_END);
	else{
		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)memfootprint_instr_intervals, IARG_ADDRINT, INS_Address(ins), IARG_ADDRINT, (ADDRINT)INS_Size(ins), IARG_END);
		INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)memfootprint_instr_interval, IARG_END);
	}
}


/* finishing... */
VOID fini_memfootprint(INT32 code, VOID* v){

	long long DmemCacheWorkingSetSize = DmemCacheWSS();
	long long DmemPageWorkingSetSize = DmemPageWSS();
	long long ImemCacheWorkingSetSize = ImemCacheWSS();
	long long ImemPageWorkingSetSize = ImemPageWSS();

	if(interval_size == -1){
		output_file_memfootprint.open(mkfilename("memfootprint_full_int"), ios::out|ios::trunc);
	}
	else{
		output_file_memfootprint.open(mkfilename("memfootprint_phases_int"), ios::out|ios::app);
	}

	output_file_memfootprint << DmemCacheWorkingSetSize << " " << DmemPageWorkingSetSize << " " << ImemCacheWorkingSetSize << " " << ImemPageWorkingSetSize << endl;
	output_file_memfootprint << "number of instructions: " << total_ins_count_for_hpc_alignment << endl;
	output_file_memfootprint.close();
}
