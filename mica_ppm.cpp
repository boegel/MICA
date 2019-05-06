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
#include "mica_ppm.h"
#include "mica_utils.h"

/* Global variables */

extern INT64 interval_size;
extern INT64 interval_ins_count;
extern INT64 interval_ins_count_for_hpc_alignment;
extern INT64 total_ins_count;
extern INT64 total_ins_count_for_hpc_alignment;

ofstream output_file_ppm;

BOOL lastInstBr; // was the last instruction a cond. branch instruction?
ADDRINT nextAddr; // address of the instruction after the last cond.branch
UINT32 numStatCondBranchInst; // number of static cond. branch instructions up until now (-> unique id for the cond. branch)
//UINT32 lastBrId; // index of last cond. branch instruction
INT64* transition_counts;
char* local_taken;
INT64* local_taken_counts;
INT64* local_brCounts;
ADDRINT* indices_condBr;
UINT32 indices_condBr_size;
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
char*** GAg_pht;
char*** PAg_pht;
char**** GAs_pht;
char**** PAs_pht;
/* check if page entries were touched (memory efficiency) */
char* GAs_touched;
char* PAs_touched;
/* prediction history */
int GAg_pred_hist[NUM_HIST_LENGTHS];
int PAg_pred_hist[NUM_HIST_LENGTHS];
int GAs_pred_hist[NUM_HIST_LENGTHS];
int PAs_pred_hist[NUM_HIST_LENGTHS];

/* initializing */
void init_ppm(){

	UINT32 i,j;
	int k;

	/* initializing total instruction counts is done in mica.cpp */

	brHist_size = 512;

	numStatCondBranchInst = 1;

	/* translation of instruction address to indices */
	indices_condBr_size = 1024;
	indices_condBr = (ADDRINT*) checked_malloc(indices_condBr_size*sizeof(ADDRINT));

	lastInstBr = false;

	/* global/local history */
	bhr = 0;
	local_bhr = (int*) checked_malloc(brHist_size * sizeof(int));

	/* GAg PPM predictor */
	GAg_pht = (char***) checked_malloc(NUM_HIST_LENGTHS * sizeof(char**));
	for(j = 0; j < NUM_HIST_LENGTHS; j++) {
		GAg_pht[j] = (char**) checked_malloc((history_lengths[j]+1)*sizeof(char*));
		for(i = 0; i <= history_lengths[j]; i++){
			GAg_pht[j][i] = (char*) checked_malloc((1 << i)*sizeof(char));
			for(k = 0; k < (1 << i); k++)
				GAg_pht[j][i][k] = 0;
		}
	}

	/* PAg PPM predictor */
	PAg_pht = (char***) checked_malloc(NUM_HIST_LENGTHS * sizeof(char**));
	for(j = 0; j < NUM_HIST_LENGTHS; j++) {
		PAg_pht[j] = (char**) checked_malloc((history_lengths[j]+1)*sizeof(char*));
		for(i = 0; i <= history_lengths[j]; i++){
			PAg_pht[j][i] = (char*) checked_malloc((1 << i)*sizeof(char));
			for(k = 0; k < (1 << i); k++)
				PAg_pht[j][i][k] = 0;
		}
	}

	/* GAs PPM predictor */
	GAs_touched = (char*) checked_malloc(brHist_size * sizeof(char));
	GAs_pht = (char****) checked_malloc(brHist_size * sizeof(char***));

	/* PAs PPM predictor */
	PAs_touched = (char*) checked_malloc(brHist_size * sizeof(char));
	PAs_pht = (char****) checked_malloc(brHist_size * sizeof(char***));

	transition_counts = (INT64*) checked_malloc(brHist_size * sizeof(INT64));
	local_taken = (char*) checked_malloc(brHist_size * sizeof(char));
	local_brCounts = (INT64*) checked_malloc(brHist_size * sizeof(INT64));
	local_taken_counts = (INT64*) checked_malloc(brHist_size * sizeof(INT64));

	for(i = 0; i < brHist_size; i++){
		transition_counts[i] = 0;
		local_taken[i] = -1;
		local_brCounts[i] = 0;
		local_taken_counts[i] = 0;
		GAs_touched[i] = 0;
		PAs_touched[i] = 0;
	}

	for(j=0; j < NUM_HIST_LENGTHS; j++){
		GAg_incorrect_pred[j] = 0;
		GAs_incorrect_pred[j] = 0;
		PAg_incorrect_pred[j] = 0;
		PAs_incorrect_pred[j] = 0;
	}

	if(interval_size != -1){
		output_file_ppm.open(mkfilename("ppm_phases_int"), ios::out|ios::trunc);
		output_file_ppm.close();
	}

}

/*VOID ppm_instr_full(){
}*/

ADDRINT ppm_instr_intervals(){

	return (ADDRINT)(interval_ins_count_for_hpc_alignment == interval_size);
}

VOID ppm_instr_interval_output(){
	int i;
	INT64 total_transition_count = 0;
	INT64 total_taken_count = 0;
	INT64 total_brCount = 0;

	output_file_ppm.open(mkfilename("ppm_phases_int"), ios::out|ios::app);

	output_file_ppm << interval_size;
	for(i = 0; i < NUM_HIST_LENGTHS; i++)
		output_file_ppm << " " << GAg_incorrect_pred[i] << " " << PAg_incorrect_pred[i] << " " << GAs_incorrect_pred[i] << " " << PAs_incorrect_pred[i];

	for(i=0; i < brHist_size; i++){
		if(local_brCounts[i] > 0){
			if( transition_counts[i] > local_brCounts[i]/2)
				total_transition_count += local_brCounts[i]-transition_counts[i];
			else
				total_transition_count += transition_counts[i];

			if( local_taken_counts[i] > local_brCounts[i]/2)
				total_taken_count += local_brCounts[i] - local_taken_counts[i];
			else
				total_taken_count += local_taken_counts[i];
			total_brCount += local_brCounts[i];
		}
	}
	output_file_ppm << " " << total_brCount << " " << total_transition_count << " " << total_taken_count << endl;
	output_file_ppm.close();
}

VOID ppm_instr_interval_reset(){

	int i;

	for(i = 0; i < NUM_HIST_LENGTHS; i++){
		GAg_incorrect_pred[i] = 0;
		GAs_incorrect_pred[i] = 0;
		PAg_incorrect_pred[i] = 0;
		PAs_incorrect_pred[i] = 0;
	}
	for(i=0; i < brHist_size; i++){
		local_brCounts[i] = 0;
		local_taken_counts[i] = 0;
		transition_counts[i] = 0;
	}
}

VOID ppm_instr_interval(){


	ppm_instr_interval_output();
	ppm_instr_interval_reset();

	interval_ins_count = 0;
	interval_ins_count_for_hpc_alignment = 0;
}

/* double memory space for branch history size when needed */
VOID reallocate_brHist(){

	INT32* int_ptr;
	char* char_ptr;
	char**** char4_ptr;
	INT64* int64_ptr;

	brHist_size = brHist_size*2;

	int_ptr = (INT32*) checked_realloc(local_bhr,brHist_size * sizeof(INT32));
	/*if(int_ptr == (INT32*) NULL) {
		cerr << "Could not allocate memory" << endl;
		exit(1);
	}*/
	local_bhr = int_ptr;

	char_ptr = (char*) checked_realloc(GAs_touched, brHist_size * sizeof(char));
	/*if(char_ptr == (char*) NULL){
		cerr << "Could not allocate memory" << endl;
		exit(1);
	}*/
	GAs_touched = char_ptr;

	char4_ptr = (char****) checked_realloc(GAs_pht,brHist_size * sizeof(char***));
	/*if(char4_ptr == (char****) NULL) {
		cerr << "Could not allocate memory" << endl;
		exit(1);
	}*/
	GAs_pht = char4_ptr;

	char_ptr = (char*) checked_realloc(PAs_touched,brHist_size * sizeof(char));
	/*if(char_ptr == (char*) NULL) {
		cerr << "Could not allocate memory" << endl;
		exit(1);
	}*/
	PAs_touched = char_ptr;

	char4_ptr = (char****) checked_realloc(PAs_pht,brHist_size * sizeof(char***));
	/*if(char4_ptr == (char****) NULL) {
		cerr << "Could not allocate memory" << endl;
		exit(1);
	}*/
	PAs_pht = char4_ptr;

	char_ptr = (char*) checked_realloc(local_taken,brHist_size * sizeof(char));
	/*if(char_ptr == (char*) NULL) {
		cerr << "Could not allocate memory" << endl;
		exit(1);
	}*/
	local_taken = char_ptr;

	int64_ptr = (INT64*) realloc(transition_counts, brHist_size * sizeof(INT64));
	/*if(int64_ptr == (INT64*)NULL) {
		cerr,"Could not allocate memory" << endl;
		exit(1);
	}*/
	transition_counts = int64_ptr;

	int64_ptr = (INT64*) realloc(local_brCounts, brHist_size * sizeof(INT64));
	/*if(int64_ptr == (INT64*)NULL) {
		cerr << "Could not allocate memory" << endl;
		exit(1);
	}*/
	local_brCounts = int64_ptr;

	int64_ptr = (INT64*) realloc(local_taken_counts, brHist_size * sizeof(INT64));
	/*if(int64_ptr == (INT64*)NULL) {
		cerr << "Could not allocate memory" << endl;
		exit(1);
	}*/
	local_taken_counts = int64_ptr;
}


VOID condBr(UINT32 id, BOOL _t){

	int i,j,k;
	int hist;
	BOOL taken = (_t != 0) ? 1 : 0;

	/* predict direction */

	/* GAs PPM predictor lookup */
	if(!GAs_touched[id]){
		/* allocate PPM predictor */

		GAs_touched[id] = 1;

		GAs_pht[id] = (char***) checked_malloc(NUM_HIST_LENGTHS * sizeof(char**));
		for(j = 0; j < NUM_HIST_LENGTHS; j++){
			GAs_pht[id][j] = (char**) checked_malloc((history_lengths[j]+1) * sizeof(char*));
			for(i = 0; i <= (int)history_lengths[j]; i++){
				GAs_pht[id][j][i] = (char*) checked_malloc((1 << i) * sizeof(char));
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

		PAs_pht[id] = (char***) checked_malloc(NUM_HIST_LENGTHS * sizeof(char**));
		for(j = 0; j < NUM_HIST_LENGTHS; j++){
			PAs_pht[id][j] = (char**) checked_malloc((history_lengths[j]+1) * sizeof(char*));
			for(i = 0; i <= (int)history_lengths[j]; i++){
				PAs_pht[id][j][i] = (char*) checked_malloc((1 << i) * sizeof(char));
				for(k = 0; k < (1 << i); k++){
					PAs_pht[id][j][i][k] = -1;
				}
			}
		}
	}

	for(j = 0; j < NUM_HIST_LENGTHS; j++){
		/* GAg PPM predictor lookup */
		for(i = (int)history_lengths[j]; i >= 0; i--){

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
		for(i = (int)history_lengths[j]; i >= 0; i--){
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
		for(i = (int)history_lengths[j]; i >= 0; i--){
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
		for(i = (int)history_lengths[j]; i >= 0; i--){
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

	/* transition/taken rate */
	if(local_taken[id] > -1){
		if(taken != local_taken[id])
			transition_counts[id]++;
	}
	local_taken[id] = taken;
	local_brCounts[id]++;
	if(taken)
		local_taken_counts[id]++;

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
		for(i = (int)GAg_pred_hist[j]; i <= (int)history_lengths[j]; i++){
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
		for(i = (int)PAg_pred_hist[j]; i <= (int)history_lengths[j]; i++){
			hist = local_bhr[id] & ((1 << i) - 1);
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
		for(i = (int)GAs_pred_hist[j]; i <= (int)history_lengths[j]; i++){
			hist = bhr & ((1 << i) - 1);
			if(taken){
				if(GAs_pht[id][j][i][hist] < 127)
					GAs_pht[id][j][i][hist]++;
			}
			else{
				if(GAs_pht[id][j][i][hist] > -127)
					GAs_pht[id][j][i][hist]--;
			}
			/* avoid == 0 because that means 'not set' */
			if(GAs_pht[id][j][i][hist] == 0){
				if(taken){
					GAs_pht[id][j][i][hist]++;
				}
				else{
					GAs_pht[id][j][i][hist]--;
				}
			}
		}
		/* update PAs PPM pattern history tables */
		for(i = (int)PAs_pred_hist[j]; i <= (int)history_lengths[j]; i++){
			hist = local_bhr[id] & ((1 << i) - 1);
			if(taken){
				if(PAs_pht[id][j][i][hist] < 127)
					PAs_pht[id][j][i][hist]++;
			}
			else{
				if(PAs_pht[id][j][i][hist] > -127)
					PAs_pht[id][j][i][hist]--;
			}
			/* avoid == 0 because that means 'not set' */
			if(PAs_pht[id][j][i][hist] == 0){
				if(taken){
					PAs_pht[id][j][i][hist]++;
				}
				else{
					PAs_pht[id][j][i][hist]--;
				}
			}
		}
	}

	/* update global history register */
	bhr = bhr << 1;
	bhr |= taken;

	/* update local history */
	local_bhr[id] = local_bhr[id] << 1;
	local_bhr[id] |= taken;
}

/* index for static conditional branch */
UINT32 index_condBr(ADDRINT ins_addr){

	UINT64 i;
	for(i=0; i <= numStatCondBranchInst; i++){
		if(indices_condBr[i] == ins_addr)
			return i; /* found */
	}
	return 0; /* not found */
}

/* register static conditional branch with some index */
void register_condBr(ADDRINT ins_addr){

	ADDRINT* ptr;

	/* reallocation needed */
	if(numStatCondBranchInst >= indices_condBr_size){

		indices_condBr_size *= 2;
		ptr = (ADDRINT*) realloc(indices_condBr, indices_condBr_size*sizeof(ADDRINT));
		/*if(ptr == (ADDRINT*)NULL){
			cerr << "Could not allocate memory (realloc in register_condBr)!" << endl;
			exit(1);
		}*/
		indices_condBr = ptr;

	}

	/* register instruction to index */
	indices_condBr[numStatCondBranchInst++] = ins_addr;
}

// static int _count  = 0;
VOID instrument_ppm_cond_br(INS ins){
    UINT32 index = index_condBr(INS_Address(ins));
	if(index < 1){

		/* We don't know the number of static conditional branch instructions up front,
		 * so we double the size of the branch history tables as needed by calling this function */
		if(numStatCondBranchInst >= brHist_size)
			reallocate_brHist();

		index = numStatCondBranchInst;

		register_condBr(INS_Address(ins));
        register_condBr(INS_Address(ins));
	}
    
    const char* str = INS_Disassemble(ins).c_str();
    const char* substr = "xbegin";
    if (strncmp(str, substr, strlen(substr)) == 0){
        printf("as of pin 3.4 -- I don't think we can parse xbegin so skipping...\n");
        return;
    }
    substr = "xend";
    if (strncmp(str, substr, strlen(substr)) == 0){
        printf("as of pin 3.4 -- I don't think we can parse xend so skipping...\n");
        return;
    }
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)condBr,IARG_UINT32, index, IARG_BRANCH_TAKEN, IARG_END);
}

/* instrumenting (instruction level) */
VOID instrument_ppm(INS ins, VOID* v){

	char cat[50];
	strcpy(cat,CATEGORY_StringShort(INS_Category(ins)).c_str());

	if(strcmp(cat,"COND_BR") == 0){
		instrument_ppm_cond_br(ins);
	}

	/* inserting calls for counting instructions (full) is done in mica.cpp */

	if(interval_size != -1){
		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)ppm_instr_intervals,IARG_END);
		/* only called if interval is 'full' */
		INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)ppm_instr_interval,IARG_END);
	}
}


/* finishing... */
VOID fini_ppm(INT32 code, VOID* v){

	int i;

	if(interval_size == -1){
		output_file_ppm.open(mkfilename("ppm_full_int"), ios::out|ios::trunc);
		output_file_ppm << total_ins_count;
	}
	else{
		output_file_ppm.open(mkfilename("ppm_phases_int"), ios::out|ios::app);
		output_file_ppm << interval_ins_count;
	}
	for(i=0; i < NUM_HIST_LENGTHS; i++)
		output_file_ppm << " " << GAg_incorrect_pred[i] << " " << PAg_incorrect_pred[i] << " " << GAs_incorrect_pred[i] << " " << PAs_incorrect_pred[i];

	INT64 total_transition_count = 0;
	INT64 total_taken_count = 0;
	INT64 total_brCount = 0;
	for(i=0; i < brHist_size; i++){
		if(local_brCounts[i] > 0){
			if( transition_counts[i] > local_brCounts[i]/2)
				total_transition_count += local_brCounts[i]-transition_counts[i];
			else
				total_transition_count += transition_counts[i];

			if( local_taken_counts[i] > local_brCounts[i]/2)
				total_taken_count += local_brCounts[i] - local_taken_counts[i];
			else
				total_taken_count += local_taken_counts[i];
			total_brCount += local_brCounts[i];
		}
	}
	output_file_ppm << " " << total_brCount << " " << total_transition_count << " " << total_taken_count << endl;
	output_file_ppm << "number of instructions: " << total_ins_count_for_hpc_alignment << endl;
	output_file_ppm.close();
}
