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
#include "mica_itypes.h"

/* Global variables */

extern INT64 interval_size;
extern INT64 interval_ins_count;
extern INT64 total_ins_count;

FILE* output_file_itypes;

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

/* counter functions */

ADDRINT itypes_instr_intervals(){
	return (ADDRINT)(total_ins_count % interval_size == 0);
};

VOID itypes_instr_interval_output(){
	output_file_itypes = fopen("itypes_phases_int_pin.out","a");
	fprintf(output_file_itypes, "%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n", (long long)interval_size, (long long)mem_read_cnt, (long long)mem_write_cnt, (long long)control_cnt, (long long)arith_cnt, (long long)fp_cnt, (long long)stack_cnt, (long long)shift_cnt, (long long)string_cnt, (long long)sse_cnt, (long long)other_cnt);
	fclose(output_file_itypes);
}

VOID itypes_instr_interval_reset(){
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

VOID itypes_instr_interval(){

	itypes_instr_interval_output();
	itypes_instr_interval_reset();
	interval_ins_count = 0;
}

VOID itypes_count_mem_read() { mem_read_cnt++; }
VOID itypes_count_mem_write() { mem_write_cnt++; }
VOID itypes_count_control() { control_cnt++; }
VOID itypes_count_arith() { arith_cnt++; }
VOID itypes_count_fp() { fp_cnt++; }
VOID itypes_count_stack() { stack_cnt++; }
VOID itypes_count_shift() { shift_cnt++; }
VOID itypes_count_string() { string_cnt++; }
VOID itypes_count_sse() { sse_cnt++; }
VOID itypes_count_other() { other_cnt++; }

/* initializing */
void init_itypes(){

	/* initializing total instruction counts is done in mica.cpp */
	
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

	if(interval_size != -1){		
		output_file_itypes = fopen("itypes_phases_int_pin.out","w");
		fclose(output_file_itypes);
	}
}

/* instrumenting (instruction level) */
VOID instrument_itypes(INS ins, VOID* v){

	char cat[50];
	char opcode[50];
	strcpy(cat,CATEGORY_StringShort(INS_Category(ins)).c_str());
	strcpy(opcode,INS_Mnemonic(ins).c_str());
	BOOL categorized = false;


	// instructions which read from memory
	if( INS_IsMemoryRead(ins) ){
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_mem_read,IARG_END);
	}

	// instructions which write to memory
	if( INS_IsMemoryWrite(ins) ){
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_mem_write,IARG_END);
	}

	// control flow instructions
	if(strcmp(cat,"COND_BR") == 0 || strcmp(cat,"UNCOND_BR") == 0 || strcmp(opcode,"LEAVE") == 0 || strcmp(opcode,"RET_NEAR") == 0 || strcmp(opcode,"CALL_NEAR") == 0){
		categorized = true;
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_control,IARG_END);
	}

	// arithmetic instructions (integer)
	if( strcmp(cat,"LOGICAL") == 0 || strcmp(cat,"DATAXFER") == 0 || strcmp(cat,"BINARY") == 0 || strcmp(cat,"FLAGOP") == 0 || strcmp(cat,"BITBYTE") == 0        ){
		categorized = true;
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_arith,IARG_END);
	}    

	// floating point instructions
	if(strcmp(cat,"X87_ALU") == 0 || strcmp(cat,"FCMOV") == 0){
		categorized = true;
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_fp,IARG_END);
	}    

	// pop/push instructions (stack usage)
	if( (strcmp(cat,"POP") == 0) || (strcmp(cat,"PUSH") == 0)){
		categorized = true;
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_stack,IARG_END);
	}    

	// [!] shift instructions (bitwise)
	if(strcmp(cat,"SHIFT") == 0){
		categorized = true;
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_shift,IARG_END);
	}    

	// [!] string instructions
	if(strcmp(cat,"STRINGOP") == 0){
		categorized = true;
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_string,IARG_END);
	}    

	// [!] MMX/SSE instructions
	if(strcmp(cat,"MMX") == 0 || strcmp(cat,"SSE") == 0){
		categorized = true;
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_sse,IARG_END);
	}    

	// other (interrupts, rotate instructions, semaphore, conditional move, system)
	if(strcmp(cat,"INTERRUPT") == 0 || strcmp(cat,"ROTATE") == 0 || strcmp(cat,"SEMAPHORE") == 0 || strcmp(cat,"CMOV") == 0 || strcmp(cat,"SYSTEM") == 0 || strcmp(cat,"MISC") == 0 || strcmp(cat,"PREFETCH") == 0 ){
		categorized = true;
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_other,IARG_END);
	}   

	if(!categorized){
		fprintf(stderr,"What the hell ?!? I don't know this one yet! (cat: %s, opcode: %s)\n", cat, opcode);
		exit(1);
	} 

	/* inserting calls for counting instructions is done in mica.cpp */	
	if(interval_size != -1){
		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_instr_intervals,IARG_END);
		/* only called if interval is 'full' */
		INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_instr_interval,IARG_END);
	}
}

/* finishing... */
VOID fini_itypes(INT32 code, VOID* v){

	if(interval_size == -1){
		output_file_itypes = fopen("itypes_full_int_pin.out","w");
		fprintf(output_file_itypes, "%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n", (long long)total_ins_count, (long long)mem_read_cnt, (long long)mem_write_cnt, (long long)control_cnt, (long long)arith_cnt, (long long)fp_cnt, (long long)stack_cnt, (long long)shift_cnt, (long long)string_cnt, (long long)sse_cnt, (long long)other_cnt);
	}
	else{
		output_file_itypes = fopen("itypes_phases_int_pin.out","a");
		fprintf(output_file_itypes, "%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n", (long long)interval_ins_count, (long long)mem_read_cnt, (long long)mem_write_cnt, (long long)control_cnt, (long long)arith_cnt, (long long)fp_cnt, (long long)stack_cnt, (long long)shift_cnt, (long long)string_cnt, (long long)sse_cnt, (long long)other_cnt);
	}
	fprintf(output_file_itypes,"number of instructions: %lld\n", total_ins_count);
	fclose(output_file_itypes);
}
