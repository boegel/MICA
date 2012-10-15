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
#include "mica_memreusedist2.h"

/* Global variables */
extern INT64 interval_size;
extern INT64 interval_ins_count;
extern INT64 interval_ins_count_for_hpc_alignment;
extern INT64 total_ins_count;
extern INT64 total_ins_count_for_hpc_alignment;
extern UINT32 _block_size;

ofstream output_file_memreusedist2;

/* A single entry of the cache line reference stack.
 * below points to the entry below us in the stack
 * above points to the entry above us in the stack
 * block_addr is the cache line index of this entry
 * distance is the distance in the stack
 */
typedef struct stack_entry_type {
	struct stack_entry_type* below;
	struct stack_entry_type* above;
	ADDRINT block_addr;
	INT32 distance;
} stack_entry;

/* A single entry of the hash table, contains an array of stack entries referenced by part of cache line index. */
typedef struct block_type_fast {
	ADDRINT id;
	stack_entry* stack_entries[MAX_MEM_ENTRIES];
	struct block_type_fast* next;
} block_fast;

static stack_entry* stack_top;
static stack_entry* stack_bottom;

static block_fast* hashTableCacheBlocks_fast[MAX_MEM_TABLE_ENTRIES];
static INT64 mem_ref_cnt;
static INT64 cold_refs;

/* Counters of accesses for each distance. */
#define DIST_MAX 100
static INT64 distances[DIST_MAX];

/* initializing */
void init_memreusedist2(){
	int i;

	/* initialize */
	cold_refs = 0;
	mem_ref_cnt = 0;
	for(i = 0; i < DIST_MAX; i++) {
		distances[i] = 0;
	}
	/* hash table */
	for (i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
		hashTableCacheBlocks_fast[i] = NULL;
	}

	stack_bottom = stack_top = NULL;

	if(interval_size != -1){
		output_file_memreusedist2.open(mkfilename("memreusedist2_phases_int"), ios::out|ios::trunc);
		output_file_memreusedist2.close();
	}
}

ADDRINT memreusedist2_instr_intervals(){
	/* counting instructions is done in all_instr_intervals() */

	return (ADDRINT)(interval_ins_count_for_hpc_alignment == interval_size);
}

VOID memreusedist2_instr_interval_output(){
	int i;
	output_file_memreusedist2.open(mkfilename("memreusedist2_phases_int"), ios::out|ios::app);
	output_file_memreusedist2 << mem_ref_cnt << " " << cold_refs << endl;
	for(i=0; i < DIST_MAX; i++){
		output_file_memreusedist2 << " " << distances[i];
	}
	output_file_memreusedist2 << endl;
	output_file_memreusedist2.close();
}

VOID memreusedist2_instr_interval_reset(){
	int i;
	cold_refs = 0;
	mem_ref_cnt = 0;
	for(i = 0; i < DIST_MAX; i++) {
		distances[i] = 0;
	}
}

VOID memreusedist2_instr_interval(){

	memreusedist2_instr_interval_output();
	memreusedist2_instr_interval_reset();
	interval_ins_count = 0;
	interval_ins_count_for_hpc_alignment = 0;
}

/* hash table support */

/** entry_lookup
 *
 * Finds an arrray of stack entry references for a given address key (upper part of address) in a hash table.
 */
static stack_entry** entry_lookup(block_fast** table, ADDRINT key){

	block_fast* b;

	for (b = table[key % MAX_MEM_TABLE_ENTRIES]; b != NULL; b = b->next){
		if(b->id == key)
			return b->stack_entries;
	}

	return NULL;
}

/** entry_install
 *
 * Installs a new array of stack entry references for a given address key (upper part of address) in a hash table.
 */
static stack_entry** entry_install(block_fast** table, ADDRINT key){

	block_fast* b;

	ADDRINT index = key % MAX_MEM_TABLE_ENTRIES;

	b = table[index];

	if(b == NULL) {
		b = (block_fast*)checked_malloc(sizeof(block_fast));
		table[index] = b;
	}
	else{
		while(b->next != NULL){
			b = b->next;
		}
		b->next = (block_fast*)checked_malloc(sizeof(block_fast));
		b = b->next;
	}
	b->next = NULL;
	b->id = key;
	for(ADDRINT i = 0; i < MAX_MEM_ENTRIES; i++){
		b->stack_entries[i] = NULL;
	}
	return b->stack_entries;
}

/** add_new_stack_entry
 *
 * Add a new stack entry at the bottom of the stack. The distance is not
 * initialized here.
 */
static stack_entry* add_new_stack_entry(ADDRINT a) {
	// allocate memory for new stack entry
	stack_entry* e = (stack_entry*) checked_malloc(sizeof(stack_entry));

	// initialize with address 
	e->block_addr = a;
	e->above = stack_bottom;
	e->below = NULL;

	if (stack_bottom) stack_bottom->below = e;
	stack_bottom = e;

	if (!stack_top) stack_top = e;
	return e;
}


/** move_to_top
 *
 * Moves the stack entry e corresponding to the address a to the top of stack.
 * The stack entry can't be NULL.
 */
static VOID move_to_top(stack_entry *e){
	/* check to see if we are not already at top of stack */
	if(e != stack_top){
		// update positions
		stack_entry *aux = stack_top;
		while (aux != e) {
			aux->distance++;
			aux = aux->below;
		}
		// disconnect the entry from its current position on the stack
		if (e->below) {
			e->below->above = e->above;
		} else {
			stack_bottom = e->above;
		}
		e->above->below = e->below;

		// place it on top of LRU stack
		e->below = stack_top;
		e->above = NULL;
		stack_top->above = e;
		stack_top = e;
	}

	e->distance = 0;
}

/* register memory access (either read of write) determine which cache lines are touched */
VOID memreusedist2_memRead(ADDRINT effMemAddr, ADDRINT size){

	ADDRINT a, endAddr, addr, upperAddr, indexInChunk;
	stack_entry** chunk;
	stack_entry* entry_for_addr;

	/* Calculate index in cache addresses. The calculation does not
	 * handle address overflows but those are unlikely to happen. */
	addr = effMemAddr >> _block_size;
	endAddr = (effMemAddr + size - 1) >> _block_size;

	/* The hit is counted for all cache lines involved. */
	for(a = addr; a <= endAddr; a++){

		/* split the cache line address into hash key of chunk and index in chunk */
		upperAddr = a >> LOG_MAX_MEM_ENTRIES;
		indexInChunk = a & MASK_MAX_MEM_ENTRIES;

		chunk = entry_lookup(hashTableCacheBlocks_fast, upperAddr);
		if(chunk == NULL) chunk = entry_install(hashTableCacheBlocks_fast, upperAddr);

		entry_for_addr = chunk[indexInChunk];

		/* determine reuse distance for this access (if it has been accessed before) */
		if (entry_for_addr) {
			int d = entry_for_addr->distance;
			if (d >= DIST_MAX) d = DIST_MAX - 1;
			distances[d]++;
		} else {
			cold_refs++;
			entry_for_addr = add_new_stack_entry(a);
		}

		/* adjust LRU stack */
		move_to_top(entry_for_addr);

		/* update hash table for new cache blocks */
		if(chunk[indexInChunk] == NULL) chunk[indexInChunk] = stack_top;

		mem_ref_cnt++;
	}
}

VOID instrument_memreusedist2(INS ins, VOID *v){

	if( INS_IsMemoryRead(ins) ){

		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)memreusedist2_memRead, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);

		if( INS_HasMemoryRead2(ins) )
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)memreusedist2_memRead, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_END);
	}

	if(interval_size != -1){
		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)memreusedist2_instr_intervals,IARG_END);
		/* only called if interval is 'full' */
		INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)memreusedist2_instr_interval,IARG_END);
	}
}

/* finishing... */
VOID fini_memreusedist2(INT32 code, VOID* v){

	int i;

	if(interval_size == -1){
		output_file_memreusedist2.open(mkfilename("memreusedist2_full_int"), ios::out|ios::trunc);
	}
	else{
		output_file_memreusedist2.open(mkfilename("memreusedist2_phases_int"), ios::out|ios::app);
	}
	output_file_memreusedist2 << mem_ref_cnt << " " << cold_refs << endl;
	for(i = 0; i < DIST_MAX; i++){
		output_file_memreusedist2 << " " << distances[i];
	}
	output_file_memreusedist2 << endl << "number of instructions: " << total_ins_count_for_hpc_alignment << endl;
	output_file_memreusedist2.close();
}
