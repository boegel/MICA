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
#include "mica_stride.h"

/* Global variables */

extern INT64 interval_size;
extern INT64 interval_ins_count;
extern INT64 total_ins_count;

FILE* output_file_stride;

UINT64 numRead, numWrite;
UINT32 readIndex;
UINT32 writeIndex;
ADDRINT* instrRead;
ADDRINT* instrWrite;
UINT64 numInstrsAnalyzed;
UINT64 numReadInstrsAnalyzed;
UINT64 numWriteInstrsAnalyzed;
UINT64 localReadDistrib[MAX_DISTR];
UINT64 globalReadDistrib[MAX_DISTR];
UINT64 localWriteDistrib[MAX_DISTR];
UINT64 globalWriteDistrib[MAX_DISTR];
ADDRINT lastReadAddr;
ADDRINT lastWriteAddr;
ADDRINT* indices_memRead;
UINT32 indices_memRead_size;
ADDRINT* indices_memWrite;
UINT32 indices_memWrite_size;


/* initializing */
void init_stride(){

	int i;

	/* initializing total instruction counts is done in mica.cpp */

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
		instrRead[i] = 0;
	for (i = 0; i < (int)numWrite; i++)
		instrWrite[i] = 0;
	lastReadAddr = 0;
	lastWriteAddr = 0;
	for (i = 0; i < MAX_DISTR; i++) {
		localReadDistrib[i] = 0;
		localWriteDistrib[i] = 0;
		globalReadDistrib[i] = 0;
		globalWriteDistrib[i] = 0;
	}
	numInstrsAnalyzed = 0;
	numReadInstrsAnalyzed = 0;
	numWriteInstrsAnalyzed = 0;

	indices_memRead_size = 1024;
	if( (indices_memRead = (ADDRINT*) malloc(indices_memRead_size*sizeof(ADDRINT))) == (ADDRINT*)NULL){
		fprintf(stderr,"Could not allocate memory for indices_memRead\n");
		exit(1);
	}
	for (i = 0; i < (int)indices_memRead_size; i++)
		indices_memRead[i] = 0;

	indices_memWrite_size = 1024;
	if( (indices_memWrite = (ADDRINT*) malloc(indices_memWrite_size*sizeof(ADDRINT))) == (ADDRINT*)NULL){
		fprintf(stderr,"Could not allocate memory for indices_memWrite\n");
		exit(1);
	}
	
	for (i = 0; i < (int)indices_memWrite_size; i++)
		indices_memWrite[i] = 0;

	if(interval_size != -1){		
		output_file_stride = fopen("stride_phases_int_pin.out","w");
		fclose(output_file_stride);
	}
}

/*VOID stride_instr_full(){
}*/

ADDRINT stride_instr_intervals(){
	/* counting instructions is done in all_instr_intervals() */

	return (ADDRINT) (total_ins_count % interval_size == 0);
}

VOID stride_instr_interval_output(){
	int i;

	UINT64 cum;

	output_file_stride = fopen("stride_phases_int_pin.out","a");

	fprintf(output_file_stride,"%lld",(long long)numReadInstrsAnalyzed);
	/* local read distribution */
	cum = 0;
	for(i = 0; i < MAX_DISTR; i++){
		cum += localReadDistrib[i];
		if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) || (i == 262144) ){
			if(cum > 0)
				fprintf(output_file_stride," %lld", (long long) cum);
			else
				fprintf(output_file_stride," %d", 0);
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
				fprintf(output_file_stride," %lld", (long long) cum);
			else	
				fprintf(output_file_stride," %d", 0);
		}
		if(i == 262144)
			break;
	}
	fprintf(output_file_stride," %lld",(long long)numWriteInstrsAnalyzed);
	/* local write distribution */
	cum = 0;
	for(i = 0; i < MAX_DISTR; i++){
		cum += localWriteDistrib[i];
		if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) || (i == 262144) ){
			if(cum > 0)
				fprintf(output_file_stride," %lld", (long long) cum);
			else	
				fprintf(output_file_stride," %d", 0);
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
				fprintf(output_file_stride," %lld", (long long) cum);
			else	
				fprintf(output_file_stride," %d", 0);
		}
		if(i == 262144){
			if(cum > 0)
				fprintf(output_file_stride," %lld\n", (long long) cum);
			else	
				fprintf(output_file_stride," %d\n", 0);
			break;
		}
	}
	fclose(output_file_stride);
}

VOID stride_instr_interval_reset(){
	int i;

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

void stride_instr_interval(){

	stride_instr_interval_output();
	stride_instr_interval_reset();
}

/* Finds indices for instruction at some address, given some list of index-instruction pairs 
 * Note: the 'nth_occur' argument is needed because a single instruction can have two read memory operands (which both have a different index) */ 
UINT32 index_memRead_stride(int nth_occur, ADDRINT ins_addr){

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

/* We don't know the static number of read/write operations until 
 * the entire program has executed, hence we dynamically allocate the arrays */
VOID reallocate_readArray_stride(){

	ADDRINT* ptr;

	numRead *= 2;

	ptr = (ADDRINT*) realloc (instrRead, numRead * sizeof (ADDRINT));
	if (ptr == (ADDRINT*) NULL) {
		fprintf (stderr, "Not enough memory (in reallocate_readArray_stride)\n");
		exit (1);
	}
	instrRead = ptr;
}

UINT32 index_memWrite_stride(ADDRINT ins_addr){

	UINT32 i;
	for(i=1; i <= writeIndex; i++){
		if(indices_memWrite[i] == ins_addr)
			return i; /* found */
	}
	return 0; /* not found */
}


VOID reallocate_writeArray_stride(){

	ADDRINT* ptr;

	numWrite *= 2;

	ptr = (ADDRINT*) realloc (instrWrite, numWrite * sizeof (ADDRINT));
	if (ptr == (ADDRINT*) NULL) {
		fprintf (stderr, "Not enough memory (in reallocate_writeArray_stride)\n");
		exit (1);
	}
	instrWrite = ptr;
}

void register_memRead_stride(ADDRINT ins_addr){

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

void register_memWrite_stride(ADDRINT ins_addr){

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

VOID readMem_stride(UINT32 index, ADDRINT effAddr, ADDRINT size){

	ADDRINT stride;

	numReadInstrsAnalyzed++;

	/* local stride	*/
	/* avoid negative values, has to be done like this (not stride < 0 => stride = -stride (avoid problems with unsigned values)) */
	if(effAddr > instrRead[index])
		stride = effAddr - instrRead[index];
	else
		stride = instrRead[index] - effAddr;
	if(stride >= MAX_DISTR){
		stride = MAX_DISTR-1; // trim if needed
	}

	localReadDistrib[stride]++;
	instrRead[index] = effAddr + size;

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
	lastReadAddr = effAddr + size;
}

VOID writeMem_stride(UINT32 index, ADDRINT effAddr, ADDRINT size){

	ADDRINT stride;

	numWriteInstrsAnalyzed++;

	/* local stride */
	/* avoid negative values, has to be doen like this (not stride < 0 => stride = -stride) */
	if(effAddr > instrWrite[index])
		stride = effAddr - instrWrite[index];
	else
		stride = instrWrite[index] - effAddr;
	if(stride >= MAX_DISTR){
		stride = MAX_DISTR-1; // trim if needed
	}

	localWriteDistrib[stride]++;
	instrWrite[index] = effAddr + size;

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
	lastWriteAddr = effAddr + size;
}

UINT32 stride_index_memRead1(ADDRINT a){

	UINT32 index = index_memRead_stride(1, a);
	if(index < 1){
		if(readIndex >= numRead){
			reallocate_readArray_stride();
		}
		index = readIndex;

		register_memRead_stride(a);
	}
	return index;
}

UINT32 stride_index_memRead2(ADDRINT a){
	UINT32 index = index_memRead_stride(2, a);
	if(index < 1){
		if(readIndex >= numRead){
			reallocate_readArray_stride();
		}
		index = readIndex;

		register_memRead_stride(a);
	}
	return index;
}

UINT32 stride_index_memWrite(ADDRINT a){
	UINT32 index = index_memWrite_stride(a);	
	if(index < 1){
		if(writeIndex >= numWrite)
			reallocate_writeArray_stride();
		index = writeIndex;
		register_memWrite_stride(a);
	}
	return index;
}

/* instrumenting (instruction level) */
VOID instrument_stride(INS ins, VOID* v){

	UINT32 index;

	if( INS_IsMemoryRead(ins) ){ // instruction has memory read operand
		
		index = stride_index_memRead1(INS_Address(ins));
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem_stride, IARG_UINT32, index, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
		
		if( INS_HasMemoryRead2(ins) ){ // second memory read operand

			index = stride_index_memRead2(INS_Address(ins));
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readMem_stride, IARG_UINT32, index, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_END);
		}
	}
	
	if( INS_IsMemoryWrite(ins) ){ // instruction has memory write operand
		index =  stride_index_memWrite(INS_Address(ins));
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeMem_stride, IARG_UINT32, index, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
	
	}

	/* inserting calls for counting instructions (full) is done in mica.cpp */

	if(interval_size != -1){
		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)stride_instr_intervals, IARG_END);
		INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)stride_instr_interval, IARG_END);
	}
}

/* finishing... */
VOID fini_stride(INT32 code, VOID* v){

	int i;

	UINT64 cum;

	if(interval_size == -1){
		output_file_stride = fopen("stride_full_int_pin.out","w");
	}
	else{
		output_file_stride = fopen("stride_phases_int_pin.out","a");
	}
	fprintf(output_file_stride,"%lld",(long long)numReadInstrsAnalyzed);
	/* local read distribution */
	cum = 0;
	for(i = 0; i < MAX_DISTR; i++){
		cum += localReadDistrib[i];
		if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) || (i == 262144) ){
			if(cum > 0)
				fprintf(output_file_stride," %lld", (long long) cum);
			else
				fprintf(output_file_stride," %d", 0);
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
				fprintf(output_file_stride," %lld", (long long) cum);
			else	
				fprintf(output_file_stride," %d", 0);
		}
		if(i == 262144)
			break;
	}
	fprintf(output_file_stride," %lld",(long long)numWriteInstrsAnalyzed);
	/* local write distribution */
	cum = 0;
	for(i = 0; i < MAX_DISTR; i++){
		cum += localWriteDistrib[i];
		if( (i == 0) || (i == 8) || (i == 64) || (i == 512) || (i == 4096) || (i == 32768) || (i == 262144) ){
			if(cum > 0)
				fprintf(output_file_stride," %lld", (long long) cum);
			else	
				fprintf(output_file_stride," %d", 0);
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
				fprintf(output_file_stride," %lld", (long long) cum);
			else	
				fprintf(output_file_stride," %d", 0);
		}
		if(i == 262144){
			if(cum > 0)
				fprintf(output_file_stride," %lld\n", (long long) cum);
			else	
				fprintf(output_file_stride," %d\n", 0);
			break;
		}
	}
	fprintf(output_file_stride,"number of instructions: %lld\n", total_ins_count);
	fclose(output_file_stride);
}
