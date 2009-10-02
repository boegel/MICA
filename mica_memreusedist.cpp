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
#include "mica_memreusedist.h"

/* Global variables */

extern INT64 interval_size;
extern INT64 interval_ins_count;
extern INT64 total_ins_count;

FILE* output_file_memreusedist;

typedef struct stack_entry_type {
	struct stack_entry_type* prev;
	struct stack_entry_type* next;
	ADDRINT block_addr;
	INT32 bucket;
} stack_entry;

typedef struct block_type_fast {
	ADDRINT id;
	stack_entry* stack_entries[MAX_MEM_ENTRIES]; 
	struct block_type_fast* next;
} block_fast;

stack_entry* stack;
stack_entry* stack_top;
INT64 stack_size;

INT64 ins_cnt;
INT64 start_ins_cnt;
block_fast* hashTableCacheBlocks_fast[MAX_MEM_TABLE_ENTRIES];
INT64 mem_ref_cnt;
INT64 cold_refs;

INT64 buckets[BUCKET_CNT];
stack_entry* borderline_stack_entries[BUCKET_CNT];

/*VOID print_stack(){

	stack_entry* e = stack_top;
	int index;

	fprintf(stderr,"borderline_stack_entries: ");
	for(index=0; index < BUCKET_CNT; index++)
		fprintf(stderr,"[%d] 0x%x, ", index, (unsigned int)borderline_stack_entries[index]);
	fprintf(stderr,"\n");

	index = 0;
	while(e != (stack_entry*)NULL){

		fprintf(stderr,"   0x%x [a: 0x%x] (bucket: %d)", (unsigned int)e, (unsigned int)e->block_addr, (int)e->bucket);
		if(borderline_stack_entries[index] == e){
			fprintf(stderr," *\n");
			index++;
		}
		else
			fprintf(stderr,"\n");

		e = e->prev;
	}
}*/

/* initializing */
void init_memreusedist(){
	
	int i;

	/* initialize */
	cold_refs = 0;
	for(i=0; i < BUCKET_CNT; i++){
		buckets[i] = 0;
		borderline_stack_entries[i] = (stack_entry*)NULL;
	}
	mem_ref_cnt = 0;
	/* hash table */
	for (i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
		hashTableCacheBlocks_fast[i] = (block_fast*) NULL;
	}
	/* stack */
	stack_top = (stack_entry*)malloc(sizeof(stack_entry));
	stack_top->block_addr = 0;
	stack_top->bucket = -1;
	stack_top->next = NULL;
	stack_top->prev = NULL;
	stack_top->bucket = 0;
	stack_size = 0;

	borderline_stack_entries[0] = stack_top;

	//print_stack();

	if(interval_size != -1){		
		output_file_memreusedist = fopen("memreusedist_phases_int_pin.out","w");
		fclose(output_file_memreusedist);
	}
}

/*VOID memreusedist_instr_full(){
	// counting instructions is done in all_instr_full()

}*/

ADDRINT memreusedist_instr_intervals(){

	/* counting instructions is done in all_instr_intervals() */

	return (ADDRINT)(total_ins_count % interval_size == 0);
}

VOID memreusedist_instr_interval_output(){
	int i;
	output_file_memreusedist = fopen("memreusedist_phases_int_pin.out","a");
	fprintf(output_file_memreusedist, "%lld %lld", mem_ref_cnt, cold_refs);
	for(i=0; i < BUCKET_CNT; i++){
		fprintf(output_file_memreusedist, " %lld", buckets[i]);
	}
	fprintf(output_file_memreusedist, "\n");
	fclose(output_file_memreusedist);
}

VOID memreusedist_instr_interval_reset(){
	int i;
	mem_ref_cnt = 0;
	cold_refs = 0;
	for(i=0; i < BUCKET_CNT; i++){
		buckets[i] = 0;
	}
}

VOID memreusedist_instr_interval(){

	memreusedist_instr_interval_output();
	memreusedist_instr_interval_reset();
	interval_ins_count = 0;
}

/* hash table support */
stack_entry** lookup(block_fast** table, ADDRINT key){

	block_fast* b;

	for (b = table[key % MAX_MEM_TABLE_ENTRIES]; b != (block_fast*)NULL; b = b->next){
		if(b->id == key)
			return b->stack_entries;
	}

	return (stack_entry**)NULL;
}

stack_entry** install(block_fast** table, ADDRINT key){

	int i;
	block_fast* b;

	ADDRINT index = key % MAX_MEM_TABLE_ENTRIES;

	b = table[index];

	if(b == (block_fast*)NULL) {
		if((b = (block_fast*)malloc(sizeof(block_fast))) == (block_fast*)NULL){
			fprintf(stderr,"Not enough memory (in install)\n");
			exit(1);
		}
		table[index] = b;
	}
	else{
		while(b->next != (block_fast*)NULL){
			b = b->next;	
		}
		if((b->next = (block_fast*)malloc(sizeof(block_fast))) == (block_fast*)NULL){
			fprintf(stderr,"Not enough memory (in install (2))\n");
			exit(1);
		}
		b = b->next;	
	}
	b->next = (block_fast*)NULL;
	b->id = key;
	for(i = 0; i < MAX_MEM_ENTRIES; i++){
		b->stack_entries[i] = (stack_entry*)NULL;
	}
	return b->stack_entries;
}

/* stack support */
VOID move_to_top_fast(stack_entry *e, ADDRINT a, stack_entry** top){
	
	INT32 i;

	/* check if entry was accessed before */	
	if(e != (stack_entry*)NULL){

		/* check to see if we already are at top of stack */
		if(e->next != (stack_entry*)NULL){

			/* avoid referencing prev for bottom of stack */
			if(e->prev != (stack_entry*)NULL){
				e->prev->next = e->next;
			}
			e->next->prev = e->prev;

			// adjust all borderline entries above the entry touched (start with i=2 to avoid problems with too small stacks)
			for(i=2; i < BUCKET_CNT && i < e->bucket; i++){
				borderline_stack_entries[i-1]->bucket++;
				borderline_stack_entries[i-1] = borderline_stack_entries[i-1]->next;
			}
			// if entry touched is borderline entry, new borderline entry is the one above the touched one (i.e. ->next)
			if(e == borderline_stack_entries[e->bucket-1]){
				borderline_stack_entries[e->bucket-1] = borderline_stack_entries[e->bucket-1]->next;
			}

			// place new entry on top of LRU stack
			e->prev = *top;
			e->next = (stack_entry*)NULL;
			(*top)->next = e;
			borderline_stack_entries[0]->bucket++;
			(*top)->bucket = 1;
			borderline_stack_entries[0] = (*top);

			*top = e;
			e->bucket = 0;

		}
		/* else: if top of stack was referenced again, nothing to do! */

	}
	else{
		// allocate memory for new stack entry
		stack_entry* e = (stack_entry*) malloc(sizeof(stack_entry));

		// initialize with address and refer prev to top of stack
		e->block_addr = a;
		e->next = (stack_entry*)NULL;
		e->prev = *top;

		// adjust top of stack
		(*top)->next = e;
		borderline_stack_entries[0]->bucket++;
		(*top)->bucket = 1;
		borderline_stack_entries[0] = (*top);

		// set new entry as top of stack
		*top = e;
		e->bucket = 0;
		
		stack_size++;

		// adjust bucket for borderline entries (except for very last bucket = overflow bucket)
		for(i=2; i < BUCKET_CNT-1 && (1 << i) <= stack_size; i++){
			borderline_stack_entries[i-1]->bucket++;
			borderline_stack_entries[i-1] = borderline_stack_entries[i-1]->next;
		}

		// if stack size reaches boundary of bucket, set boundary entry for this bucket
		if(stack_size == (1 << i) -1 && i < BUCKET_CNT){
			borderline_stack_entries[i-1] = borderline_stack_entries[i-2];
			while(borderline_stack_entries[i-1]->prev != (stack_entry*)NULL){
				borderline_stack_entries[i-1] = borderline_stack_entries[i-1]->prev;
			}
		}
	}
}

/* determine reuse distance (= number of unique cache blocks referenced since last time this cache  was referenced) 
 * this is done by climbing up the LRU stack entry-by-entry until top of stack is reached */
INT64 det_reuse_dist_bucket(stack_entry* e){

	if(e != (stack_entry*)NULL)
		return e->bucket;
	else
		return -1;
}

/* register memory access (either read of write) determine which 64-byte cache blocks are touched */
VOID memreusedist_memRead(ADDRINT effMemAddr, ADDRINT size){

	ADDRINT a, endAddr, addr, upperAddr, indexInChunk;
	stack_entry** chunk;
	stack_entry* entry_for_addr;

	/* D-stream (64-byte) cache  memory footprint */

	addr = effMemAddr >> 6;
	endAddr = (effMemAddr + size) >> 6;

	for(a = addr; a <= endAddr; a++){

		upperAddr = a >> LOG_MAX_MEM_ENTRIES;
		indexInChunk = a ^ (upperAddr << LOG_MAX_MEM_ENTRIES);

		chunk = lookup(hashTableCacheBlocks_fast, upperAddr);
		if(chunk == (stack_entry**)NULL)
			chunk = install(hashTableCacheBlocks_fast, upperAddr);

		entry_for_addr = chunk[indexInChunk];

		/* determine reuse distance for this access (if it has been accessed before) */
		INT64 b = det_reuse_dist_bucket(entry_for_addr);

		if(b < 0)
			cold_refs++;
		else
			buckets[b]++;

		/* adjust LRU stack */	
		move_to_top_fast(entry_for_addr, a, &stack_top);

		/* update hash table for new cache blocks */
		if(chunk[indexInChunk] == (stack_entry*)NULL)
			chunk[indexInChunk] = stack_top;

		mem_ref_cnt++;
	}
}

VOID instrument_memreusedist(INS ins, VOID *v){

	if( INS_IsMemoryRead(ins) ){

		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)memreusedist_memRead, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);

		if( INS_HasMemoryRead2(ins) )
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)memreusedist_memRead, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_END);
	}

	if(interval_size != -1){
		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)memreusedist_instr_intervals,IARG_END);
		/* only called if interval is 'full' */
		INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)memreusedist_instr_interval,IARG_END);
	}
}

/* finishing... */
VOID fini_memreusedist(INT32 code, VOID* v){

	int i;

	if(interval_size == -1){
		output_file_memreusedist = fopen("memreusedist_full_int_pin.out","w");
	}
	else{
		output_file_memreusedist = fopen("memreusedist_phases_int_pin.out","a");
	}
	fprintf(output_file_memreusedist, "%lld %lld", mem_ref_cnt, cold_refs);
	for(i=0; i < BUCKET_CNT; i++){
		fprintf(output_file_memreusedist, " %lld", buckets[i]);
	}
	fprintf(output_file_memreusedist,"\nnumber of instructions: %lld\n", total_ins_count);
	fclose(output_file_memreusedist);
}
