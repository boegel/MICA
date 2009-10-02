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
 *  === Collecting microarchitecture-independent program phase characteristics using PIN ===
 * 
 *  source code by Kenneth Hoste (Ghent University, Belgium), December 2006 - September 2007
 *  based on code written by Lieven Eeckhout (for ATOM)
 *
 * PLEASE DO NOT REDISTRIBUTE THIS CODE WITHOUT INFORMING THE AUTHORS.
 *
 * Contact: Kenneth.Hoste@elis.ugent.be , Lieven.Eeckhout@elis.ugent.be
 * 
 * ILP (instr.win.size: 32,64,128,256) [4]
 *   measured by assuming perfect branch prediction, perfect caches, infinite number of registers, ...
     only limits are data dependences and instruction window size
 * instruction mix [10]
 *   - instructions which read memory (load)
 *   - instruction that write to memory (store)
 *   - control flow instructions
 *   - arithmetic instructions (integer)
 *   - floating point instructions
 *   - pop/push instructions (stack usage)
 *   - [!] shift instructions (bitwise) 
 *   - [!] string instructions
 *   - [!] SSE instructions
 *   - other (interrupts, rotate instructions, semaphore, conditional move, system, miscellaneous)  
 * [!] = rare instructions 
 * branch predictability (PPM) [14]
 *   - GAg: global branch history, one global predictor table shared by all branches
 *   - PAg: local branch history, one global predictor table shared by all branches
 *   - GAs: global branch history, seperate global predictor tables per branch
 *   - PAs: local branch history, seperate global predictor tables per branch
 *   number of history bits is set to 12/8/4 (3 different predictors)
 *   the transition and taken rate is averaged over branches
 * register traffic [9]
 *   - average number of input operands per instruction
 *   - average degree of use 
 *     (average number of consumations (register read) since production (register write)
 *   - register dependency distance (number of dynamic instructions between writing a register and reading it)
 *     prob. reg. dep. dist. = 1, <= 2, <= 4, <= 8, <= 16, <= 32, <= 64
 * data stream strides [28]
 *   - prob. local read stride. = 0, <= 8 (2^3), <= 64 (2^6), <= 512 (2^9), <= 4096 (2^12), <= 32768 (2^15), <= 262144 (2^18)
 *   - prob. local write stride. = 0, <= 8 (2^3), <= 64 (2^6), <= 512 (2^9), <= 4096 (2^12), <= 32768 (2^15), <= 262144 (2^18)
 *   - prob. global read stride. = 0, <= 8 (2^3), <= 64 (2^6), <= 512 (2^9), <= 4096 (2^12), <= 32768 (2^15), <= 262144 (2^18)
 *   - prob. global write stride. = 0, <= 8 (2^3), <= 64 (2^6), <= 512 (2^9), <= 4096 (2^12), <= 32768 (2^15), <= 262144 (2^18)
 * memfootprint sizes [4]
 *   - number of unique 4KB pages tocuhed for instruction accesses
 *   - number of unique 32-byte blocks tocuhed for instruction accesses
 *   - number of unique 4KB pages tocuhed for data accesses
 *   - number of unique 32-byte blocks tocuhed for data accesses
 *
 * TOTAL: 69 microarchitecture-independent program characteristics
 *
 * Usage: pin -t mica <'full'| interval size> < all | ... > -- <application command line>
 */

#include <stdio.h>
#include <string.h>
#include "pin.H"

#define CHAR_CNT 69
#define TMP_OUTPUT_SIZE 1000000000

#define ALL 803637001
#define ILP 803637002
#define ITYPES 803637003
#define PPM 803637004
#define REG_MODE 803637005
#define STRIDE 803637006
#define MEMFOOTPRINT 803637007
#define ILP_MEMFOOTPRINT 803637008

FILE* instr_count_output;
int mode;


/* ==========================================
 *                 SHARED (variables)
 * ========================================== */

#define NUM_WINDOW_SIZES 4
#define MAX_MEM_ENTRIES 65536
#define LOG_MAX_MEM_ENTRIES 16
#define LOG_MAX_MEM_BLOCK 16
#define MAX_MEM_BLOCK MAX_MEM_ENTRIES
#define MAX_MEM_BLOCK_ENTRIES 65536

/* linked list typedefs (copied from ATOM instrumentation files (c) Lieven Eeckhout) */
typedef struct memNode_type{
	/* ilp */
	INT32 timeAvailable[MAX_MEM_ENTRIES];
	/* memfootprint */
	char numReferenced [MAX_MEM_BLOCK];
} memNode;

int size_pow_times;
INT64 index_all_times;
UINT64* all_times[NUM_WINDOW_SIZES];

void increase_size_all_times(){
	int i;
	UINT64* ptr;
	size_pow_times++;
	for(i=0; i < NUM_WINDOW_SIZES; i++){
		ptr = (UINT64*)realloc(all_times[i],(1 << size_pow_times)*sizeof(UINT64));
		if(ptr == (UINT64*)NULL){
			fprintf(stderr,"Could not allocate memory (realloc)!\n");
			exit(1);
		}
		all_times[i] = ptr;
	}
}

typedef struct nlist_type {
	ADDRINT id;
	memNode* mem;
	struct nlist_type* next;
} nlist;

/* translation from instruction addres to index */
ADDRINT* indices_memRead;
UINT32 indices_memRead_size;
ADDRINT* indices_memWrite;
UINT32 indices_memWrite_size;
ADDRINT* indices_condBr;
UINT32 indices_condBr_size;

/* ==========================================
 *                 ILP (variables)
 * ========================================== */

#define MAX_NUM_REGS 4096
#define MAX_MEM_TABLE_ENTRIES 2048

/* output files */
FILE* out_file_ilp;
FILE* out_file_itypes;
FILE* out_file_ppm;
FILE* out_file_reg;
FILE* out_file_stride;
FILE* out_file_memfootprint;

/* The running count of instructions is kept here */
INT64 total_count;

BOOL full_run;
/* The instruction interval size */
INT64 interval_size;
INT64 total_count_interval;
INT64 cpuClock_interval[NUM_WINDOW_SIZES];

/* Global variables */
UINT32 win_size[NUM_WINDOW_SIZES];
UINT64 timeAvailable[NUM_WINDOW_SIZES][MAX_NUM_REGS];
nlist* memAddressesTable[MAX_MEM_TABLE_ENTRIES];
UINT32 windowHead[NUM_WINDOW_SIZES];
UINT32 windowTail[NUM_WINDOW_SIZES];
UINT64 cpuClock[NUM_WINDOW_SIZES];
UINT64* executionProfile[NUM_WINDOW_SIZES];
UINT64 issueTime[NUM_WINDOW_SIZES];

/* ==========================================
 *             ITYPES (variables)
 * ========================================== */

INT64 mem_read_cnt;
INT64 mem_write_cnt;
INT64 control_cnt;
INT64 arith_cnt;
INT64 fp_cnt;
INT64 stack_cnt;
INT64 shift_cnt;
INT64 string_cnt;
INT64 sse_cnt;
INT64 other_cnt;

/* ==========================================
 *          PPM (variables)
 * ========================================== */

#define MAX_HIST_LENGTH 12
#define NUM_HIST_LENGTHS 3

/* Global variables */
BOOL lastInstBr; // was the last instruction a cond. branch instruction?
ADDRINT nextAddr; // address of the instruction after the last cond.branch
UINT32 numStatCondBranchInst; // number of static cond. branch instructions up until now (-> unique id for the cond. branch)
UINT32 lastBrId; // index of last cond. branch instruction
INT64* transition_counts;
char* local_taken;
INT64* local_taken_counts;
INT64* local_brCounts;
/* incorrect predictions counters */
INT64 GAg_incorrect_pred[NUM_HIST_LENGTHS];
INT64 GAs_incorrect_pred[NUM_HIST_LENGTHS];
INT64 PAg_incorrect_pred[NUM_HIST_LENGTHS];
INT64 PAs_incorrect_pred[NUM_HIST_LENGTHS];
/* prediction for each of the 4 predictors */
INT32 GAg_pred_taken[NUM_HIST_LENGTHS];
INT32 GAs_pred_taken[NUM_HIST_LENGTHS];
INT32 PAg_pred_taken[NUM_HIST_LENGTHS];
INT32 PAs_pred_taken[NUM_HIST_LENGTHS];
/* size of local pattern history */
INT64 brHist_size;
/* global/local history */
INT32 bhr;
INT32* local_bhr;
/* global/local pattern history tables */
char history_lengths[NUM_HIST_LENGTHS] = {12,8,4};
char*** GAg_pht;
char*** PAg_pht;
char**** GAs_pht;
char**** PAs_pht;
/* */
char* GAs_touched;
char* PAs_touched;
/* prediction history */
int GAg_pred_hist[NUM_HIST_LENGTHS];
int PAg_pred_hist[NUM_HIST_LENGTHS];
int GAs_pred_hist[NUM_HIST_LENGTHS];
int PAs_pred_hist[NUM_HIST_LENGTHS]; 

/* ==========================================
 *         REGISTER TRAFFIC (variables)
 * ========================================== */

#define MAX_NUM_OPER 7
#define MAX_DIST 128
#define MAX_COMM_DIST MAX_DIST
#define MAX_REG_USE MAX_DIST

/* Global variables */
UINT64* opCounts; // array which keeps track of number-of-operands-per-instruction stats
BOOL* regRef; // register references
INT64* PCTable; // production addresses of registers
INT64* regUseCnt; // usage counters for each register
INT64* regUseDistr; // distribution of register usage
INT64* regAgeDistr; // distribution of register ages

/* ==========================================
 *              STRIDE (variables)
 * ========================================== */

#define MAX_DISTR 524288 // 2^21

/* Global variables */
UINT32 readIndex;
UINT32 writeIndex;
UINT64 numInstrsAnalyzed;
UINT64 numReadInstrsAnalyzed;
UINT64 numWriteInstrsAnalyzed;
UINT64 numRead, numWrite;
ADDRINT* instrRead;
ADDRINT* instrWrite;
UINT64 localReadDistrib [MAX_DISTR];
UINT64 globalReadDistrib [MAX_DISTR];
UINT64 localWriteDistrib [MAX_DISTR];
UINT64 globalWriteDistrib [MAX_DISTR];
ADDRINT lastReadAddr;
ADDRINT lastWriteAddr;

/* ==========================================
 *             MEMFOOTPRINT (variables)
 * ========================================== */

/* Global variables */
nlist* DmemCacheWorkingSetTable [MAX_MEM_BLOCK_ENTRIES];
nlist* DmemPageWorkingSetTable [MAX_MEM_BLOCK_ENTRIES];
nlist* ImemCacheWorkingSetTable [MAX_MEM_BLOCK_ENTRIES];
nlist* ImemPageWorkingSetTable [MAX_MEM_BLOCK_ENTRIES];

/* ==========================================
 *               ILP (functions)
 * ========================================== */

/* nothing ILP specific, see shared functions */

/* ==========================================
 *             ITYPES (functions)
 * ========================================== */

/* functions to increment suitable instruction mix variable */

VOID count_mem_read() { mem_read_cnt++; }

VOID count_mem_write() { mem_write_cnt++; }

VOID count_control() { control_cnt++; }

VOID count_arith() { arith_cnt++; }

VOID count_fp() { fp_cnt++; }

VOID count_stack() { stack_cnt++; }

VOID count_shift() { shift_cnt++; }

VOID count_string() { string_cnt++; }

VOID count_sse() { sse_cnt++; }

VOID count_other() { other_cnt++; }

/* ==========================================
 *                PPM (functions)
 * ========================================== */

/* double memory space for branch history size when needed */
VOID reallocate_brHist(){

	INT32* int_ptr;
	char* char_ptr;
	char**** char4_ptr;
	INT64* int64_ptr;

	brHist_size = brHist_size*2;

	int_ptr = (INT32*) realloc (local_bhr,brHist_size * sizeof(INT32));
	if(int_ptr == (INT32*) NULL) {
		fprintf(stderr,"Could not allocate memory\n");
		exit(1);
	}
	local_bhr = int_ptr;

	char_ptr = (char*) realloc (GAs_touched, brHist_size * sizeof(char));
	if(char_ptr == (char*) NULL){
		fprintf(stderr,"Could not allocate memory\n");
		exit(1);
	}
	GAs_touched = char_ptr;

	char4_ptr = (char****) realloc (GAs_pht,brHist_size * sizeof(char***));
	if(char4_ptr == (char****) NULL) { 
		fprintf(stderr,"Could not allocate memory\n");
		exit(1);
	}
	GAs_pht = char4_ptr;

	char_ptr = (char*) realloc (PAs_touched,brHist_size * sizeof(char));
	if(char_ptr == (char*) NULL) {	
		fprintf(stderr,"Could not allocate memory\n");
		exit(1);
	}
	PAs_touched = char_ptr;

	char4_ptr = (char****) realloc (PAs_pht,brHist_size * sizeof(char***));
	if(char4_ptr == (char****) NULL) {
		fprintf(stderr,"Could not allocate memory\n");
		exit(1);
	}
	PAs_pht = char4_ptr;

	char_ptr = (char*) realloc (local_taken,brHist_size * sizeof(char));
	if(char_ptr == (char*) NULL) {	
		fprintf(stderr,"Could not allocate memory\n");
		exit(1);
	}
	local_taken = char_ptr;

	int64_ptr = (INT64*) realloc(transition_counts, brHist_size * sizeof(INT64));
	if(int64_ptr == (INT64*)NULL) {
		fprintf(stderr,"Could not allocate memory\n");
		exit(1);
	}
	transition_counts = int64_ptr;

	int64_ptr = (INT64*) realloc(local_brCounts, brHist_size * sizeof(INT64));
	if(int64_ptr == (INT64*)NULL) {
		fprintf(stderr,"Could not allocate memory\n");
		exit(1);
	}
	local_brCounts = int64_ptr;

	int64_ptr = (INT64*) realloc(local_taken_counts, brHist_size * sizeof(INT64));
	if(int64_ptr == (INT64*)NULL) {
		fprintf(stderr,"Could not allocate memory\n");
		exit(1);
	}
	local_taken_counts = int64_ptr;
}

/* When the instruction is a conditional branch, predict taken/not taken for GAg, GAs, PAg and PAs */
VOID condBr(ADDRINT addr, UINT32 id){
	int i,j, k, hist = 0;
	int mem_cnt = 0;
	/* set variables to for next prediction evaluation */
	nextAddr = addr;
	lastInstBr = true;
	lastBrId = id;

	/* GAs PPM predictor lookup */
	if(!GAs_touched[id]){
		/* allocate PPM predictor */

		GAs_touched[id] = 1;

		if((GAs_pht[id] = ((char***) malloc (NUM_HIST_LENGTHS * sizeof(char**)))) == (char***) NULL) {
			fprintf(stderr,"Could not allocate memory)\n");
			exit(1);
		}

		for(j = 0; j < NUM_HIST_LENGTHS; j++){	
			if((GAs_pht[id][j] = ((char**) malloc ((history_lengths[j]+1) * sizeof(char*)))) == (char**) NULL) {
				fprintf(stderr,"Could not allocate memory)\n");
				exit(1);
			}

			for(i = 0; i <= history_lengths[j]; i++){
				if((GAs_pht[id][j][i] = (char*) malloc((1 << i) * sizeof(char))) == (char*) NULL) {
					fprintf(stderr,"Could not allocate memory\n");
					exit(1);
				}
				for(k = 0; k < (1<<i); k++){
					GAs_pht[id][j][i][k] = -1;
				}
			}
		}
	}

	/* PAs PPM predictor lookup */
	if(!PAs_touched[id]){
		/* allocate PPM predictor */

		PAs_touched[id] = 1;

		if((PAs_pht[id] = ((char***) malloc (NUM_HIST_LENGTHS * sizeof(char**)))) == (char***) NULL) {
			fprintf(stderr,"Could not allocate memory\n");
			exit(1);
		}

		for(j = 0; j < NUM_HIST_LENGTHS; j++){	
			if((PAs_pht[id][j] = ((char**) malloc ((history_lengths[j]+1) * sizeof(char*)))) == (char**) NULL) {
				fprintf(stderr,"Could not allocate memory\n");
				exit(1);
			}

			mem_cnt = 0;
			for(i = 0; i <= history_lengths[j]; i++){
				if((PAs_pht[id][j][i] = (char*) malloc((1 << i) * sizeof(char))) == (char*) NULL) {
					fprintf(stderr,"Could not allocate memory\n");
					exit(1);
				}
				for(k = 0; k < (1 << i); k++){
					PAs_pht[id][j][i][k] = -1;
				}
			}
		}
	}

	for(j = 0; j < NUM_HIST_LENGTHS; j++){	
		/* GAg PPM predictor lookup */
		for(i = history_lengths[j]; i >= 0; i--){

			hist = bhr & (((int) 1 << i) -1);
			if(GAg_pht[j][i][hist] != 0){
				GAg_pred_hist[j] = i; // used to only update predictor doing the prediction and higher order predictors (update exclusion)
				if(GAg_pht[j][i][hist] > 0)
					GAg_pred_taken[j] = 1;
				else
					GAg_pred_taken[j] = 0;
				break;
			}
		}

		/* PAg PPM predictor lookup */
		for(i = history_lengths[j]; i >= 0; i--){
			hist = local_bhr[id] & (((int) 1 << i) -1);
			if(PAg_pht[j][i][hist] != 0){
				PAg_pred_hist[j] = i;
				if(PAg_pht[j][i][hist] > 0)
					PAg_pred_taken[j] = 1;
				else
					PAg_pred_taken[j] = 0;
				break;
			}
		}

		/* GAs PPM predictor lookup */
		for(i = history_lengths[j]; i >= 0; i--){
			hist = bhr & (((int) 1 << i) -1);
			if(GAs_pht[id][j][i][hist] != 0){
				GAs_pred_hist[j] = i;
				if(GAs_pht[id][j][i][hist] > 0)
					GAs_pred_taken[j] = 1;
				else
					GAs_pred_taken[j] = 0;
				break;
			}
		}

		/* PAs PPM predictor lookup */
		for(i = history_lengths[j]; i >= 0; i--){
			hist = local_bhr[id] & (((int) 1 << i) -1);
			if(PAs_pht[id][j][i][hist] != 0){
				PAs_pred_hist[j] = i;
				if(PAs_pht[id][j][i][hist] > 0)
					PAs_pred_taken[j] = 1;
				else
					PAs_pred_taken[j] = 0;
				break;
			}
		}
	}
}

/* Update PPM history tables according to last prediction */
VOID checkUpdate(ADDRINT addr){
	int i, j, hist;
	int taken;

	/* If the last instruction was a branch, the PPM history tables
	 * should be updated according to the prediction that was made 
	 */

	if(lastInstBr){ /* check if last (dynamic) instruction was a branch */

		/* determine branch taken/not taken */
		if(nextAddr!=addr)
			taken = 1;
		else
			taken = 0;

		/* transition rate */
		if(local_taken[lastBrId] > -1){
			if(taken != local_taken[lastBrId])
				transition_counts[lastBrId]++;
		}
		local_taken[lastBrId] = taken;
		local_brCounts[lastBrId]++;
		if(taken)
			local_taken_counts[lastBrId]++;

		for(j=0; j < NUM_HIST_LENGTHS; j++){	
			/* update statistics according to predictions */
			if(taken != GAg_pred_taken[j])
				GAg_incorrect_pred[j]++;
			if(taken != GAs_pred_taken[j])
				GAs_incorrect_pred[j]++;
			if(taken != PAg_pred_taken[j])
				PAg_incorrect_pred[j]++;
			if(taken != PAs_pred_taken[j])
				PAs_incorrect_pred[j]++;

			/* using update exclusion: only update predictor doing the prediction and higher order predictors */

			/* update GAg PPM pattern history tables */		
			for(i = GAg_pred_hist[j]; i <= history_lengths[j]; i++){
				hist = bhr & ((1 << i) - 1);
				if(taken){
					if(GAg_pht[j][i][hist] < 127)
						GAg_pht[j][i][hist]++;
				}
				else{
					if(GAg_pht[j][i][hist] > -127)
						GAg_pht[j][i][hist]--;
				}
				/* avoid == 0 because that means 'not set' */
				if(GAg_pht[j][i][hist] == 0){
					if(taken){
						GAg_pht[j][i][hist]++;
					}
					else{
						GAg_pht[j][i][hist]--;
					}
				}
			}
			/* update PAg PPM pattern history tables */		
			for(i = PAg_pred_hist[j]; i <= history_lengths[j]; i++){
				hist = local_bhr[lastBrId] & ((1 << i) - 1);
				if(taken){
					if(PAg_pht[j][i][hist] < 127)
						PAg_pht[j][i][hist]++;
				}
				else{
					if(PAg_pht[j][i][hist] > -127)
						PAg_pht[j][i][hist]--;
				}
				/* avoid == 0 because that means 'not set' */
				if(PAg_pht[j][i][hist] == 0){
					if(taken){
						PAg_pht[j][i][hist]++;
					}
					else{
						PAg_pht[j][i][hist]--;
					}
				}
			}
			/* update GAs PPM pattern history tables */		
			for(i = GAs_pred_hist[j]; i <= history_lengths[j]; i++){
				hist = bhr & ((1 << i) - 1);
				if(taken){
					if(GAs_pht[lastBrId][j][i][hist] < 127)
						GAs_pht[lastBrId][j][i][hist]++;
				}
				else{
					if(GAs_pht[lastBrId][j][i][hist] > -127)
						GAs_pht[lastBrId][j][i][hist]--;
				}
				/* avoid == 0 because that means 'not set' */
				if(GAs_pht[lastBrId][j][i][hist] == 0){
					if(taken){
						GAs_pht[lastBrId][j][i][hist]++;
					}
					else{
						GAs_pht[lastBrId][j][i][hist]--;
					}
				}
			}
			/* update PAs PPM pattern history tables */		
			for(i = PAs_pred_hist[j]; i <= history_lengths[j]; i++){
				hist = local_bhr[lastBrId] & ((1 << i) - 1);
				if(taken){
					if(PAs_pht[lastBrId][j][i][hist] < 127)
						PAs_pht[lastBrId][j][i][hist]++;
				}
				else{
					if(PAs_pht[lastBrId][j][i][hist] > -127)
						PAs_pht[lastBrId][j][i][hist]--;
				}
				/* avoid == 0 because that means 'not set' */
				if(PAs_pht[lastBrId][j][i][hist] == 0){
					if(taken){
						PAs_pht[lastBrId][j][i][hist]++;
					}
					else{
						PAs_pht[lastBrId][j][i][hist]--;
					}
				}
			}
		}

		/* update global history register */
		bhr = bhr << 1;
		bhr |= taken;

		/* update local history */
		local_bhr[lastBrId] = local_bhr[lastBrId] << 1;
		local_bhr[lastBrId] |= taken;


		lastInstBr = false;
	}
}

/* ==========================================
 *                REG (functions)
 * ========================================== */

/* none specific, see shared functions */

/* ==========================================
 *               STRIDE (functions)
 * ========================================== */

/* We don't know the static number of read/write operations until 
 * the entire program has executed, hence we dynamically allocate the arrays */
VOID reallocate_readArray(){

	ADDRINT* ptr;

	numRead *= 2;

	ptr = (ADDRINT*) realloc (instrRead, numRead * sizeof (ADDRINT));
	if (ptr == (ADDRINT*) NULL) {
		fprintf (stderr, "Not enough memory (in reallocate_readArray)\n");
		exit (1);
	}
	instrRead = ptr;
}

VOID reallocate_writeArray(){

	ADDRINT* ptr;

	numWrite *= 2;

	ptr = (ADDRINT*) malloc (numWrite * sizeof (ADDRINT));
	if (ptr == (ADDRINT*) NULL) {
		fprintf (stderr, "Not enough memory (in reallocate_writeArray)\n");
		exit (1);
	}
	instrWrite = ptr;
}

/* ==========================================
 *               SHARED (functions)
 * ========================================== */

/* Linked list functions, copied from ATOM instrumentation files (c) Lieven Eeckhout */
memNode* lookup (nlist** table, ADDRINT key){

	nlist* np;

	for (np = table[key % MAX_MEM_TABLE_ENTRIES]; np != (nlist*)NULL; np = np->next){
		if(np-> id == key)
			return np->mem;
	}

	return (memNode*)NULL;
}

memNode* install(nlist** table, ADDRINT key){

	nlist* np;
	int index, i; // j;

	index = key % MAX_MEM_TABLE_ENTRIES;

	np = table[index];

	if(np == (nlist*)NULL) {
		if((np = (nlist*)malloc(sizeof(nlist))) == (nlist*)NULL){
			fprintf(stderr,"Not enough memory (in install)\n");
			exit(1);
		}
		table[index] = np;
	}
	else{
		while(np->next != (nlist*)NULL){
			np = np->next;	
		}
		if((np->next = (nlist*)malloc(sizeof(nlist))) == (nlist*)NULL){
			fprintf(stderr,"Not enough memory (in install (2))\n");
			exit(1);
		}
		np = np->next;	
	}
	np->next = (nlist*)NULL;
	np->id = key;
	if((np->mem = (memNode*)malloc (sizeof(memNode))) == (memNode*)NULL){
		fprintf(stderr,"Not enough memory (in install (3))\n");
		exit(1);
	}
	for(i = 0; i < MAX_MEM_ENTRIES; i++){
		(np->mem)->timeAvailable[i] = 0;
	}
	return (np->mem);
}

/* Finds indices for instruction at some address, given some list of index-instruction pairs 
 * Note: the 'nth_occur' argument is needed because a single instruction can have two read memory operands (which both have a different index) */ 
UINT32 index_memRead(int nth_occur, ADDRINT ins_addr){

	UINT32 i;
	int j=0;
	for(i=1; i <= readIndex; i++){
		if(indices_memRead[i] == ins_addr)
			j++;
		if(j==nth_occur)
			return i; /* found */
	}
	return 0; /* not found */
}

UINT32 index_memWrite(ADDRINT ins_addr){

	UINT32 i;
	for(i=1; i <= writeIndex; i++){
		if(indices_memWrite[i] == ins_addr)
			return i; /* found */
	}
	return 0; /* not found */
}

UINT32 index_condBr(ADDRINT ins_addr){

	UINT64 i;
	for(i=0; i <= numStatCondBranchInst; i++){
		if(indices_condBr[i] == ins_addr)
			return i; /* found */
	}
	return 0; /* not found */
}


/* Register new instruction to an index in the given list */
void register_memRead(ADDRINT ins_addr){

	ADDRINT* ptr;

	/* reallocation needed */
	if(readIndex >= indices_memRead_size){

		indices_memRead_size *= 2;
		ptr = (ADDRINT*) realloc(indices_memRead, indices_memRead_size*sizeof(ADDRINT));
		if(ptr == (ADDRINT*)NULL){
			fprintf(stderr,"Could not allocate memory (realloc in register_readMem)!\n");
			exit(1);
		}
		indices_memRead = ptr;

	}

	/* register instruction to index */
	indices_memRead[readIndex++] = ins_addr;
}

void register_memWrite(ADDRINT ins_addr){

	ADDRINT* ptr;

	/* reallocation needed */
	if(writeIndex >= indices_memWrite_size){

		indices_memWrite_size *= 2;
		ptr = (ADDRINT*) realloc(indices_memWrite, indices_memWrite_size*sizeof(ADDRINT));
		if(ptr == (ADDRINT*)NULL){
			fprintf(stderr,"Could not allocate memory (realloc in register_writeMem)!\n");
			exit(1);
		}
		indices_memWrite = ptr;

	}

	/* register instruction to index */
	indices_memWrite[writeIndex++] = ins_addr;
}

void register_condBr(ADDRINT ins_addr){

	ADDRINT* ptr;

	/* reallocation needed */
	if(numStatCondBranchInst >= indices_condBr_size){

		indices_condBr_size *= 2;
		ptr = (ADDRINT*) realloc(indices_condBr, indices_condBr_size*sizeof(ADDRINT));
		if(ptr == (ADDRINT*)NULL){
			fprintf(stderr,"Could not allocate memory (realloc in register_condBr)!\n");
			exit(1);
		}
		indices_condBr = ptr;

	}

	/* register instruction to index */
	indices_condBr[numStatCondBranchInst++] = ins_addr;
}

VOID instr_ppm(ADDRINT instrAddr, UINT32 regOpCnt) {
	int i,j;
	if(total_count%TMP_OUTPUT_SIZE == 0){
		fprintf(stderr,"ppm (TMP) @ %lld -> %lld",(long long)total_count,(long long)total_count);
		for(j=0; j < NUM_HIST_LENGTHS; j++)
			fprintf(stderr," %lld %lld %lld %lld",(long long)GAg_incorrect_pred[j],(long long)PAg_incorrect_pred[j],(long long)GAs_incorrect_pred[j],(long long)PAs_incorrect_pred[j]);
		INT64 total_transition_count = 0;
		INT64 total_taken_count = 0;
		INT64 total_brCount = 0;
		for(j=0; j < brHist_size; j++){
			if(local_brCounts[j] > 0){
				if( transition_counts[j] > local_brCounts[j]/2)
					total_transition_count += local_brCounts[j]-transition_counts[j];
				else
					total_transition_count += transition_counts[j];

				if( local_taken_counts[j] > local_brCounts[j]/2)
					total_taken_count += local_brCounts[j] - local_taken_counts[j];
				else
					total_taken_count += local_taken_counts[j];
				total_brCount += local_brCounts[j];
			}
		}
		fprintf(stderr," %lld %lld %lld\n",(long long)total_brCount,(long long)total_transition_count,(long long)total_taken_count); 
	}
	if( !full_run && (total_count > 0 && total_count%interval_size == 0)){

		/* *** PPM *** */
		fprintf(out_file_ppm,"%lld",(long long)total_count_interval);
		for(j=0; j < NUM_HIST_LENGTHS; j++)
			fprintf(out_file_ppm," %lld %lld %lld %lld",(long long)GAg_incorrect_pred[j],(long long)PAg_incorrect_pred[j],(long long)GAs_incorrect_pred[j],(long long)PAs_incorrect_pred[j]);
		INT64 total_transition_count = 0;
		INT64 total_taken_count = 0;
		INT64 total_brCount = 0;
		for(j=0; j < brHist_size; j++){
			if(local_brCounts[j] > 0){
				if( transition_counts[j] > local_brCounts[j]/2)
					total_transition_count += local_brCounts[j]-transition_counts[j];
				else
					total_transition_count += transition_counts[j];

				if( local_taken_counts[j] > local_brCounts[j]/2)
					total_taken_count += local_brCounts[j] - local_taken_counts[j];
				else
					total_taken_count += local_taken_counts[j];
				total_brCount += local_brCounts[j];
			}
		}
		fprintf(out_file_ppm," %lld %lld %lld\n",(long long)total_brCount,(long long)total_transition_count,(long long)total_taken_count);

		/* RESET */
		for(j = 0; j < NUM_HIST_LENGTHS; j++){
			GAg_incorrect_pred[j] = 0;
			GAs_incorrect_pred[j] = 0;
			PAg_incorrect_pred[j] = 0;
			PAs_incorrect_pred[j] = 0;
		}
		for(i=0; i < brHist_size; i++){
			local_brCounts[i] = 0;
			local_taken_counts[i] = 0;
			transition_counts[i] = 0;
			// local_taken[i] = -1;
		}
	}
}

VOID instr_stride() {

	int i;

	if( !full_run && (total_count > 0 && total_count%interval_size == 0)){

		/* *** STRIDE *** */
		UINT64 cum;

		fprintf(out_file_stride,"%lld",(long long)numReadInstrsAnalyzed);
		/* local read distribution */
		cum = 0;
		for(i = 0; i < MAX_DISTR; i++){
			cum += localReadDistrib[i];
			if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) || (i == 262144) ){
				if(cum > 0)
					fprintf(out_file_stride," %lld", (long long) cum);
				else
					fprintf(out_file_stride," %d", 0);
			}
			if(i == 262144)
				break;
		}
		/* global read distribution */
		cum = 0;
		for(i = 0; i < MAX_DISTR; i++){
			cum += globalReadDistrib[i];
			if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) || (i == 262144) ){
				if(cum > 0)
					fprintf(out_file_stride," %lld", (long long) cum);
				else	
					fprintf(out_file_stride," %d", 0);
			}
			if(i == 262144)
				break;
		}
		fprintf(out_file_stride," %lld",(long long)numWriteInstrsAnalyzed);
		/* local write distribution */
		cum = 0;
		for(i = 0; i < MAX_DISTR; i++){
			cum += localWriteDistrib[i];
			if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) || (i == 262144) ){
				if(cum > 0)
					fprintf(out_file_stride," %lld", (long long) cum);
				else	
					fprintf(out_file_stride," %d", 0);
			}
			if(i == 262144)
				break;
		}
		/* global write distribution */
		cum = 0;
		for(i = 0; i < MAX_DISTR; i++){
			cum += globalWriteDistrib[i];
			if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) ){
				if(cum > 0)
					fprintf(out_file_stride," %lld", (long long) cum);
				else	
					fprintf(out_file_stride," %d", 0);
			}
			if(i == 262144){
				if(cum > 0)
					fprintf(out_file_stride," %lld\n", (long long) cum);
				else	
					fprintf(out_file_stride," %d\n", 0);
				break;
			}
		}
		/* RESET */

		/* do NOT reset last read/write memory addresses per instruction */
		/*
		   for (i = 0; i < numRead; i++)
		   instrRead [i] = 0;
		   for(i = 0; i < numWrite; i++)
		   instrWrite [i] = 0;
		 */
		/* do NOT reset last read/write memory addresses */
		/*
		   lastReadAddr = 0;
		   lastWriteAddr = 0;
		 */
		for (i = 0; i < MAX_DISTR; i++) {
			localReadDistrib [i] = 0;
			localWriteDistrib [i] = 0;
			globalReadDistrib [i] = 0;
			globalWriteDistrib [i] = 0;
		}
		numInstrsAnalyzed = 0;
		numReadInstrsAnalyzed = 0;
		numWriteInstrsAnalyzed = 0;
	}
}

VOID instr_all() {
	/* *** output? *** */
	if( !full_run && total_count > 0 && total_count%interval_size == 0){

		/* *** RESET *** */
		total_count_interval = 0;

	}
	/* GLOBAL */

	if(total_count%1000000 == 0){
		fprintf(instr_count_output,"%lld * 10^6 instrs done\n",(long long)(total_count/1000000));	
		fflush(instr_count_output);
	}
	/*if(total_count%1000000000 == 0){
		fprintf(instr_count_output,"%lld * 10^9 instrs done\n",(long long)(total_count/1000000000));	
		fflush(instr_count_output);
	}*/

	total_count++;
	total_count_interval++;
}

VOID instr_itypes() {
	if( !full_run && (total_count > 0 && total_count%interval_size == 0)){

		/* *** ITYPES *** */
		fprintf(out_file_itypes,"%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",(long long)total_count_interval, (long long)mem_read_cnt, (long long)mem_write_cnt, (long long)control_cnt, (long long)arith_cnt, (long long)fp_cnt, (long long)stack_cnt, (long long)shift_cnt, (long long)string_cnt, (long long)sse_cnt, (long long)other_cnt);

		/* RESET */
		mem_read_cnt = 0;
		mem_write_cnt = 0;
		control_cnt = 0;
		arith_cnt = 0;
		fp_cnt = 0;
		stack_cnt = 0;
		shift_cnt = 0;
		string_cnt = 0;
		sse_cnt = 0;
		other_cnt = 0;
	}
}

VOID instr_ilp() {
	int i,j;
	/* *** ILP *** */

	if( !full_run && (total_count > 0 && total_count%interval_size == 0)){

		/* *** ILP *** */
		fprintf(out_file_ilp,"%lld",(long long)total_count_interval);
		for(j=0; j < NUM_WINDOW_SIZES; j++)
			fprintf(out_file_ilp," %lld",(long long)cpuClock_interval[j]);
		fprintf(out_file_ilp,"\n");

		/* RESET */	

		/* clean used memory, to avoid memory shortage for long (CPU2006) benchmarks */ 	
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
			np = DmemCacheWorkingSetTable[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm->mem);
				free(np_rm);
			}
			np = DmemPageWorkingSetTable[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm->mem);
				free(np_rm);
			}
			np = ImemCacheWorkingSetTable[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm->mem);
				free(np_rm);
			}
			np = ImemPageWorkingSetTable[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm->mem);
				free(np_rm);
			}
		}
		for (i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
			memAddressesTable[i] = (nlist*) NULL;
			DmemCacheWorkingSetTable[i] = (nlist*) NULL;
			DmemPageWorkingSetTable[i] = (nlist*) NULL;
			ImemCacheWorkingSetTable[i] = (nlist*) NULL;
			ImemPageWorkingSetTable[i] = (nlist*) NULL;
		}
		/* ILP */
		for(j=0; j < NUM_WINDOW_SIZES; j++)
			cpuClock_interval[j] = 0;
		size_pow_times = 10;
		index_all_times = 1;
		for(i=0; i < NUM_WINDOW_SIZES; i++){
			free(all_times[i]);
			if((all_times[i] = (UINT64*)malloc((1 << size_pow_times) * sizeof(UINT64))) == (UINT64*)NULL){
				fprintf(stderr,"Could not allocate memory\n");
				exit(1);
			}
		}
	}

	UINT32 reordered = 0;

	/* minimum issue time: CPU clock */
	for(j=0; j < NUM_WINDOW_SIZES; j++){
		if(cpuClock[j] > issueTime[j])
			issueTime[j] = cpuClock[j];
	}

	/* set issue time for tail of instruction window */
	for(j=0; j < NUM_WINDOW_SIZES; j++){
		executionProfile[j][windowTail[j]] = issueTime[j];
		windowTail[j] = (windowTail[j] + 1) % win_size[j];
	}

	for(j=0; j < NUM_WINDOW_SIZES; j++){
		/* if instruction window (issue buffer) full */
		if(windowHead[j] == windowTail[j]){
			cpuClock[j]++;
			cpuClock_interval[j]++;
			reordered = 0;
			/* remove all instructions which are ready to be issued from beginning of window, 
			 * until an instruction comes along which is not ready for issueing yet:
			 * -> check executionProfile to see which instructions are ready
			 * -> remove maximum win_size instructions (i.e. stop when issue buffer is empty)
			 */
			while((executionProfile[j][windowHead[j]] < cpuClock[j]) && (reordered < win_size[j])) {
				windowHead[j] = (windowHead[j] + 1) % win_size[j];
				reordered++;
			}
			if(reordered == 0){
				fprintf(stderr,"reordered == 0\n");
				exit(1);
			}
		}

	}

	/* reset issue time */
	for(j=0; j < NUM_WINDOW_SIZES; j++)
		issueTime[j] = 0;
}

VOID instr_reg(UINT32 regOpCnt) {
	/* *** REG *** */

	int i;

	if( !full_run && (total_count > 0 && total_count%interval_size == 0)){

		/* *** REG *** */ 
		UINT64 totNumOps = 0;
		UINT64 num;
		double avg;

		/* total number of operands */
		for(i = 1; i < MAX_NUM_OPER; i++){
			totNumOps += opCounts[i]*i;
		}
		fprintf(out_file_reg,"%lld %lld",(long long)total_count_interval,(long long)totNumOps); 

		/* average degree of use */
		num = 0;
		avg = 0.0;
		for(i = 0; i < MAX_REG_USE; i++){
			num += regUseDistr[i];
		}
		fprintf(out_file_reg," %lld",(long long)num);
		num = 0;
		for(i = 0; i < MAX_REG_USE; i++){
			num += i * regUseDistr[i];
		}
		fprintf(out_file_reg," %lld",(long long)num); 

		/* register dependency distributions */
		num = 0;
		avg = 0.0;
		for(i = 0; i < MAX_COMM_DIST; i++){
			num += regAgeDistr[i];
		}
		fprintf(out_file_reg," %lld",(long long)num);
		num = 0;
		for(i = 0; i < MAX_COMM_DIST; i++){
			num += regAgeDistr[i];
			if( (i == 1) || (i == 2) || (i == 4) || (i == 8) || (i == 16) || (i == 32) || (i == 64)){
				fprintf(out_file_reg," %lld",(long long)num);
			}
		}
		fprintf(out_file_reg,"\n");

		/* RESET */
		for(i = 0; i < MAX_NUM_OPER; i++){
			opCounts[i] = 0;
		}
		/* do NOT reset register use counts or register definition addresses
		 * that should only be done when the register is written to */
		/* for(i = 0; i < MAX_NUM_REGS; i++){
		   regRef[i] = false;
		   PCTable[i] = 0;
		   regUseCnt[i] = 0;
		   } */
		for(i = 0; i < MAX_REG_USE; i++){
			regUseDistr[i] = 0;
		}
		for(i = 0; i < MAX_COMM_DIST; i++){
			regAgeDistr[i] = 0;
		}
	}

	opCounts[regOpCnt]++;
}

VOID instr_memfootprint(ADDRINT instrAddr) {
	/* *** MEMFOOTPRINT *** */

	ADDRINT addr, upperAddr, indexInChunk;
	memNode* chunk;
	int i,j;
	nlist* np;

	/* tmp output ? */
	if(total_count%TMP_OUTPUT_SIZE == 0){
		long long DmemCacheWorkingSetSize = 0L;
		long long DmemPageWorkingSetSize = 0L;
		long long ImemCacheWorkingSetSize = 0L;
		long long ImemPageWorkingSetSize = 0L;

		for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			for (np = DmemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
				for (j = 0; j < MAX_MEM_BLOCK; j++) {
					if ((np->mem)->numReferenced [j] > 0) {
						DmemCacheWorkingSetSize++;
					}
				}
			}
		}
		for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			for (np = DmemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
				for (j = 0; j < MAX_MEM_BLOCK; j++) {
					if ((np->mem)->numReferenced [j] > 0) {
						DmemPageWorkingSetSize++;
					}
				}
			}
		}
		for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			for (np = ImemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
				for (j = 0; j < MAX_MEM_BLOCK; j++) {
					if ((np->mem)->numReferenced [j] > 0) {
						ImemCacheWorkingSetSize++;
					}
				}
			}
		}
		for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			for (np = ImemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
				for (j = 0; j < MAX_MEM_BLOCK; j++) {
					if ((np->mem)->numReferenced [j] > 0) {
						ImemPageWorkingSetSize++;
					}
				}
			}
		}
		fprintf(stderr,"memfootprint (TMP) @ %lld -> %lld: %lld : %lld : %lld\n",(long long)total_count,(long long)DmemCacheWorkingSetSize,(long long)DmemPageWorkingSetSize,(long long)ImemCacheWorkingSetSize,(long long)ImemPageWorkingSetSize); 
	}
	if( !full_run && (total_count > 0 && total_count%interval_size == 0)){

		/* *** MEMFOOTPRINT *** */

		nlist* np;
		long long DmemCacheWorkingSetSize = 0L;
		long long DmemPageWorkingSetSize = 0L;
		long long ImemCacheWorkingSetSize = 0L;
		long long ImemPageWorkingSetSize = 0L;

		for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			for (np = DmemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
				for (j = 0; j < MAX_MEM_BLOCK; j++) {
					if ((np->mem)->numReferenced [j] > 0) {
						DmemCacheWorkingSetSize++;
					}
				}
			}
		}
		for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			for (np = DmemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
				for (j = 0; j < MAX_MEM_BLOCK; j++) {
					if ((np->mem)->numReferenced [j] > 0) {
						DmemPageWorkingSetSize++;
					}
				}
			}
		}
		for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			for (np = ImemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
				for (j = 0; j < MAX_MEM_BLOCK; j++) {
					if ((np->mem)->numReferenced [j] > 0) {
						ImemCacheWorkingSetSize++;
					}
				}
			}
		}
		for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			for (np = ImemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
				for (j = 0; j < MAX_MEM_BLOCK; j++) {
					if ((np->mem)->numReferenced [j] > 0) {
						ImemPageWorkingSetSize++;
					}
				}
			}
		}
		fprintf(out_file_memfootprint,"%lld: %lld : %lld : %lld\n",DmemCacheWorkingSetSize,DmemPageWorkingSetSize,ImemCacheWorkingSetSize,ImemPageWorkingSetSize); 
		/* RESET */	

		/* clean used memory, to avoid memory shortage for long (CPU2006) benchmarks */ 	
		nlist* np_rm;
		for(i=0; i < MAX_MEM_TABLE_ENTRIES; i++){
			np = memAddressesTable[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm->mem);
				free(np_rm);
			}
			np = DmemCacheWorkingSetTable[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm->mem);
				free(np_rm);
			}
			np = DmemPageWorkingSetTable[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm->mem);
				free(np_rm);
			}
			np = ImemCacheWorkingSetTable[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm->mem);
				free(np_rm);
			}
			np = ImemPageWorkingSetTable[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm->mem);
				free(np_rm);
			}
		}
		for (i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
			memAddressesTable[i] = (nlist*) NULL;
			DmemCacheWorkingSetTable[i] = (nlist*) NULL;
			DmemPageWorkingSetTable[i] = (nlist*) NULL;
			ImemCacheWorkingSetTable[i] = (nlist*) NULL;
			ImemPageWorkingSetTable[i] = (nlist*) NULL;
		}
	}

	/* I-stream (32-byte) cache block memory footprint */

	addr = instrAddr >> 5;
	upperAddr = addr >> LOG_MAX_MEM_BLOCK;
	indexInChunk = addr ^ (upperAddr << LOG_MAX_MEM_BLOCK);

	chunk = lookup(ImemCacheWorkingSetTable, upperAddr);
	if(chunk == (memNode*)NULL)
		chunk = install(ImemCacheWorkingSetTable, upperAddr);

	chunk->numReferenced[indexInChunk] = 1;

	/* I-stream (4KB) page block memory footprint */

	addr = instrAddr >> 12;
	upperAddr = addr >> LOG_MAX_MEM_BLOCK;
	indexInChunk = addr ^ (upperAddr << LOG_MAX_MEM_BLOCK);

	chunk = lookup(ImemPageWorkingSetTable, upperAddr);
	if(chunk == (memNode*)NULL)
		chunk = install(ImemPageWorkingSetTable, upperAddr);

	chunk->numReferenced[indexInChunk] = 1;
}

/* This function is called before every instruction is executed */
VOID instr(ADDRINT instrAddr, UINT32 regOpCnt) {
	
	int i,j;
	nlist* np;

	/* tmp output ? */
	if((mode==MEMFOOTPRINT || mode==ILP_MEMFOOTPRINT) && total_count%TMP_OUTPUT_SIZE == 0){
		long long DmemCacheWorkingSetSize = 0L;
		long long DmemPageWorkingSetSize = 0L;
		long long ImemCacheWorkingSetSize = 0L;
		long long ImemPageWorkingSetSize = 0L;

		for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			for (np = DmemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
				for (j = 0; j < MAX_MEM_BLOCK; j++) {
					if ((np->mem)->numReferenced [j] > 0) {
						DmemCacheWorkingSetSize++;
					}
				}
			}
		}
		for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			for (np = DmemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
				for (j = 0; j < MAX_MEM_BLOCK; j++) {
					if ((np->mem)->numReferenced [j] > 0) {
						DmemPageWorkingSetSize++;
					}
				}
			}
		}
		for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			for (np = ImemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
				for (j = 0; j < MAX_MEM_BLOCK; j++) {
					if ((np->mem)->numReferenced [j] > 0) {
						ImemCacheWorkingSetSize++;
					}
				}
			}
		}
		for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			for (np = ImemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
				for (j = 0; j < MAX_MEM_BLOCK; j++) {
					if ((np->mem)->numReferenced [j] > 0) {
						ImemPageWorkingSetSize++;
					}
				}
			}
		}
		fprintf(stderr,"memfootprint (TMP) @ %lld -> %lld: %lld : %lld : %lld\n",(long long)total_count,(long long)DmemCacheWorkingSetSize,(long long)DmemPageWorkingSetSize,(long long)ImemCacheWorkingSetSize,(long long)ImemPageWorkingSetSize); 
	}
	if(mode==PPM && total_count%TMP_OUTPUT_SIZE == 0){
		fprintf(stderr,"ppm (TMP) @ %lld -> %lld",(long long)total_count,(long long)total_count);
		for(j=0; j < NUM_HIST_LENGTHS; j++)
			fprintf(stderr," %lld %lld %lld %lld",(long long)GAg_incorrect_pred[j],(long long)PAg_incorrect_pred[j],(long long)GAs_incorrect_pred[j],(long long)PAs_incorrect_pred[j]);
		INT64 total_transition_count = 0;
		INT64 total_taken_count = 0;
		INT64 total_brCount = 0;
		for(j=0; j < brHist_size; j++){
			if(local_brCounts[j] > 0){
				if( transition_counts[j] > local_brCounts[j]/2)
					total_transition_count += local_brCounts[j]-transition_counts[j];
				else
					total_transition_count += transition_counts[j];

				if( local_taken_counts[j] > local_brCounts[j]/2)
					total_taken_count += local_brCounts[j] - local_taken_counts[j];
				else
					total_taken_count += local_taken_counts[j];
				total_brCount += local_brCounts[j];
			}
		}
		fprintf(stderr," %lld %lld %lld\n",(long long)total_brCount,(long long)total_transition_count,(long long)total_taken_count); 
	}
	
	/* *** output? *** */
	if( !full_run && total_count > 0 && total_count%interval_size == 0){

		if(mode==ALL || mode==ILP || mode==ILP_MEMFOOTPRINT){
			/* *** ILP *** */
			fprintf(out_file_ilp,"%lld",(long long)total_count_interval);
			for(j=0; j < NUM_WINDOW_SIZES; j++)
				fprintf(out_file_ilp," %lld",(long long)cpuClock_interval[j]);
			fprintf(out_file_ilp,"\n");
		}
		if(mode==ALL || mode==ITYPES){
			/* *** ITYPES *** */
			fprintf(out_file_itypes,"%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",(long long)total_count_interval, (long long)mem_read_cnt, (long long)mem_write_cnt, (long long)control_cnt, (long long)arith_cnt, (long long)fp_cnt, (long long)stack_cnt, (long long)shift_cnt, (long long)string_cnt, (long long)sse_cnt, (long long)other_cnt);
		}
		if(mode==ALL || mode==PPM){
			/* *** PPM *** */
			fprintf(out_file_ppm,"%lld",(long long)total_count_interval);
			for(j=0; j < NUM_HIST_LENGTHS; j++)
				fprintf(out_file_ppm," %lld %lld %lld %lld",(long long)GAg_incorrect_pred[j],(long long)PAg_incorrect_pred[j],(long long)GAs_incorrect_pred[j],(long long)PAs_incorrect_pred[j]);
			INT64 total_transition_count = 0;
			INT64 total_taken_count = 0;
			INT64 total_brCount = 0;
			for(j=0; j < brHist_size; j++){
				if(local_brCounts[j] > 0){
					if( transition_counts[j] > local_brCounts[j]/2)
						total_transition_count += local_brCounts[j]-transition_counts[j];
					else
						total_transition_count += transition_counts[j];

					if( local_taken_counts[j] > local_brCounts[j]/2)
						total_taken_count += local_brCounts[j] - local_taken_counts[j];
					else
						total_taken_count += local_taken_counts[j];
					total_brCount += local_brCounts[j];
				}
			}
			fprintf(out_file_ppm," %lld %lld %lld\n",(long long)total_brCount,(long long)total_transition_count,(long long)total_taken_count); 
		}
		if(mode==ALL || mode==REG_MODE){
			/* *** REG *** */ 
			UINT64 totNumOps = 0;
			UINT64 num;
			double avg;

			/* total number of operands */
			for(i = 1; i < MAX_NUM_OPER; i++){
				totNumOps += opCounts[i]*i;
			}
			fprintf(out_file_reg,"%lld %lld",(long long)total_count_interval,(long long)totNumOps); 

			/* average degree of use */
			num = 0;
			avg = 0.0;
			for(i = 0; i < MAX_REG_USE; i++){
				num += regUseDistr[i];
			}
			fprintf(out_file_reg," %lld",(long long)num);
			num = 0;
			for(i = 0; i < MAX_REG_USE; i++){
				num += i * regUseDistr[i];
			}
			fprintf(out_file_reg," %lld",(long long)num); 

			/* register dependency distributions */
			num = 0;
			avg = 0.0;
			for(i = 0; i < MAX_COMM_DIST; i++){
				num += regAgeDistr[i];
			}
			fprintf(out_file_reg," %lld",(long long)num);
			num = 0;
			for(i = 0; i < MAX_COMM_DIST; i++){
				num += regAgeDistr[i];
				if( (i == 1) || (i == 2) || (i == 4) || (i == 8) || (i == 16) || (i == 32) || (i == 64)){
					fprintf(out_file_reg," %lld",(long long)num);
				}
			}
			fprintf(out_file_reg,"\n");
		}
		if(mode==ALL || mode==STRIDE){
			/* *** STRIDE *** */
			UINT64 cum;

			fprintf(out_file_stride,"%lld",(long long)numReadInstrsAnalyzed);
			/* local read distribution */
			cum = 0;
			for(i = 0; i < MAX_DISTR; i++){
				cum += localReadDistrib[i];
				if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) || (i == 262144) ){
					if(cum > 0)
						fprintf(out_file_stride," %lld", (long long) cum);
					else
						fprintf(out_file_stride," %d", 0);
				}
				if(i == 262144)
					break;
			}
			/* global read distribution */
			cum = 0;
			for(i = 0; i < MAX_DISTR; i++){
				cum += globalReadDistrib[i];
				if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) || (i == 262144) ){
					if(cum > 0)
						fprintf(out_file_stride," %lld", (long long) cum);
					else	
						fprintf(out_file_stride," %d", 0);
				}
				if(i == 262144)
					break;
			}
			fprintf(out_file_stride," %lld",(long long)numWriteInstrsAnalyzed);
			/* local write distribution */
			cum = 0;
			for(i = 0; i < MAX_DISTR; i++){
				cum += localWriteDistrib[i];
				if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) || (i == 262144) ){
					if(cum > 0)
						fprintf(out_file_stride," %lld", (long long) cum);
					else	
						fprintf(out_file_stride," %d", 0);
				}
				if(i == 262144)
					break;
			}
			/* global write distribution */
			cum = 0;
			for(i = 0; i < MAX_DISTR; i++){
				cum += globalWriteDistrib[i];
				if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) ){
					if(cum > 0)
						fprintf(out_file_stride," %lld", (long long) cum);
					else	
						fprintf(out_file_stride," %d", 0);
				}
				if(i == 262144){
					if(cum > 0)
						fprintf(out_file_stride," %lld\n", (long long) cum);
					else	
						fprintf(out_file_stride," %d\n", 0);
					break;
				}
			}
		}
		if(mode==ALL || mode==MEMFOOTPRINT || mode==ILP_MEMFOOTPRINT){
			/* *** MEMFOOTPRINT *** */

			long long DmemCacheWorkingSetSize = 0L;
			long long DmemPageWorkingSetSize = 0L;
			long long ImemCacheWorkingSetSize = 0L;
			long long ImemPageWorkingSetSize = 0L;

			for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
				for (np = DmemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
					for (j = 0; j < MAX_MEM_BLOCK; j++) {
						if ((np->mem)->numReferenced [j] > 0) {
							DmemCacheWorkingSetSize++;
						}
					}
				}
			}
			for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
				for (np = DmemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
					for (j = 0; j < MAX_MEM_BLOCK; j++) {
						if ((np->mem)->numReferenced [j] > 0) {
							DmemPageWorkingSetSize++;
						}
					}
				}
			}
			for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
				for (np = ImemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
					for (j = 0; j < MAX_MEM_BLOCK; j++) {
						if ((np->mem)->numReferenced [j] > 0) {
							ImemCacheWorkingSetSize++;
						}
					}
				}
			}
			for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
				for (np = ImemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
					for (j = 0; j < MAX_MEM_BLOCK; j++) {
						if ((np->mem)->numReferenced [j] > 0) {
							ImemPageWorkingSetSize++;
						}
					}
				}
			}
			fprintf(out_file_memfootprint,"%lld: %lld : %lld : %lld\n",DmemCacheWorkingSetSize,DmemPageWorkingSetSize,ImemCacheWorkingSetSize,ImemPageWorkingSetSize); 
		}

		/* *** RESET *** */
		total_count_interval = 0;

		if(mode==ALL || mode==ILP || mode==ILP_MEMFOOTPRINT){
			/* ILP */
			for(j=0; j < NUM_WINDOW_SIZES; j++)
				cpuClock_interval[j] = 0;
			size_pow_times = 10;
			index_all_times = 1;
    			for(i=0; i < NUM_WINDOW_SIZES; i++){
	    			free(all_times[i]);
	    			if((all_times[i] = (UINT64*)malloc((1 << size_pow_times) * sizeof(UINT64))) == (UINT64*)NULL){
					fprintf(stderr,"Could not allocate memory\n");
					exit(1);
	    			}
			}
		}
		if(mode==ALL || mode==ITYPES){
			/* ITYPES */
			mem_read_cnt = 0;
			mem_write_cnt = 0;
			control_cnt = 0;
			arith_cnt = 0;
			fp_cnt = 0;
			stack_cnt = 0;
			shift_cnt = 0;
			string_cnt = 0;
			sse_cnt = 0;
			other_cnt = 0;
		}
		if(mode==ALL || mode==PPM){
			/* PPM */
			for(j = 0; j < NUM_HIST_LENGTHS; j++){
				GAg_incorrect_pred[j] = 0;
				GAs_incorrect_pred[j] = 0;
				PAg_incorrect_pred[j] = 0;
				PAs_incorrect_pred[j] = 0;
			}
			for(i=0; i < brHist_size; i++){
				local_brCounts[i] = 0;
				local_taken_counts[i] = 0;
				transition_counts[i] = 0;
				// local_taken[i] = -1;
			}
		}
		if(mode==ALL || mode==REG_MODE){
			/* REG */
			for(i = 0; i < MAX_NUM_OPER; i++){
				opCounts[i] = 0;
			}
			/* do NOT reset register use counts or register definition addresses
			 * that should only be done when the register is written to */
			/* for(i = 0; i < MAX_NUM_REGS; i++){
			   regRef[i] = false;
			   PCTable[i] = 0;
			   regUseCnt[i] = 0;
			   } */
			for(i = 0; i < MAX_REG_USE; i++){
				regUseDistr[i] = 0;
			}
			for(i = 0; i < MAX_COMM_DIST; i++){
				regAgeDistr[i] = 0;
			}
		}
		if(mode==ALL || mode==STRIDE){
			/* STRIDE */

			/* do NOT reset last read/write memory addresses per instruction */
			/*
			   for (i = 0; i < numRead; i++)
			   instrRead [i] = 0;
			   for(i = 0; i < numWrite; i++)
			   instrWrite [i] = 0;
			 */
			/* do NOT reset last read/write memory addresses */
			/*
			   lastReadAddr = 0;
			   lastWriteAddr = 0;
			 */
			for (i = 0; i < MAX_DISTR; i++) {
				localReadDistrib [i] = 0;
				localWriteDistrib [i] = 0;
				globalReadDistrib [i] = 0;
				globalWriteDistrib [i] = 0;
			}
			numInstrsAnalyzed = 0;
			numReadInstrsAnalyzed = 0;
			numWriteInstrsAnalyzed = 0;
		}

		if(mode==ALL || mode==MEMFOOTPRINT || mode==ILP_MEMFOOTPRINT || mode==ILP){
			/* MEMFOOTPRINT */	

			/* clean used memory, to avoid memory shortage for long (CPU2006) benchmarks */ 	
			nlist* np_rm;
			for(i=0; i < MAX_MEM_TABLE_ENTRIES; i++){
				np = memAddressesTable[i];
				while(np != (nlist*)NULL){
					np_rm = np;
					np = np->next;
					free(np_rm->mem);
					free(np_rm);
				}
				np = DmemCacheWorkingSetTable[i];
				while(np != (nlist*)NULL){
					np_rm = np;
					np = np->next;
					free(np_rm->mem);
					free(np_rm);
				}
				np = DmemPageWorkingSetTable[i];
				while(np != (nlist*)NULL){
					np_rm = np;
					np = np->next;
					free(np_rm->mem);
					free(np_rm);
				}
				np = ImemCacheWorkingSetTable[i];
				while(np != (nlist*)NULL){
					np_rm = np;
					np = np->next;
					free(np_rm->mem);
					free(np_rm);
				}
				np = ImemPageWorkingSetTable[i];
				while(np != (nlist*)NULL){
					np_rm = np;
					np = np->next;
					free(np_rm->mem);
					free(np_rm);
				}
			}
			for (i = 0; i < MAX_MEM_TABLE_ENTRIES; i++) {
				memAddressesTable[i] = (nlist*) NULL;
				DmemCacheWorkingSetTable[i] = (nlist*) NULL;
				DmemPageWorkingSetTable[i] = (nlist*) NULL;
				ImemCacheWorkingSetTable[i] = (nlist*) NULL;
				ImemPageWorkingSetTable[i] = (nlist*) NULL;
			}

			/* only reset counters (faster, but uses a lot more memory space) */ 

			/*for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			  for (np = DmemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
			  for (j = 0; j < MAX_MEM_BLOCK; j++) {
			  (np->mem)->numReferenced[j] = 0;
			  }
			  }
			  }
			  for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			  for (np = DmemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
			  for (j = 0; j < MAX_MEM_BLOCK; j++) {
			  (np->mem)->numReferenced[j] = 0;
			  }
			  }
			  }
			  for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			  for (np = ImemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
			  for (j = 0; j < MAX_MEM_BLOCK; j++) {
			  (np->mem)->numReferenced [j] = 0;
			  }
			  }
			  }
			  for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
			  for (np = ImemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
			  for (j = 0; j < MAX_MEM_BLOCK; j++) {
			  (np->mem)->numReferenced[j] = 0;
			  }
			  }
			  }*/
		}
		
	}

	/* GLOBAL */

	if(total_count%1000000000 == 0){
		fprintf(instr_count_output,"%lld * 10^9 instrs done\n",(long long)(total_count/1000000000));	
		fflush(instr_count_output);
	}

	total_count++;
	total_count_interval++;
	
	/* *** ILP *** */
	
	if(mode==ALL || mode==ILP || mode==ILP_MEMFOOTPRINT){
		UINT32 reordered = 0;

		/* minimum issue time: CPU clock */
		for(j=0; j < NUM_WINDOW_SIZES; j++){
			if(cpuClock[j] > issueTime[j])
				issueTime[j] = cpuClock[j];
		}

		/* add this instruction to tail of instruction window:
		 * - set issue time for this instruction in executionProfile
		 * - adjust window tail index (cyclic)
		 */
		for(j=0; j < NUM_WINDOW_SIZES; j++){
			executionProfile[j][windowTail[j]] = issueTime[j];
			windowTail[j] = (windowTail[j] + 1) % win_size[j];
		}

		for(j=0; j < NUM_WINDOW_SIZES; j++){
			/* if instruction window (issue buffer) full */
			if(windowHead[j] == windowTail[j]){
				cpuClock[j]++;
				cpuClock_interval[j]++;
				reordered = 0;
				/* remove all instructions which are ready to be issued from beginning of window, 
				 * until an instruction comes along which is not ready for issueing yet:
				 * -> check executionProfile to see which instructions are ready
				 * -> remove maximum win_size instructions (i.e. stop when issue buffer is empty)
				 */
				while((executionProfile[j][windowHead[j]] < cpuClock[j]) && (reordered < win_size[j])) {
					windowHead[j] = (windowHead[j] + 1) % win_size[j];
					reordered++;
				}
				if(reordered == 0){
					fprintf(stderr,"reordered == 0\n");
					exit(1);
				}
			}
		}

		/* reset issue time */
		for(j=0; j < NUM_WINDOW_SIZES; j++)
			issueTime[j] = 0;
	}

	/* *** REG *** */

	if(mode==ALL || mode==REG_MODE){
		opCounts[regOpCnt]++;
	}

	/* *** MEMFOOTPRINT *** */

	if(mode==ALL || mode==MEMFOOTPRINT || mode==ILP_MEMFOOTPRINT){
		ADDRINT addr, upperAddr, indexInChunk;
		memNode* chunk;

		/* I-stream (32-byte) cache block memory footprint */

		addr = instrAddr >> 5;
		upperAddr = addr >> LOG_MAX_MEM_BLOCK;
		indexInChunk = addr ^ (upperAddr << LOG_MAX_MEM_BLOCK);

		chunk = lookup(ImemCacheWorkingSetTable, upperAddr);
		if(chunk == (memNode*)NULL)
			chunk = install(ImemCacheWorkingSetTable, upperAddr);

		chunk->numReferenced[indexInChunk] = 1;

		/* I-stream (4KB) page block memory footprint */

		addr = instrAddr >> 12;
		upperAddr = addr >> LOG_MAX_MEM_BLOCK;
		indexInChunk = addr ^ (upperAddr << LOG_MAX_MEM_BLOCK);

		chunk = lookup(ImemPageWorkingSetTable, upperAddr);
		if(chunk == (memNode*)NULL)
			chunk = install(ImemPageWorkingSetTable, upperAddr);

		chunk->numReferenced[indexInChunk] = 1;
	}
}

/* read register operand */
VOID readRegOp_ilp(UINT32 regId){
	int j;

	/* *** ILP *** */
	for(j=0; j < NUM_WINDOW_SIZES; j++){

		if(timeAvailable[j][regId] > issueTime[j])
			issueTime[j] = timeAvailable[j][regId];
		if(cpuClock[j] > issueTime[j])
			issueTime[j] = cpuClock[j];
	}
}

/* read register operand */
VOID readRegOp_reg(UINT32 regId){

	/* *** REG *** */

	/* register age */
	INT64 age = total_count - PCTable[regId]; // dependency distance
	if(age >= MAX_COMM_DIST){
		age = MAX_COMM_DIST - 1; // trim if needed
	}
	regAgeDistr[age]++;

	/* register usage */
	regUseCnt[regId]++;
	regRef[regId] = 1; // (operand) register was referenced 
}

/* read register operand */
VOID readRegOp(UINT32 regId){
	int j;

		// *** ILP *** 
		for(j=0; j < NUM_WINDOW_SIZES; j++){

			if(timeAvailable[j][regId] > issueTime[j])
				issueTime[j] = timeAvailable[j][regId];
			if(cpuClock[j] > issueTime[j])
				issueTime[j] = cpuClock[j];
		}
		// *** REG *** 

		// register age 
		INT64 age = total_count - PCTable[regId]; // dependency distance
		if(age >= MAX_COMM_DIST){
			age = MAX_COMM_DIST - 1; // trim if needed
		}
		regAgeDistr[age]++;

		// register usage 
		regUseCnt[regId]++;
		regRef[regId] = 1; // (operand) register was referenced 
}

/* write register operand */
VOID writeRegOp_ilp(UINT32 regId){
	int j;

	/* *** ILP *** */
	for(j=0; j < NUM_WINDOW_SIZES; j++)
		timeAvailable[j][regId] = issueTime[j] + 1;
}

VOID writeRegOp_reg(UINT32 regId){

	/* *** REG *** */
	UINT32 num;

	/* if register was referenced before, adjust use distribution */
	if(regRef[regId]){
		num = regUseCnt[regId];
		if(num >= MAX_REG_USE) // trim if needed
			num = MAX_REG_USE - 1;
		regUseDistr[num]++;
	}

	/* reset register stuff because of new value produced */

	PCTable[regId] = total_count; // last production = now
	regUseCnt[regId] = 0; // new value is never used (yet)
	regRef[regId] = true; // (destination) register was referenced (for tracking use distribution)
}

/* write register operand */
VOID writeRegOp(UINT32 regId){
	int j;
	UINT32 num;

		// *** ILP *** 
		for(j=0; j < NUM_WINDOW_SIZES; j++)
			timeAvailable[j][regId] = issueTime[j] + 1;
		// *** REG *** 

		// if register was referenced before, adjust use distribution 
		if(regRef[regId]){
			num = regUseCnt[regId];
			if(num >= MAX_REG_USE) // trim if needed
				num = MAX_REG_USE - 1;
			regUseDistr[num]++;
		}

		// reset register stuff because of new value produced 

		PCTable[regId] = total_count; // last production = now
		regUseCnt[regId] = 0; // new value is never used (yet)
		regRef[regId] = true; // (destination) register was referenced (for tracking use distribution)
}

/* memory read */
VOID readMem_ilp(UINT32 index, ADDRINT effAddr){

	int j;

	/* *** ILP *** */
	ADDRINT upperMemAddr, indexInChunk;
	memNode* chunk = (memNode*)NULL;
	ADDRINT shiftedAddr = effAddr >> 5;

	upperMemAddr = shiftedAddr >> LOG_MAX_MEM_ENTRIES;
	indexInChunk = shiftedAddr ^ (upperMemAddr << LOG_MAX_MEM_ENTRIES);

	chunk = lookup(memAddressesTable,upperMemAddr);
	if(chunk == (memNode*)NULL)
		chunk = install(memAddressesTable,upperMemAddr);

	for(j=0; j < NUM_WINDOW_SIZES; j++){

		if(all_times[j][chunk->timeAvailable[indexInChunk]] > issueTime[j])
			issueTime[j] = all_times[j][chunk->timeAvailable[indexInChunk]];

		if(cpuClock[j] > issueTime[j])
			issueTime[j] = cpuClock[j];
	}
}

VOID readMem_stride(UINT32 index, ADDRINT effAddr){

	/* *** STRIDE *** */

	ADDRINT stride;

	numReadInstrsAnalyzed++;

	/* local stride	*/
	/* avoid negative values, has to be doen like this (not stride < 0 => stride = -stride (avoid problems with unsigned values)) */
	if(effAddr > instrRead[readIndex])
		stride = effAddr - instrRead[index];
	else
		stride = instrRead[index]-effAddr;
	if(stride >= MAX_DISTR){
		stride = MAX_DISTR-1; // trim if needed
	}

	localReadDistrib[stride]++;
	instrRead[index] = effAddr;

	/* global stride */
	/* avoid negative values, has to be done like this (not stride < 0 => stride = -stride (avoid problems with unsigned values)) */
	if(effAddr > lastReadAddr)
		stride = effAddr - lastReadAddr;
	else
		stride = lastReadAddr - effAddr;
	if(stride >= MAX_DISTR){
		stride = MAX_DISTR-1; // trim if needed
	}

	globalReadDistrib[stride]++;
	lastReadAddr = effAddr;
}

/* memory read */
VOID readMem(UINT32 index, ADDRINT effAddr){
	int j;

	// *** ILP *** 
	ADDRINT upperMemAddr, indexInChunk;
	memNode* chunk = (memNode*)NULL;
	ADDRINT shiftedAddr = effAddr >> 5;

	upperMemAddr = shiftedAddr >> LOG_MAX_MEM_ENTRIES;
	indexInChunk = shiftedAddr ^ (upperMemAddr << LOG_MAX_MEM_ENTRIES);

	chunk = lookup(memAddressesTable,upperMemAddr);
	if(chunk == (memNode*)NULL)
		chunk = install(memAddressesTable,upperMemAddr);

	for(j=0; j < NUM_WINDOW_SIZES; j++){

		if(all_times[j][chunk->timeAvailable[indexInChunk]] > issueTime[j])
			issueTime[j] = all_times[j][chunk->timeAvailable[indexInChunk]];

		if(cpuClock[j] > issueTime[j])
			issueTime[j] = cpuClock[j];
	}

	// *** STRIDE ***

	ADDRINT stride;

	numReadInstrsAnalyzed++;

	// local stride	
	// avoid negative values, has to be doen like this (not stride < 0 => stride = -stride (avoid problems with unsigned values)) 
	if(effAddr > instrRead[readIndex])
		stride = effAddr - instrRead[index];
	else
		stride = instrRead[index]-effAddr;
	if(stride >= MAX_DISTR){
		stride = MAX_DISTR-1; // trim if needed
	}

	localReadDistrib[stride]++;
	instrRead[index] = effAddr;

	// global stride 
	// avoid negative values, has to be done like this (not stride < 0 => stride = -stride (avoid problems with unsigned values)) 
	if(effAddr > lastReadAddr)
		stride = effAddr - lastReadAddr;
	else
		stride = lastReadAddr - effAddr;
	if(stride >= MAX_DISTR){
		stride = MAX_DISTR-1; // trim if needed
	}

	globalReadDistrib[stride]++;
	lastReadAddr = effAddr;
}

/* memory written */
VOID writeMem_ilp(UINT32 index, ADDRINT effAddr){
	int j;

	/* *** ILP *** */
	ADDRINT upperMemAddr, indexInChunk;
	memNode* chunk = (memNode*)NULL;
	ADDRINT shiftedAddr = effAddr >> 5;

	upperMemAddr = shiftedAddr >> LOG_MAX_MEM_ENTRIES;
	indexInChunk = shiftedAddr ^ (upperMemAddr << LOG_MAX_MEM_ENTRIES);

	chunk = lookup(memAddressesTable,upperMemAddr);
	if(chunk == (memNode*)NULL)
		chunk = install(memAddressesTable,upperMemAddr);

	if(chunk->timeAvailable[indexInChunk] == 0){
		index_all_times++;
		if(index_all_times >= (1 << size_pow_times))
			increase_size_all_times();
		chunk->timeAvailable[indexInChunk] = index_all_times;
	}
	for(j=0; j < NUM_WINDOW_SIZES; j++)
		all_times[j][chunk->timeAvailable[indexInChunk]] = issueTime[j] + 1;
}

VOID writeMem_stride(UINT32 index, ADDRINT effAddr){

	/* *** STRIDE *** */

	ADDRINT stride;

	numWriteInstrsAnalyzed++;

	/* local stride */
	/* avoid negative values, has to be doen like this (not stride < 0 => stride = -stride) */
	if(effAddr > instrWrite[writeIndex])
		stride = effAddr - instrWrite[index];
	else
		stride = instrWrite[index] - effAddr;
	if(stride >= MAX_DISTR){
		stride = MAX_DISTR-1; // trim if needed
	}

	localWriteDistrib[stride]++;
	instrWrite[index] = effAddr;

	/* global stride */
	/* avoid negative values, has to be doen like this (not stride < 0 => stride = -stride) */
	if(effAddr > lastWriteAddr)
		stride = effAddr - lastWriteAddr;
	else
		stride = lastWriteAddr - effAddr;
	if(stride >= MAX_DISTR){
		stride = MAX_DISTR-1; // trim if needed
	}

	globalWriteDistrib[stride]++;
	lastWriteAddr = effAddr;
}

/* memory written */
VOID writeMem(UINT32 index, ADDRINT effAddr){
	int j;

	// *** ILP *** 
	ADDRINT upperMemAddr, indexInChunk;
	memNode* chunk = (memNode*)NULL;
	ADDRINT shiftedAddr = effAddr >> 5;

	upperMemAddr = shiftedAddr >> LOG_MAX_MEM_ENTRIES;
	indexInChunk = shiftedAddr ^ (upperMemAddr << LOG_MAX_MEM_ENTRIES);

	chunk = lookup(memAddressesTable,upperMemAddr);
	if(chunk == (memNode*)NULL)
		chunk = install(memAddressesTable,upperMemAddr);

	if(chunk->timeAvailable[indexInChunk] == 0){
		index_all_times++;
		if(index_all_times >= (1 << size_pow_times))
			increase_size_all_times();
		chunk->timeAvailable[indexInChunk] = index_all_times;
	}
	for(j=0; j < NUM_WINDOW_SIZES; j++)
		all_times[j][chunk->timeAvailable[indexInChunk]] = issueTime[j] + 1;
	// *** STRIDE *** 

	ADDRINT stride;

	numWriteInstrsAnalyzed++;

	// local stride 
	// avoid negative values, has to be doen like this (not stride < 0 => stride = -stride) 
	if(effAddr > instrWrite[writeIndex])
		stride = effAddr - instrWrite[index];
	else
		stride = instrWrite[index] - effAddr;
	if(stride >= MAX_DISTR){
		stride = MAX_DISTR-1; // trim if needed
	}

	localWriteDistrib[stride]++;
	instrWrite[index] = effAddr;

	// global stride 
	// avoid negative values, has to be doen like this (not stride < 0 => stride = -stride) 
	if(effAddr > lastWriteAddr)
		stride = effAddr - lastWriteAddr;
	else
		stride = lastWriteAddr - effAddr;
	if(stride >= MAX_DISTR){
		stride = MAX_DISTR-1; // trim if needed
	}

	globalWriteDistrib[stride]++;
	lastWriteAddr = effAddr;
}

/* ==========================================
 *             MEMFOOTPRINT (functions)
 * ========================================== */

/* called for every instruction operand which accesses memory (D-stream memfootprint) */
VOID memOp(ADDRINT effMemAddr){
	ADDRINT addr, upperAddr, indexInChunk;
	memNode* chunk;

	/* D-stream (32-byte) cache block memory footprint */

	addr = effMemAddr >> 5;
	upperAddr = addr >> LOG_MAX_MEM_BLOCK;
	indexInChunk = addr ^ (upperAddr << LOG_MAX_MEM_BLOCK);

	chunk = lookup(DmemCacheWorkingSetTable, upperAddr);
	if(chunk == (memNode*)NULL)
		chunk = install(DmemCacheWorkingSetTable, upperAddr);

	chunk->numReferenced[indexInChunk] = 1;

	/* D-stream (4KB) page block memory footprint */

	addr = effMemAddr >> 12;
	upperAddr = addr >> LOG_MAX_MEM_BLOCK;
	indexInChunk = addr ^ (upperAddr << LOG_MAX_MEM_BLOCK);

	chunk = lookup(DmemPageWorkingSetTable, upperAddr);
	if(chunk == (memNode*)NULL)
		chunk = install(DmemPageWorkingSetTable, upperAddr);

	chunk->numReferenced[indexInChunk] = 1;
	//fprintf(stderr,"done\n");
}

/* ==========================================
 *                     PIN 
 * ========================================== */

/* Pin calls this function every time a new instruction is encountered */
VOID Instruction(INS ins, VOID *v)
{
	UINT32 index;
	UINT32 i, maxNumRegsProd, maxNumRegsCons, opCnt, regOpCnt;

	char cat[50];
	char opcode[50];
	strcpy(cat,CATEGORY_StringShort(INS_Category(ins)).c_str());
	strcpy(opcode,INS_Mnemonic(ins).c_str());
	bool categorized = false;

	/* *** REGISTER READS *** */

	if(mode==ALL || mode==REG_MODE || mode==ILP || mode==ILP_MEMFOOTPRINT){
		// for each register operand read operation of this instruction, insert a call 
		// !!! a single instruction can read AND write a single register !!!
		maxNumRegsCons = INS_MaxNumRRegs(ins); // maximum number of register consumations (reads)

		for(i = 0; i < maxNumRegsCons; i++){ // finding all register operands which are read
			const REG reg = INS_RegR(ins,i);
			// only counting valid general purpose or floating point registers
			if(REG_valid(reg) && (REG_is_gr(reg) || REG_is_fr(reg))){
				switch(mode){
					case ILP:
					case ILP_MEMFOOTPRINT:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readRegOp_ilp,IARG_UINT32,reg,IARG_END);
						break;
					case REG_MODE:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readRegOp_reg,IARG_UINT32,reg,IARG_END);
						break;
					case ALL:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readRegOp,IARG_UINT32,reg,IARG_END);
						break;
				}
			}
			else
				break;
		}
	}

	/* *** MEMORY READS *** */

	if( INS_IsMemoryRead(ins) ){ // instruction has memory read operand

		/* ilp & stride */
		if(mode==ALL || mode==ILP || mode==STRIDE || mode==ILP_MEMFOOTPRINT){

			index = index_memRead(1, INS_Address(ins));
			if(index < 1){

				/* check if more memory space is needed */
				if(readIndex >= numRead){
					reallocate_readArray();
				}

				switch(mode){
					case ILP:
					case ILP_MEMFOOTPRINT:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem_ilp,IARG_UINT32,readIndex,IARG_MEMORYREAD_EA,IARG_END);
						break;
					case STRIDE:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem_stride,IARG_UINT32,readIndex,IARG_MEMORYREAD_EA,IARG_END);
						break;
					case ALL:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem,IARG_UINT32,readIndex,IARG_MEMORYREAD_EA,IARG_END);
						break;
				}

				//fprintf(stderr,"registered MEM_READ %x to %d\n",INS_Address(ins),readIndex);
				register_memRead(INS_Address(ins));
			}
			else{
				//fprintf(stderr,"Hey, I have seen this MEM_READ instruction (hex: %x) before! (index found: %ld)\n",INS_Address(ins),index);
				switch(mode){
					case ILP:
					case ILP_MEMFOOTPRINT: 
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem_ilp,IARG_UINT32,index,IARG_MEMORYREAD_EA,IARG_END);
						break;
					case STRIDE:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem_stride,IARG_UINT32,index,IARG_MEMORYREAD_EA,IARG_END);
						break;
					case ALL:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem,IARG_UINT32,index,IARG_MEMORYREAD_EA,IARG_END);
						break;
				}
			}

		}
		/* instruction mix */
		if(mode==ALL || mode==ITYPES){
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)count_mem_read,IARG_END);
		}
		/* memfootprint */	
		if(mode==ALL || mode==MEMFOOTPRINT || mode==ILP_MEMFOOTPRINT){
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)memOp,IARG_MEMORYREAD_EA,IARG_END);
		}

		if( INS_HasMemoryRead2(ins) ){ // second memory read operand

			/* ilp & stride */
			if(mode==ALL || mode==ILP || mode==STRIDE || mode==ILP_MEMFOOTPRINT){

				index = index_memRead(2, INS_Address(ins));
				if(index < 1){

					/* check if more memory space is needed */
					if(readIndex >= numRead){
						reallocate_readArray();
					}

					switch(mode){
						case ILP:
						case ILP_MEMFOOTPRINT: 
							INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem_ilp,IARG_UINT32,readIndex,IARG_MEMORYREAD2_EA,IARG_END);
							break;
						case STRIDE:
							INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem_stride,IARG_UINT32,readIndex,IARG_MEMORYREAD2_EA,IARG_END);
							break;
						case ALL:
							INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem,IARG_UINT32,readIndex,IARG_MEMORYREAD2_EA,IARG_END);
							break;
					}

					//fprintf(stderr,"registered MEM_READ (2) %x to %d\n",INS_Address(ins),readIndex);
					register_memRead(INS_Address(ins));
				}
				else{
					//fprintf(stderr,"Hey, I have seen this MEM_READ (2) instruction (hex: %x) before! (index found: %ld)\n",INS_Address(ins),index);
					switch(mode){
						case ILP:
						case ILP_MEMFOOTPRINT:
							INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem_ilp,IARG_UINT32,index,IARG_MEMORYREAD2_EA,IARG_END);
							break;
						case STRIDE:
							INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem_stride,IARG_UINT32,index,IARG_MEMORYREAD2_EA,IARG_END);
							break;
						case ALL:
							INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem,IARG_UINT32,index,IARG_MEMORYREAD2_EA,IARG_END);
							break;
					}
				}

			}
			/* memfootprint */
			if(mode==ALL || mode==MEMFOOTPRINT || mode==ILP_MEMFOOTPRINT){
				INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)memOp,IARG_MEMORYREAD2_EA,IARG_END);
			}
		}
	} 

	/* *** REGISTER WRITES *** */
	if(mode==ALL || mode==REG_MODE || mode==ILP || mode==ILP_MEMFOOTPRINT){
		// for each register operand write operation of this instruction, insert a call 
		// !!! a single instruction can read AND write a single register !!!
		maxNumRegsProd = INS_MaxNumWRegs(ins); // maximum number of register productions (writes)
		for(i = 0; i < maxNumRegsProd; i++){ // finding all register operands which are written
			const REG reg = INS_RegW(ins,i);
			// only counting valid general purpose or floating point registers
			if(REG_valid(reg) && (REG_is_gr(reg) || REG_is_fr(reg))){ 
				switch(mode){
					case ILP:
					case ILP_MEMFOOTPRINT:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeRegOp_ilp,IARG_UINT32,reg,IARG_END);
						break;
					case REG_MODE: 	
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeRegOp_reg,IARG_UINT32,reg,IARG_END);
						break;
					case ALL:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeRegOp,IARG_UINT32,reg,IARG_END);
						break;
				}
			}
			else
				break;
		}
	}

	/* *** MEMORY WRITES *** */
	if( INS_IsMemoryWrite(ins) ){ // instruction has memory write operand

		/* ilp & stride */
		if(mode==ALL || mode==ILP || mode==STRIDE || mode==ILP_MEMFOOTPRINT){

			index = index_memWrite(INS_Address(ins));	
			if(index < 1){

				/* check if more memory space is needed */
				if(writeIndex >= numWrite)
					reallocate_writeArray();

				switch(mode){
					case ILP:
					case ILP_MEMFOOTPRINT:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeMem_ilp,IARG_UINT32,writeIndex,IARG_MEMORYWRITE_EA,IARG_END);
						break;
					case STRIDE:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeMem_stride,IARG_UINT32,writeIndex,IARG_MEMORYWRITE_EA,IARG_END);
						break;
					case ALL:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeMem,IARG_UINT32,writeIndex,IARG_MEMORYWRITE_EA,IARG_END);
						break;
				}
				
				//fprintf(stderr,"registered MEM_WRITE %x to %d\n",INS_Address(ins),writeIndex);
				register_memWrite(INS_Address(ins));
			}
			else{
				//fprintf(stderr,"Hey, I have seen this MEM_WRITE instruction (hex: %x) before! (index found: %ld)\n",INS_Address(ins),index);
				switch(mode){
					case ILP:
					case ILP_MEMFOOTPRINT:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeMem_ilp,IARG_UINT32,index,IARG_MEMORYWRITE_EA,IARG_END);
						break;
					case STRIDE:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeMem_stride,IARG_UINT32,index,IARG_MEMORYWRITE_EA,IARG_END);
						break;
					case ALL:
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeMem,IARG_UINT32,index,IARG_MEMORYWRITE_EA,IARG_END);
						break;
				}
			}

		}
		/* *** ITYPES *** */
		if(mode==ALL || mode==ITYPES){
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)count_mem_write,IARG_END);
		}
		/* *** MEMFOOTPRINT *** */
		if(mode==ALL || mode==MEMFOOTPRINT || mode==ILP_MEMFOOTPRINT){
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)memOp,IARG_MEMORYWRITE_EA,IARG_END);
		}
	}

	/* *** ITYPES *** */

	if(mode==ALL || mode==ITYPES){
		// *** control flow instructions ***
		if(strcmp(cat,"COND_BR") == 0 || strcmp(cat,"UNCOND_BR") == 0 || strcmp(opcode,"LEAVE") == 0 || strcmp(opcode,"RET_NEAR") == 0 || strcmp(opcode,"CALL_NEAR") == 0){
			categorized = true;
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)count_control,IARG_END);
		}

		// *** arithmetic instructions (integer) ***
		if( strcmp(cat,"LOGICAL") == 0 || strcmp(cat,"DATAXFER") == 0 || strcmp(cat,"BINARY") == 0 || strcmp(cat,"FLAGOP") == 0 || strcmp(cat,"BITBYTE") == 0        ){
			categorized = true;
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)count_arith,IARG_END);
		}    

		// *** floating point instructions ***
		if(strcmp(cat,"X87_ALU") == 0 || strcmp(cat,"FCMOV") == 0){
			categorized = true;
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)count_fp,IARG_END);
		}    

		// *** pop/push instructions (stack usage) ***
		if( (strcmp(opcode,"POP") == 0) || (strcmp(opcode,"PUSH") == 0) || (strcmp(opcode,"POPFD") == 0) || (strcmp(opcode,"PUSHFD") == 0)){
			categorized = true;
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)count_stack,IARG_END);
		}    

		// *** [!] shift instructions (bitwise) ***
		if(strcmp(cat,"SHIFT") == 0){
			categorized = true;
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)count_shift,IARG_END);
		}    

		// *** [!] string instructions ***
		if(strcmp(cat,"STRINGOP") == 0){
			categorized = true;
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)count_string,IARG_END);
		}    

		// *** [!] MMX/SSE instructions ***
		if(strcmp(cat,"MMX") == 0 || strcmp(cat,"SSE") == 0){
			categorized = true;
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)count_sse,IARG_END);
		}    

		// *** other (interrupts, rotate instructions, semaphore, conditional move, system) ***
		if(strcmp(cat,"INTERRUPT") == 0 || strcmp(cat,"ROTATE") == 0 || strcmp(cat,"SEMAPHORE") == 0 || strcmp(cat,"CMOV") == 0 || strcmp(cat,"SYSTEM") == 0 || strcmp(cat,"MISC") == 0 || strcmp(cat,"PREFETCH") == 0 ){
			categorized = true;
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)count_other,IARG_END);
		}   

		if(!categorized){
			cout << "What the hell ?!? I don't know this one yet! (cat: " << cat << ", opcode: " << opcode << ")" << endl;
			exit(1);
		} 
	}

	/* *** PPM *** */

	if(mode==ALL || mode==PPM){
		// Insert a call which updates the PPM history tables if necessary (i.e. if the last (dynamic) instruction was a branch)
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)checkUpdate,IARG_ADDRINT,INS_Address(ins),IARG_END);
		if(strcmp(cat,"COND_BR") == 0){

			/* DEBUG */
			//cout << "  cond. branch found..." << endl;

			index = index_condBr(INS_Address(ins));	
			if(index < 1){

				/* We don't know the number of static conditional branch instructions up front,
				 * so we double the size of the branch history tables as needed by calling this function */
				if(numStatCondBranchInst >= brHist_size)
					reallocate_brHist();
				// determine next prediction for this branch 
				INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)condBr,IARG_ADDRINT,INS_NextAddress(ins),IARG_UINT32,numStatCondBranchInst,IARG_END);

				//fprintf(stderr,"registered COND_BR %x to %d\n",INS_Address(ins),numStatCondBranchInst);
				register_condBr(INS_Address(ins));
			}
			else{
				//fprintf(stderr,"Hey, I have seen this COND_BR instruction (hex: %x) before! (index found: %ld)\n",INS_Address(ins),index);
				INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)condBr,IARG_ADDRINT,INS_NextAddress(ins),IARG_UINT32,index,IARG_END);
			}

		}
	}

	regOpCnt = 0;
	if(mode==REG_MODE){
		opCnt = INS_OperandCount(ins);
		for(i = 0; i < opCnt; i++){
			if(INS_OperandIsReg(ins,i))
				regOpCnt++;
		}
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)instr_reg,IARG_UINT32,regOpCnt,IARG_END);
		if(regOpCnt >= MAX_NUM_OPER){
			fprintf(stderr,"BOOM! -> MAX_NUM_OPER is exceeded! (%u)\n",regOpCnt);
			exit(1);
		}
	}

	/* *** ALL INSTRUCTIONS *** */

	/* includes arguments for REG (operand count) and MEMFOOTPRINT (instruction address) */

	switch(mode){
		case ILP:
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)instr_ilp,IARG_END);
			break;
		case ITYPES:
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)instr_itypes,IARG_END);
			break;
		case STRIDE:
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)instr_stride,IARG_END);
			break;
		case MEMFOOTPRINT:
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)instr_memfootprint,IARG_ADDRINT,INS_Address(ins),IARG_END);
			break;
		case ILP_MEMFOOTPRINT:
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)instr_ilp,IARG_END);
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)instr_memfootprint,IARG_ADDRINT,INS_Address(ins),IARG_END);
			break;
		case PPM:
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)instr_ppm,IARG_END);
			break;
		case ALL:
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)instr,IARG_ADDRINT,INS_Address(ins),IARG_UINT32,regOpCnt,IARG_END);
			break;
	}

	if(mode != ALL)
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)instr_all,IARG_END);
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
	int i,j;

	if( full_run || total_count%interval_size > 0){

		if(mode==ALL || mode==ILP || mode==ILP_MEMFOOTPRINT){
			// *** ILP ***
			fprintf(out_file_ilp,"%lld",(long long)total_count_interval);
			for(j=0; j < NUM_WINDOW_SIZES; j++)
				fprintf(out_file_ilp," %lld",(long long)cpuClock_interval[j]);
			fprintf(out_file_ilp,"\n");
		}
		if(mode==ALL || mode==ITYPES){
			// *** ITYPES ***
			fprintf(out_file_itypes,"%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",(long long)total_count_interval, (long long)mem_read_cnt, (long long)mem_write_cnt, (long long)control_cnt, (long long)arith_cnt, (long long)fp_cnt, (long long)stack_cnt, (long long)shift_cnt, (long long)string_cnt, (long long)sse_cnt, (long long)other_cnt);
		}
		if(mode==ALL || mode==PPM){
			// *** PPM ***	
			fprintf(out_file_ppm,"%lld",(long long)total_count_interval);
			for(j=0; j < NUM_HIST_LENGTHS; j++)
				fprintf(out_file_ppm," %lld %lld %lld %lld",(long long)GAg_incorrect_pred[j],(long long)PAg_incorrect_pred[j],(long long)GAs_incorrect_pred[j],(long long)PAs_incorrect_pred[j]);
			INT64 total_transition_count = 0;
			INT64 total_taken_count = 0;
			INT64 total_brCount = 0;
			for(j=0; j < brHist_size; j++){
				if(local_brCounts[j] > 0){
					if( transition_counts[j] > local_brCounts[j]/2)
						total_transition_count += local_brCounts[j]-transition_counts[j];
					else
						total_transition_count += transition_counts[j];

					if( local_taken_counts[j] > local_brCounts[j]/2)
						total_taken_count += local_brCounts[j] - local_taken_counts[j];
					else
						total_taken_count += local_taken_counts[j];
					total_brCount += local_brCounts[j];
				}
			}
			fprintf(out_file_ppm," %lld %lld %lld\n",(long long)total_brCount,(long long)total_transition_count,(long long)total_taken_count); 
		}
		if(mode==ALL || mode==REG_MODE){
			// *** REG ***
			UINT64 totNumOps = 0;
			UINT64 num;
			double avg;
			/* total number of operands */
			for(i = 1; i < MAX_NUM_OPER; i++){
				totNumOps += opCounts[i]*i;
			}
			fprintf(out_file_reg,"%lld %lld",(long long)total_count_interval,(long long)totNumOps); 

			// ** average degree of use **
			num = 0;
			avg = 0.0;
			for(i = 0; i < MAX_REG_USE; i++){
				num += regUseDistr[i];
			}
			fprintf(out_file_reg," %lld",(long long)num);
			num = 0;
			for(i = 0; i < MAX_REG_USE; i++){
				num += i * regUseDistr[i];
				//avg += (double) i * (double) regUseDistr[i] / (double) num;
			}
			fprintf(out_file_reg," %lld",(long long)num); 

			// ** register dependency distributions **
			num = 0;
			avg = 0.0;
			for(i = 0; i < MAX_COMM_DIST; i++){
				num += regAgeDistr[i];
			}
			fprintf(out_file_reg," %lld",(long long)num);
			num = 0;
			for(i = 0; i < MAX_COMM_DIST; i++){
				num += regAgeDistr[i];
				//avg += (double) regAgeDistr[i] / (double) num;
				if( (i == 1) || (i == 2) || (i == 4) || (i == 8) || (i == 16) || (i == 32) || (i == 64)){
					fprintf(out_file_reg," %lld",(long long)num);
				}
			}
			fprintf(out_file_reg,"\n");
		}
		if(mode==ALL || mode==STRIDE){
			// *** STRIDE ***
			UINT64 cum;
			// ** local read distribution **
			cum = 0;
			fprintf(out_file_stride,"%lld",(long long)numReadInstrsAnalyzed);
			for(i = 0; i < MAX_DISTR; i++){
				cum += localReadDistrib[i];
				if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) || (i == 262144) ){
					if(cum > 0)
						fprintf(out_file_stride," %lld", (long long) cum);
					else // cum == 0 => numReadInstrsAnalyzed == 0	
						fprintf(out_file_stride," %d", 0);
				}
				if(i == 262144)
					break;
			}
			// ** global read distribution **
			cum = 0;
			for(i = 0; i < MAX_DISTR; i++){
				cum += globalReadDistrib[i];
				if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) || (i == 262144) ){
					if(cum > 0)
						fprintf(out_file_stride," %lld", (long long) cum);
					else // cum == 0 => numReadInstrsAnalyzed == 0	
						fprintf(out_file_stride," %d", 0);
				}
				if(i == 262144)
					break;
			}
			fprintf(out_file_stride," %lld",(long long)numWriteInstrsAnalyzed);
			// ** local write distribution **
			cum = 0;
			for(i = 0; i < MAX_DISTR; i++){
				cum += localWriteDistrib[i];
				if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) || (i == 262144) ){
					if(cum > 0)
						fprintf(out_file_stride," %lld", (long long) cum);
					else // cum == 0 => numWriteInstrsAnalyzed == 0	
						fprintf(out_file_stride," %d", 0);
				}
				if(i == 262144)
					break;
			}
			// ** global write distribution **
			cum = 0;
			for(i = 0; i < MAX_DISTR; i++){
				cum += globalWriteDistrib[i];
				if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) ){
					if(cum > 0)
						fprintf(out_file_stride," %lld", (long long) cum);
					else // cum == 0 => numWriteInstrsAnalyzed == 0	
						fprintf(out_file_stride," %d", 0);
				}
				if(i == 262144){
					if(cum > 0)
						fprintf(out_file_stride," %lld\n", (long long) cum);
					else // cum == 0 => numWriteInstrsAnalyzed == 0	
						fprintf(out_file_stride," %d\n", 0);
					break;
				}
			}
		}


		if(mode==ALL || mode==MEMFOOTPRINT || mode==ILP_MEMFOOTPRINT){
			/* *** MEMFOOTPRINT *** */
			long long DmemCacheWorkingSetSize = 0L;
			long long DmemPageWorkingSetSize = 0L;
			long long ImemCacheWorkingSetSize = 0L;
			long long ImemPageWorkingSetSize = 0L;
			nlist* np;

			for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
				for (np = DmemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
					for (j = 0; j < MAX_MEM_BLOCK; j++) {
						if ((np->mem)->numReferenced [j] > 0) {
							DmemCacheWorkingSetSize++;
						}
					}
				}
			}
			for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
				for (np = DmemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
					for (j = 0; j < MAX_MEM_BLOCK; j++) {
						if ((np->mem)->numReferenced [j] > 0) {
							DmemPageWorkingSetSize++;
						}
					}
				}
			}
			for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
				for (np = ImemCacheWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
					for (j = 0; j < MAX_MEM_BLOCK; j++) {
						if ((np->mem)->numReferenced [j] > 0) {
							ImemCacheWorkingSetSize++;
						}
					}
				}
			}
			for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
				for (np = ImemPageWorkingSetTable [i]; np != (nlist*) NULL; np = np->next) {
					for (j = 0; j < MAX_MEM_BLOCK; j++) {
						if ((np->mem)->numReferenced [j] > 0) {
							ImemPageWorkingSetSize++;
						}
					}
				}
			}
			fprintf(out_file_memfootprint,"%lld: %lld : %lld : %lld\n",DmemCacheWorkingSetSize,DmemPageWorkingSetSize,ImemCacheWorkingSetSize,ImemPageWorkingSetSize); 
		}

	}
	if(mode==ALL || mode==ILP || mode==ILP_MEMFOOTPRINT){
		fprintf(out_file_ilp,"number of instructions: %lld\n",(long long)total_count);
	}
	if(mode==ALL || mode==ITYPES){
		fprintf(out_file_itypes,"number of instructions: %lld\n",(long long)total_count);
	}
	if(mode==ALL || mode==PPM){
		fprintf(out_file_ppm,"number of instructions: %lld\n",(long long)total_count);
	}
	if(mode==ALL || mode==REG_MODE){
		fprintf(out_file_reg,"number of instructions: %lld\n",(long long)total_count);
	}
	if(mode==ALL || mode==STRIDE){
		fprintf(out_file_stride,"number of instructions: %lld\n",(long long)total_count);
	}
	if(mode==ALL || mode==MEMFOOTPRINT || mode==ILP_MEMFOOTPRINT){
		fprintf(out_file_memfootprint,"number of instructions: %lld\n",(long long)total_count);
	}
	/* freeing all mallocced memory */

	/* ilp and memfootprint */

	if(mode==ALL || mode==ILP || mode==ILP_MEMFOOTPRINT){
		for(i=0; i < NUM_WINDOW_SIZES; i++)
			free(all_times[i]);
		fclose(out_file_ilp);
	}
	nlist* np;
	nlist* np_rm;
	if(mode==ALL || mode==MEMFOOTPRINT || mode==ILP_MEMFOOTPRINT){
		for(i=0; i < MAX_MEM_TABLE_ENTRIES; i++){
			np = memAddressesTable[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm);
			}
			np = DmemCacheWorkingSetTable[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm);
			}
			np = DmemPageWorkingSetTable[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm);
			}
			np = ImemCacheWorkingSetTable[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm);
			}
			np = ImemPageWorkingSetTable[i];
			while(np != (nlist*)NULL){
				np_rm = np;
				np = np->next;
				free(np_rm);
			}
		}
		fclose(out_file_memfootprint);
	}
	/* ppm */
	if(mode==ALL || mode==PPM){
		free(local_bhr);
		for(j = 0; j < NUM_HIST_LENGTHS; j++){
			for(i = 0; i <= history_lengths[j]; i++) {
				free(GAg_pht[j][i]);
				free(PAg_pht[j][i]);
			}
			free(GAg_pht[j]);
			free(PAg_pht[j]);
		}
		free(GAg_pht);
		free(PAg_pht);
		int id;
		for(id = 0; id < brHist_size; id++){
			if(GAs_touched[id]){
				for(j = 0; j < NUM_HIST_LENGTHS; j++){
					for(i = 0; i <= history_lengths[j]; i++){
						free(GAs_pht[id][j][i]);
					}
					free(GAs_pht[id][j]);
				}
				free(GAs_pht[id]);
			}
			if(PAs_touched[id]){
				for(j = 0; j < NUM_HIST_LENGTHS; j++){
					for(i = 0; i <= history_lengths[j]; i++){
						free(PAs_pht[id][j][i]);
					}
					free(PAs_pht[id][j]);
				}
				free(PAs_pht[id]);
			}
		}
		free(GAs_pht);
		free(PAs_pht);
		free(GAs_touched);
		free(PAs_touched);
		free(transition_counts);
		free(local_taken);
		free(local_brCounts);
		free(local_taken_counts);
		fclose(out_file_ppm);
	}
	/* register traffic */
	if(mode==ALL || mode==REG_MODE){
		free(opCounts);
		free(regRef);
		free(PCTable);
		free(regUseCnt);
		free(regUseDistr);
		free(regAgeDistr);
		fclose(out_file_reg);
	}
	/* stride */ 
	if(mode == ALL || mode==STRIDE){
		free(instrRead);
		free(instrWrite);
		fclose(out_file_stride);
	}    

	if(mode==ALL || mode==ITYPES){
		fclose(out_file_itypes);
	}
	fclose(instr_count_output);
}

/* ==========================================
 *                     MAIN 
 * ========================================== */

// argc, argv are the entire command line, including pin -t <toolname> -- ...
int main(int argc, char * argv[])
{
    int i,j,k;
    int index;
    int argc_PIN = 0;

    //fprintf(stderr,"INITIALIZING...\n");

    char* path;
    if((path = (char*)malloc(100*sizeof(char))) == (char*)NULL){
	fprintf(stderr,"Could not allocate memory\n");
	exit(1);
    }
    //fprintf(stderr,"malloc %d bytes\n",sizeof(nlist));

    total_count = 0;
    total_count_interval = 0;

    instr_count_output = fopen("instr_count_output.txt","w");

    /* parse command line arguments */
    index = 0;
    if(strcmp("-pause_tool",argv[1])==0){
	index = 2;
    }
    for(i=0; i < argc; i++){
	if(strcmp(argv[i],"--")==0)
		break;
	argc_PIN++;
    }
    if(argc_PIN != index+5){
	fprintf(stderr,"ERROR! Usage: %s [-pause-tool S] -t %s <interval size | full> <all | ilp | itypes | ppm | reg | stride | memfootprint | ilp_memfootprint> (wrong arg.count: %d)\n",argv[0],argv[index+2],argc_PIN);
    	exit(1);	
    }
    if(strcmp(argv[index+4],"all") == 0){
	mode = ALL;
	//MEMFOOTPRINT_ONLY = 0;
    }
    else{
	if(strcmp(argv[index+4],"ilp") == 0){
		mode = ILP;
    	}
   	else{
		if(strcmp(argv[index+4],"itypes") == 0){
			mode = ITYPES;
		}
		else{
			if(strcmp(argv[index+4],"ppm") == 0){
				mode = PPM;
			}
			else{
				if(strcmp(argv[index+4],"reg") == 0){
					mode = REG_MODE;
				}
				else{
					if(strcmp(argv[index+4],"stride") == 0){
						mode = STRIDE;
					}
					else{
						if(strcmp(argv[index+4],"memfootprint") == 0){
							mode = MEMFOOTPRINT;
						}
						else{
							if(strcmp(argv[index+4],"ilp_memfootprint") == 0){
								mode = ILP_MEMFOOTPRINT;
							}
							else{
								fprintf(stderr,"ERROR: Please one of the following to specify which characteristics to measure: 'all','ilp','itypes','ppm','reg','stride','memfootprint','ilp_memfootprint'\n");
							exit(1);
							}
						}
					}
				}
			}
		}
    	}
    }
    if(strcmp(argv[index+3],"full") == 0){
	    full_run = true;
	    interval_size = 1;
	    fprintf(stderr,"Returning data for full execution...\n");
    }
    else{
	    full_run = false;
	    interval_size = (INT64) atoll(argv[index+3]);
	    fprintf(stderr,"Returning data for each interval of %lld instructions...\n", (long long)interval_size);
    }
	

    switch(mode){
	case ALL:
		fprintf(stderr,"Measuring all %d characteristics...\n",(int)CHAR_CNT);
		break;
	case ILP:
    		fprintf(stderr,"Only measuring ILP characteristics...\n");
		break;
	case ITYPES:
    		fprintf(stderr,"Only measuring ITYPES characteristics...\n");
		break;
	case PPM:
    		fprintf(stderr,"Only measuring PPM characteristics...\n");
		break;
	case REG_MODE:
    		fprintf(stderr,"Only measuring REG characteristics...\n");
		break;
	case STRIDE:
    		fprintf(stderr,"Only measuring STRIDE characteristics...\n");
		break;
	case MEMFOOTPRINT:
    		fprintf(stderr,"Only measuring MEMFOOTPRINT characteristics...\n");
		break;
	case ILP_MEMFOOTPRINT:
    		fprintf(stderr,"Only measuring ILP & MEMFOOTPRINT characteristics...\n");
		break;
	default:
		fprintf(stderr,"ERROR: Unknown mode selected!\n");
		exit(1);
    }

    if(mode==ALL || mode==ILP || mode==ILP_MEMFOOTPRINT){
	    strcpy(path,"ilp_win_phases32-64-128-256_int_pin.out");
	    out_file_ilp = fopen(path,"w");
    }
    if(mode==ALL || mode==ITYPES){
	    strcpy(path,"itypes_phases_int_pin.out");
	    out_file_itypes = fopen(path,"w");
    }
    if(mode==ALL || mode==PPM){
	    strcpy(path,"ppm_phases_int_pin.out");
	    out_file_ppm = fopen(path,"w");
    }
    if(mode==ALL || mode==REG_MODE){
	    strcpy(path,"reg_phases_int_pin.out");
	    out_file_reg = fopen(path,"w");
    }
    if(mode==ALL || mode==STRIDE){
	    strcpy(path,"stride_phases_int_pin.out");
	    out_file_stride = fopen(path,"w");
    }
    if(mode==ALL || mode==MEMFOOTPRINT || mode==ILP_MEMFOOTPRINT){
	    strcpy(path,"memfootprint_phases_pin.out");
	    out_file_memfootprint = fopen(path,"w");
    }

    /* shared */
    
    if(mode==ALL || mode==ILP || mode==STRIDE || mode==ILP_MEMFOOTPRINT){

	    /* translation of instruction address to indices */
	    indices_memRead_size = 1024;
	    if( (indices_memRead = (ADDRINT*) malloc(indices_memRead_size*sizeof(ADDRINT))) == (ADDRINT*)NULL){
		    fprintf(stderr,"Could not allocate memory for indices_memRead\n");
		    exit(1);
	    }
	    
	    indices_memWrite_size = 1024;
	    if( (indices_memWrite = (ADDRINT*) malloc(indices_memWrite_size*sizeof(ADDRINT))) == (ADDRINT*)NULL){
		    fprintf(stderr,"Could not allocate memory for indices_memWrite\n");
		    exit(1);
	    }
    }


    /* *** ILP *** */ 

    if(mode==ALL || mode==ILP || mode==ILP_MEMFOOTPRINT){
	    size_pow_times = 10;
	    for(j=0; j < NUM_WINDOW_SIZES; j++){
		    if((all_times[j] = (UINT64*)malloc((1 << size_pow_times) * sizeof(UINT64))) == (UINT64*)NULL){
			    fprintf(stderr,"Could not allocate memory\n");
			    exit(1);
		    }
		    //fprintf(stderr,"malloc %ld bytes (all_times)\n",(long)(1 << size_pow_times)*sizeof(UINT64));
	    }
	    index_all_times = 1; // don't use first element of all_times


	    win_size[0] = 32;
	    win_size[1] = 64;
	    win_size[2] = 128;
	    win_size[3] = 256;

	    for(j=0; j < NUM_WINDOW_SIZES; j++){ 
		    windowHead[j] = 0;
		    windowTail[j] = 0;
		    cpuClock[j] = 0;
		    cpuClock_interval[j] = 0;
		    for(i = 0; i < MAX_NUM_REGS; i++){
			    timeAvailable[j][i] = 0;
		    }

		    if((executionProfile[j] = (UINT64*)malloc(win_size[j]*sizeof(UINT64))) == (UINT64*)NULL){
			    fprintf(stderr,"Not enough memory (in main)\n");
			    exit(1);
		    }
		    //fprintf(stderr,"malloc %d bytes\n",win_size[j]*sizeof(UINT64));

		    for(i = 0; i < (int)win_size[j]; i++){
			    executionProfile[j][i] = 0;
		    }
		    issueTime[j] = 0;
	    }
    }

    /* *** ITYPES *** */

    if(mode==ALL || mode==ITYPES){
	    mem_read_cnt = 0;
	    mem_write_cnt = 0;
	    control_cnt = 0;
	    arith_cnt = 0;
	    fp_cnt = 0;
	    stack_cnt = 0;
	    shift_cnt = 0;
	    string_cnt = 0;
	    sse_cnt = 0;
	    other_cnt = 0;
    }

    /* *** PPM *** */

    if(mode==ALL || mode==PPM){
	    brHist_size = 512;

	    numStatCondBranchInst = 1;

	    /* translation of instruction address to indices */
	    indices_condBr_size = 1024;
	    if( (indices_condBr = (ADDRINT*) malloc(indices_condBr_size*sizeof(ADDRINT))) == (ADDRINT*)NULL){
		    fprintf(stderr,"Could not allocate memory for indices_condBr\n");
		    exit(1);
	    }

	    lastInstBr = false;
	    /* global/local history */
	    bhr = 0;
	    if((local_bhr = (int*) malloc (brHist_size * sizeof(int))) == (int*) NULL) {
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }
	    //fprintf(stderr,"malloc %d bytes\n",brHist_size*sizeof(int));

	    // GAg PPM predictor
	    if((GAg_pht = ((char***) malloc (NUM_HIST_LENGTHS * sizeof(char**)))) == (char***) NULL) {
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }

	    for(j = 0; j < NUM_HIST_LENGTHS; j++) {
		    if((GAg_pht[j] = (char**) malloc((history_lengths[j]+1)*sizeof(char*))) == (char**) NULL) {
			    fprintf(stderr,"Could not allocate memory\n");
			    exit(1);
		    }
		    for(i = 0; i <= history_lengths[j]; i++){
			    if((GAg_pht[j][i] = (char*) malloc((1 << i)*sizeof(char))) == (char*) NULL) {
				    fprintf(stderr,"Could not allocate memory\n");
				    exit(1);
			    }
			    for(k = 0; k < (1 << i); k++)
				    GAg_pht[j][i][k] = 0;
		    }
	    }

	    // PAg PPM predictor
	    if((PAg_pht = ((char***) malloc (NUM_HIST_LENGTHS * sizeof(char**)))) == (char***) NULL) {
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }

	    for(j = 0; j < NUM_HIST_LENGTHS; j++) {
		    if((PAg_pht[j] = (char**) malloc((history_lengths[j]+1)*sizeof(char*))) == (char**) NULL) {
			    fprintf(stderr,"Could not allocate memory\n");
			    exit(1);
		    }
		    for(i = 0; i <= history_lengths[j]; i++){
			    if((PAg_pht[j][i] = (char*) malloc((1 << i)*sizeof(char))) == (char*) NULL) {
				    fprintf(stderr,"Could not allocate memory\n");
				    exit(1);
			    }
			    for(k = 0; k < (1 << i); k++)
				    PAg_pht[j][i][k] = 0;
		    }
	    }

	    // GAs PPM predictor
	    if((GAs_touched = (char*) malloc (brHist_size * sizeof(char))) == (char*) NULL){
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }
	    //fprintf(stderr,"malloc %d bytes\n",brHist_size*sizeof(char));

	    if((GAs_pht = (char****) malloc (brHist_size * sizeof(char***))) == (char****) NULL) { 
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }
	    //fprintf(stderr,"malloc %d bytes\n",brHist_size*sizeof(char**));

	    // PAs PPM predictor
	    if((PAs_touched = (char*) malloc (brHist_size * sizeof(char))) == (char*) NULL) {	
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }
	    //fprintf(stderr,"malloc %d bytes\n",brHist_size*sizeof(char));

	    if((PAs_pht = (char****) malloc (brHist_size * sizeof(char***))) == (char****) NULL) {
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }
	    //fprintf(stderr,"malloc %d bytes\n",brHist_size*sizeof(char**));

	    if((transition_counts = (INT64*) malloc (brHist_size * sizeof(INT64))) == (INT64*) NULL) {
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }
	    //total_mem_br += brHist_size*sizeof(INT64);
	    //fprintf(stderr,"malloc %d bytes\n",brHist_size*sizeof(INT64));

	    if((local_taken = (char*) malloc (brHist_size * sizeof(char))) == (char*) NULL) {
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }
	    //total_mem_br += brHist_size*sizeof(char);
	    //fprintf(stderr,"malloc %d bytes\n",brHist_size*sizeof(char));

	    if((local_brCounts = (INT64*) malloc (brHist_size * sizeof(INT64))) == (INT64*) NULL) {
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }
	    //total_mem_br += brHist_size*sizeof(INT64);
	    //fprintf(stderr,"malloc %d bytes\n",brHist_size*sizeof(INT64));

	    if((local_taken_counts = (INT64*) malloc (brHist_size * sizeof(INT64))) == (INT64*) NULL) {
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }

	    for(i = 0; i < brHist_size; i++){
		    transition_counts[i] = 0;
		    local_taken[i] = -1;
		    local_brCounts[i] = 0;
		    local_taken_counts[i] = 0;
		    GAs_touched[i] = 0;
		    PAs_touched[i] = 0;
	    } 

	    for(j=0; j < NUM_HIST_LENGTHS; j++){
		    // number of incorrect predictions
		    GAg_incorrect_pred[j] = 0;
		    GAs_incorrect_pred[j] = 0;
		    PAg_incorrect_pred[j] = 0;
		    PAs_incorrect_pred[j] = 0;
	    }
    }

    /* *** REG *** */

    if(mode==ALL || mode==REG_MODE){
	    /* allocate memory */
	    if((opCounts = (UINT64*) malloc(MAX_NUM_OPER * sizeof(UINT64))) == (UINT64*)NULL){
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }
	    //fprintf(stderr,"malloc %d bytes\n",MAX_NUM_OPER*sizeof(UINT64));

	    if((regRef = (BOOL*) malloc(MAX_NUM_REGS * sizeof(BOOL))) == (BOOL*)NULL){
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }
	    //fprintf(stderr,"malloc %d bytes\n",MAX_NUM_REGS*sizeof(BOOL));

	    if((PCTable = (INT64*) malloc(MAX_NUM_REGS * sizeof(INT64))) == (INT64*)NULL){
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }
	    //fprintf(stderr,"malloc %d bytes\n",MAX_NUM_REGS*sizeof(INT64));

	    if((regUseCnt = (INT64*) malloc(MAX_NUM_REGS * sizeof(INT64))) == (INT64*)NULL){
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }
	    //fprintf(stderr,"malloc %d bytes\n",MAX_NUM_REGS*sizeof(INT64));

	    if((regUseDistr = (INT64*) malloc(MAX_REG_USE * sizeof(INT64))) == (INT64*)NULL){
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }
	    //fprintf(stderr,"malloc %d bytes\n",MAX_NUM_REGS*sizeof(INT64));

	    if((regAgeDistr = (INT64*) malloc(MAX_COMM_DIST * sizeof(INT64))) == (INT64*)NULL){
		    fprintf(stderr,"Could not allocate memory\n");
		    exit(1);
	    }
	    //fprintf(stderr,"malloc %d bytes\n",MAX_NUM_REGS*sizeof(INT64));

	    /* initialize */
	    for(i = 0; i < MAX_NUM_OPER; i++){
		    opCounts[i] = 0;
	    }
	    for(i = 0; i < MAX_NUM_REGS; i++){
		    regRef[i] = false;
		    PCTable[i] = 0;
		    regUseCnt[i] = 0;
	    }
	    for(i = 0; i < MAX_REG_USE; i++){
		    regUseDistr[i] = 0;
	    }
	    for(i = 0; i < MAX_COMM_DIST; i++){
		    regAgeDistr[i] = 0;
	    }
    }
	
    /* *** STRIDE *** */
    
    if(mode==ALL || mode==ILP || mode==STRIDE || mode==ILP_MEMFOOTPRINT){
	    /* initial sizes */
	    numRead = 1024;
	    numWrite = 1024;

	    /* allocate memory */
	    if ((instrRead = (ADDRINT*) malloc (numRead * sizeof (ADDRINT))) == (ADDRINT*) NULL) {
		    fprintf (stderr, "Not enough memory (in main (2))\n");
		    exit (0);
	    }
	    //fprintf(stderr,"malloc %d bytes\n",numRead*sizeof(ADDRINT));

	    if ((instrWrite = (ADDRINT*) malloc (numWrite * sizeof (ADDRINT))) == (ADDRINT*) NULL) {
		    fprintf (stderr, "Not enough memory (in main (3))\n");
		    exit (0);
	    }
	    //fprintf(stderr,"malloc %d bytes\n",numWrite*sizeof(ADDRINT));

	    /* initialize */
	    readIndex = 1;
	    writeIndex = 1;
	    for (i = 0; i < (int)numRead; i++)
		    instrRead [i] = 0;
	    for (i = 0; i < (int)numWrite; i++)
		    instrWrite [i] = 0;
	    lastReadAddr = 0;
	    lastWriteAddr = 0;
	    for (i = 0; i < MAX_DISTR; i++) {
		    localReadDistrib [i] = 0;
		    localWriteDistrib [i] = 0;
		    globalReadDistrib [i] = 0;
		    globalWriteDistrib [i] = 0;
	    }
	    numInstrsAnalyzed = 0;
	    numReadInstrsAnalyzed = 0;
	    numWriteInstrsAnalyzed = 0;
    }

    /* *** MEMFOOTPRINT *** */
   
    if(mode==ALL || mode==MEMFOOTPRINT || mode==ILP_MEMFOOTPRINT){ 
	    /* initialize tables */
	    for (i = 0; i < MAX_MEM_BLOCK_ENTRIES; i++) {
		    DmemCacheWorkingSetTable[i] = (nlist*) NULL;
		    DmemPageWorkingSetTable[i] = (nlist*) NULL;
		    ImemCacheWorkingSetTable[i] = (nlist*) NULL;
		    ImemPageWorkingSetTable[i] = (nlist*) NULL;
	    }
    }

    /* *** PIN *** */

    // Initialize pin
    PIN_Init(argc, argv);

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    //fprintf(stderr,"STARTING APP...\n");
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
