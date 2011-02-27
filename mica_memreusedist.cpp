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
extern INT64 interval_ins_count_for_hpc_alignment;
extern INT64 total_ins_count;
extern INT64 total_ins_count_for_hpc_alignment;

extern UINT32 _block_size;
UINT32 memreusedist_block_size;

ofstream output_file_memreusedist;

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

	//borderline_stack_entries[0] = stack_top; // NO! First bucket contains two entries
	// dummy entry as first borderline entry
	borderline_stack_entries[0] = (stack_entry*)malloc(sizeof(stack_entry));
	borderline_stack_entries[0]->block_addr = 0;
	borderline_stack_entries[0]->bucket = -1;
	borderline_stack_entries[0]->next = NULL;
	borderline_stack_entries[0]->prev = NULL;
	borderline_stack_entries[0]->bucket = 0;

	stack_size = 0;

        memreusedist_block_size = _block_size;

	if(interval_size != -1){		
		output_file_memreusedist.open("memreusedist_phases_int_pin.out", ios::out|ios::trunc);
		output_file_memreusedist.close();
	}
}

/*VOID memreusedist_instr_full(){
	// counting instructions is done in all_instr_full()

}*/

ADDRINT memreusedist_instr_intervals(){

	/* counting instructions is done in all_instr_intervals() */

	return (ADDRINT)(interval_ins_count_for_hpc_alignment == interval_size);
}

VOID memreusedist_instr_interval_output(){
	int i;
	output_file_memreusedist.open("memreusedist_phases_int_pin.out", ios::out|ios::app);
	output_file_memreusedist << mem_ref_cnt << " " << cold_refs;
	for(i=0; i < BUCKET_CNT; i++){
		output_file_memreusedist << " " << buckets[i];
	}
	output_file_memreusedist << endl;
	output_file_memreusedist.close();
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
	interval_ins_count_for_hpc_alignment = 0;
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
		b = (block_fast*)malloc(sizeof(block_fast));
		/*if((b = (block_fast*)malloc(sizeof(block_fast))) == (block_fast*)NULL){
			cerr << "Not enough memory (in install)" << endl;
			exit(1);
		}*/
		table[index] = b;
	}
	else{
		while(b->next != (block_fast*)NULL){
			b = b->next;	
		}
		b->next = (block_fast*)malloc(sizeof(block_fast));
		/*if((b->next = (block_fast*)malloc(sizeof(block_fast))) == (block_fast*)NULL){
			cerr << "Not enough memory (in install (2))" << endl;
			exit(1);
		}*/
		b = b->next;	
	}
	b->next = (block_fast*)NULL;
	b->id = key;
	for(i = 0; i < MAX_MEM_ENTRIES; i++){
		b->stack_entries[i] = (stack_entry*)NULL;
	}
	return b->stack_entries;
}

VOID print_stack(stack_entry* top){

	stack_entry* e;
	int i;

	e = top;
	i = 0;
	while(e != NULL){

		fprintf(stderr, "[%d] 0x%llx, b: %d (next: 0x%llx, prev: 0x%llx)\n", i, (unsigned long long)e, e->bucket, (unsigned long long)e->next, (unsigned long long)e->prev);
		i++;
		e = e->prev;
	}
	fprintf(stderr, "------------------------\n");
}

VOID stack_sanity_check(stack_entry* top){

	int cnt=0;
	int bucket = 0;
	stack_entry *e;

	e = top;

	if(top->next != NULL){
		fprintf(stderr, "ERROR! top->next != NULL\n");
		print_stack(top);
		exit(-1);
	}

	while(e != NULL){

		cnt++;

		if(cnt > (2 << bucket)){
			fprintf(stderr, "ERROR @ [%d]! Bucket too big (cnt: %d, bucket: %d, max. size: %d)\n", cnt-1, cnt, bucket, 2 << bucket);
			print_stack(top);
			exit(-1);
		}

		if(e->bucket != bucket){
			fprintf(stderr, "ERROR @ [%d]! Bucket doesn't match @ 0x%llx (b: %d != %d)!\n", cnt-1, (unsigned long long)e, e->bucket, bucket);
			print_stack(top);
			exit(-1);
		}

		if(e == borderline_stack_entries[bucket])
			bucket++;

		e = e->prev;
	}
	//fprintf(stderr, "STACK ok!\n");
}

/* stack support */
VOID move_to_top_fast(stack_entry *e, ADDRINT a, stack_entry** top){
	
	INT32 i;

	/* check if entry was accessed before */	
	if(e != (stack_entry*)NULL){

		/* check to see if we already are at top of stack */
		if(e != *top){

			// if entry touched is borderline entry, new borderline entry is the one above the touched one (i.e. ->next)
			if(e->bucket > 0 && e == borderline_stack_entries[e->bucket]){
				borderline_stack_entries[e->bucket] = borderline_stack_entries[e->bucket]->next;
			}

			// take entry out of stack, update entries above and below accordingly
			if(e->prev != (stack_entry*)NULL){ // avoid referencing prev for bottom of stack
				e->prev->next = e->next;
			}
			e->next->prev = e->prev;

			// adjust all borderline entries above the entry touched (start with i=2 to avoid problems with too small stacks)
			for(i=2; i < BUCKET_CNT && i <= e->bucket; i++){
				borderline_stack_entries[i-1]->bucket++;
				borderline_stack_entries[i-1] = borderline_stack_entries[i-1]->next;
			}

			// place new entry on top of LRU stack
			e->prev = *top;
			e->next = (stack_entry*)NULL; // e will be the next top
			(*top)->next = e; // current top will slide down
			if(e != borderline_stack_entries[0]){
				borderline_stack_entries[0]->bucket++; // borderline stack entry for first bucket moves to next bucket, unless it's the same as the borderline entry
			}
			//(*top)->bucket = 1; // current top slides into next bucket (INCORRECT, because first bucket contains top *and* previous top)
			borderline_stack_entries[0] = (*top); // current top is borderline stack entry for first bucket

			*top = e; // set new top of stack
			e->bucket = 0; // set bucket for new top of stack

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
		//(*top)->bucket = 1; // current top slides into next bucket (INCORRECT, because first bucket contains top *and* previous top)
		borderline_stack_entries[0] = (*top);

		// set new entry as top of stack
		*top = e;
		e->bucket = 0;
		
		stack_size++;

		// adjust bucket for borderline entries (except for very last bucket = overflow bucket)
		for(i=2; i < BUCKET_CNT && (1 << i) <= stack_size; i++){
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
	//stack_sanity_check(*top);
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

	/* D-stream (64-byte) cache memory footprint */

	addr = effMemAddr >> memreusedist_block_size;
	endAddr = (effMemAddr + size - 1) >> memreusedist_block_size;

        if(size > 0){
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
		output_file_memreusedist.open("memreusedist_full_int_pin.out", ios::out|ios::trunc);
	}
	else{
		output_file_memreusedist.open("memreusedist_phases_int_pin.out", ios::out|ios::app);
	}
	output_file_memreusedist << mem_ref_cnt << " " << cold_refs;
	for(i=0; i < BUCKET_CNT; i++){
		output_file_memreusedist << " " << buckets[i];
	}
        output_file_memreusedist << endl << "number of instructions: " << total_ins_count_for_hpc_alignment << endl;
	output_file_memreusedist.close();
}
