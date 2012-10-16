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
#include "mica_memstackdist.h"

/* Global variables */

extern INT64 interval_size;
extern INT64 interval_ins_count;
extern INT64 interval_ins_count_for_hpc_alignment;
extern INT64 total_ins_count;
extern INT64 total_ins_count_for_hpc_alignment;

extern UINT32 _block_size;

static UINT32 memstackdist_block_size;

static ofstream output_file_memstackdist;

/* A single entry of the cache line reference stack.
 * below points to the entry below us in the stack
 * above points to the entry above us in the stack
 * block_addr is the cache line index of this entry
 * bucket is the number of the stack depth bucket where this entry belongs
 */
typedef struct stack_entry_type {
	struct stack_entry_type* below;
	struct stack_entry_type* above;
	ADDRINT block_addr;
	INT32 bucket;
} stack_entry;

/* A single entry of the hash table, contains an array of stack entries referenced by part of cache line index. */
typedef struct block_type_fast {
	ADDRINT id;
	stack_entry* stack_entries[MAX_MEM_ENTRIES];
	struct block_type_fast* next;
} block_fast;

static stack_entry* stack_top;
static UINT64 stack_size;

static block_fast* hashTableCacheBlocks_fast[MAX_MEM_TABLE_ENTRIES];
static INT64 mem_ref_cnt;
static INT64 cold_refs;

/* Counters of accesses into each bucket. */
static INT64 buckets[BUCKET_CNT];
/* References to stack entries that are the oldest entries belonging to the particular bucket.
 * This is used to update bucket attributes of stack entries efficiently. Since the last
 * bucket is overflow bucket, last borderline entry should never be set. */
static stack_entry* borderline_stack_entries[BUCKET_CNT];

/* initializing */
void init_memstackdist(){

	int i;

	/* initialize */
	cold_refs = 0;
	for(i=0; i < BUCKET_CNT; i++){
		buckets[i] = 0;
		borderline_stack_entries[i] = NULL;
	}
	mem_ref_cnt = 0;
	/* hash table */
	for (i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
		hashTableCacheBlocks_fast[i] = NULL;
	}
	/* access stack */
	/* a dummy entry is inserted on the stack top to save some checks later */
	/* since the dummy entry is not in the hash table, it should never be used */
	stack_top = (stack_entry*) checked_malloc(sizeof(stack_entry));
	stack_top->block_addr = 0;
	stack_top->above = NULL;
	stack_top->below = NULL;
	stack_top->bucket = 0;
	stack_size = 1;

	memstackdist_block_size = _block_size;

	if(interval_size != -1){
		output_file_memstackdist.open(mkfilename("memstackdist_phases_int"), ios::out|ios::trunc);
		output_file_memstackdist.close();
	}
}

/*VOID memstackdist_instr_full(){
	// counting instructions is done in all_instr_full()

}*/

static ADDRINT memstackdist_instr_intervals(){

	/* counting instructions is done in all_instr_intervals() */

	return (ADDRINT)(interval_ins_count_for_hpc_alignment == interval_size);
}

VOID memstackdist_instr_interval_output(){
	int i;
	output_file_memstackdist.open(mkfilename("memstackdist_phases_int"), ios::out|ios::app);
	output_file_memstackdist << mem_ref_cnt << " " << cold_refs;
	for(i=0; i < BUCKET_CNT; i++){
		output_file_memstackdist << " " << buckets[i];
	}
	output_file_memstackdist << endl;
	output_file_memstackdist.close();
}

VOID memstackdist_instr_interval_reset(){
	int i;
	mem_ref_cnt = 0;
	cold_refs = 0;
	for(i=0; i < BUCKET_CNT; i++){
		buckets[i] = 0;
	}
}

static VOID memstackdist_instr_interval(){

	memstackdist_instr_interval_output();
	memstackdist_instr_interval_reset();
	interval_ins_count = 0;
	interval_ins_count_for_hpc_alignment = 0;
}

/* hash table support */

/** entry_lookup
 *
 * Finds an arrray of stack entry references for a given address key (upper part of address) in a hash table.
 */
stack_entry** entry_lookup(block_fast** table, ADDRINT key){

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


/* stack support */

#if 0
/** stack_sanity_check
 *
 * Checks whether the stack structure is internally consistent.
 */
static VOID stack_sanity_check(){

	UINT64 position = 0;
	INT32 bucket = 0;

	stack_entry *e = stack_top;

	if (e->above != NULL){
		ERROR_MSG("Item above top of stack.");
		exit(1);
	}

	while (e != NULL){

		// Check whether the stack entry has a correct bucket.
		if (e->bucket != bucket){
			ERROR_MSG("Stack entry with invalid bucket.");
			exit(1);
		}

		// Check whether the stack entry is linked correctly.
		if (e->above && (e->above->below != e)){
			ERROR_MSG("Incorrectly linked stack.");
			exit(1);
		}
		if (e->below && (e->below->above != e)){
			ERROR_MSG("Incorrectly linked stack.");
			exit(1);
		}

		// Calculate which bucket we should be in next.
		// Never spill over the overflow bucket though.
		if (bucket < BUCKET_CNT - 1)
		{
			UINT64 borderline = ((UINT64) 1) << bucket;
			if (position == borderline){
				if (borderline_stack_entries [bucket] != e){
					ERROR_MSG("Incorrect bucket borderline.");
					exit(1);
				}
				bucket ++;
			}
		}

		// Go on through the entire stack.
		e = e->below;
		position++;
	}
}
#endif


/** move_to_top_fast
 *
 * Moves the stack entry e corresponding to the address a to the top of stack.
 * The stack entry can be NULL, in which case a new stack entry is created.
 */
static VOID move_to_top_fast(stack_entry *e, ADDRINT a){

	INT32 bucket;

	/* check if entry was accessed before */
	if(e != NULL){

		/* check to see if we already are at top of stack */
		if(e->above != NULL){

			// disconnect the entry from its current position on the stack
			if (e->below != NULL) e->below->above = e->above;
			e->above->below = e->below;

			// adjust all borderline entries above the entry touched (note that we can be sure those entries exist)
			// a borderline entry is an entry whose bucket will change when an item is inserted above it on the stack
			for(bucket=0; bucket < BUCKET_CNT && bucket < e->bucket; bucket++){
				borderline_stack_entries[bucket]->bucket++;
				borderline_stack_entries[bucket] = borderline_stack_entries[bucket]->above;
			}
			// if the entry touched was a borderline entry, new borderline entry is the one above the touched one
			if(e == borderline_stack_entries[e->bucket]){
				borderline_stack_entries[e->bucket] = borderline_stack_entries[e->bucket]->above;
			}

			// place new entry on top of LRU stack
			e->below = stack_top;
			e->above = NULL;
			stack_top->above = e;
			stack_top = e;
			e->bucket = 0;
		}
		/* else: if top of stack was referenced again, nothing to do! */

	}
	else{
		// allocate memory for new stack entry
		stack_entry* e = (stack_entry*) checked_malloc(sizeof(stack_entry));

		// initialize with address and refer prev to top of stack
		e->block_addr = a;
		e->above = NULL;
		e->below = stack_top;
		e->bucket = 0;

		// adjust top of stack
		stack_top->above = e;
		stack_top = e;

		stack_size++;

		// adjust all borderline entries that exist up until the overflow bucket
		// (which really has no borderline entry since there is no next bucket)
		// we retain the number of the first free bucket for next code
		for(bucket=0; bucket < BUCKET_CNT - 1; bucket++){
			if (borderline_stack_entries[bucket] == NULL) break;
			borderline_stack_entries[bucket]->bucket++;
			borderline_stack_entries[bucket] = borderline_stack_entries[bucket]->above;
		}

		// if the stack size has reached a boundary of a bucket, set the boundary entry for this bucket
		// the variable types are chosen deliberately large for overflow safety
		// at least they should not overflow sooner than stack_size anyway
		// overflow bucket boundar is never set
		if (bucket < BUCKET_CNT - 1)
		{
			UINT64 borderline_distance = ((UINT64) 2) << bucket;
			if(stack_size == borderline_distance){
				// find the bottom of the stack by traversing from somewhere close to it
				stack_entry *stack_bottom;
				if (bucket) stack_bottom = borderline_stack_entries [bucket-1];
				       else stack_bottom = stack_top;
				while (stack_bottom->below) stack_bottom = stack_bottom->below;
				// the new borderline is the bottom of the stack
				borderline_stack_entries [bucket] = stack_bottom;
			}
		}
	}

	// stack_sanity_check();
}

/* determine reuse distance (= number of unique cache blocks referenced since last time this cache was referenced)
 * reuse distance is tracked in move_to_top_fast (by climbing up the LRU stack entry-by-entry until top of stack is reached),
 * this function only returns the reuse distance calculated by move_to_top_fast */

static INT64 det_reuse_dist_bucket(stack_entry* e){

	if(e != NULL)
		return e->bucket;
	else
		return -1;
}

/* register memory access (either read of write) determine which cache lines are touched */
VOID memstackdist_memRead(ADDRINT effMemAddr, ADDRINT size){

	ADDRINT a, endAddr, addr, upperAddr, indexInChunk;
	stack_entry** chunk;
	stack_entry* entry_for_addr;

	/* Calculate index in cache addresses. The calculation does not
	 * handle address overflows but those are unlikely to happen. */
	addr = effMemAddr >> memstackdist_block_size;
	endAddr = (effMemAddr + size - 1) >> memstackdist_block_size;

	/* The hit is counted for all cache lines involved. */
	for(a = addr; a <= endAddr; a++){

		/* split the cache line address into hash key of chunk and index in chunk */
		upperAddr = a >> LOG_MAX_MEM_ENTRIES;
		indexInChunk = a & MASK_MAX_MEM_ENTRIES;

		chunk = entry_lookup(hashTableCacheBlocks_fast, upperAddr);
		if(chunk == NULL) chunk = entry_install(hashTableCacheBlocks_fast, upperAddr);

		entry_for_addr = chunk[indexInChunk];

		/* determine reuse distance for this access (if it has been accessed before) */
		INT64 b = det_reuse_dist_bucket(entry_for_addr);

		if(b < 0)
			cold_refs++;
		else
			buckets[b]++;

		/* adjust LRU stack */
		/* as a side effect, can allocate new entry, which could have been NULL so far */
		move_to_top_fast(entry_for_addr, a);

		/* update hash table for new cache blocks */
		if(chunk[indexInChunk] == NULL) chunk[indexInChunk] = stack_top;

		mem_ref_cnt++;
	}
}

VOID instrument_memstackdist(INS ins, VOID *v){

	if( INS_IsMemoryRead(ins) ){

		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)memstackdist_memRead, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);

		if( INS_HasMemoryRead2(ins) )
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)memstackdist_memRead, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_END);
	}

	if(interval_size != -1){
		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)memstackdist_instr_intervals,IARG_END);
		/* only called if interval is 'full' */
		INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)memstackdist_instr_interval,IARG_END);
	}
}

/* finishing... */
VOID fini_memstackdist(INT32 code, VOID* v){

	int i;

	if(interval_size == -1){
		output_file_memstackdist.open(mkfilename("memstackdist_full_int"), ios::out|ios::trunc);
	}
	else{
		output_file_memstackdist.open(mkfilename("memstackdist_phases_int"), ios::out|ios::app);
	}
	output_file_memstackdist << mem_ref_cnt << " " << cold_refs;
	for(i=0; i < BUCKET_CNT; i++){
		output_file_memstackdist << " " << buckets[i];
	}
	output_file_memstackdist << endl << "number of instructions: " << total_ins_count_for_hpc_alignment << endl;
	output_file_memstackdist.close();
}
