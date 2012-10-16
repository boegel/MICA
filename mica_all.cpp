 /*
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework.
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

/* MICA includes */
#include "mica_all.h"
#include "mica_ilp.h" // needed for empty_all_buffer_all
#include "mica_itypes.h" // needed for itypes_count , itypes_instr_interval_output and itypes_instr_interval_reset
#include "mica_ppm.h" // needed for instrument_ppm_cond_br, ppm_instr_interval_output and ppm_instr_interval_reset
#include "mica_reg.h" // needed for reg_instr_full, reg_instr_intervals, reg_instr_interval_output and reg_instr_interval_reset
#include "mica_stride.h" // needed for stride_index_mem*, readMem_stride, writeMem_stride, stride_instr_interval_output and stride_instr_interval_reset
#include "mica_memfootprint.h" // needed for memOp, memfootprint_instr_interval_output and memfootprint_instr_interval_reset
#include "mica_memstackdist.h" // needed for memstackdist_memRead, memstackdist_instr_interval_output and memstackdist_instr_interval_reset

#define PROGRESS_THRESHOLD 10000000 // 10M

extern INT64 total_ins_count;
extern INT64 total_ins_count_for_hpc_alignment;
extern INT64 interval_ins_count;
extern INT64 interval_ins_count_for_hpc_alignment; // one count for REP prefixed instructions

extern INT64 interval_size;

extern identifier** group_identifiers;
extern INT64* group_ids_cnt;
extern INT64* group_counts;
extern INT64 number_of_groups;

extern INT64 other_ids_cnt;
extern INT64 other_ids_max_cnt;
extern identifier* other_group_identifiers;

void init_all(){

	init_ilp_all();
	init_itypes();
	init_ppm();
	init_reg();
	init_stride();
	init_memfootprint();
	init_memstackdist();
}

ADDRINT returnArg(BOOL arg){

	return arg;
}

VOID all_instr_full_count_always(){

	total_ins_count++;

#ifdef VERBOSE
	if (__builtin_expect (total_ins_count % PROGRESS_THRESHOLD == 0, false)) {
		ofstream progress_file;
		progress_file.open ("mica_progress.txt", ios::out | ios::trunc);
		progress_file << total_ins_count << " instructions analyzed" << endl;
		progress_file.close ();
	}
#endif
}

VOID all_instr_full_count_for_hpc_alignment_no_rep(){
	total_ins_count_for_hpc_alignment++;
}

VOID all_instr_full_count_for_hpc_alignment_with_rep(UINT32 repCnt){
	if(repCnt > 0){
		total_ins_count_for_hpc_alignment++;
	}
}

VOID all_instr_intervals_count_always(){
	total_ins_count++;
	interval_ins_count++;

#ifdef VERBOSE
	if (__builtin_expect(total_ins_count % PROGRESS_THRESHOLD == 0, false)) {
		ofstream progress_file;
		progress_file.open("mica_progress.txt", ios::out | ios::trunc);
		progress_file << total_ins_count << " instructions analyzed" << endl;
		progress_file.close();
	}
#endif
}

VOID all_instr_intervals_count_for_hpc_alignment_no_rep(){
	total_ins_count_for_hpc_alignment++;
	interval_ins_count_for_hpc_alignment++;
}

VOID all_instr_intervals_count_for_hpc_alignment_with_rep(UINT32 repCnt){
	if(repCnt > 0){
		total_ins_count_for_hpc_alignment++;
		interval_ins_count_for_hpc_alignment++;
	}
}

ADDRINT all_buffer_instruction_2reads_write(void* _e, ADDRINT read1_addr, ADDRINT read2_addr, ADDRINT read_size, UINT32 stride_index_memread1, UINT32 stride_index_memread2, ADDRINT write_addr, ADDRINT write_size, UINT32 stride_index_memwrite){

	//itypes_count_mem_read();
	//itypes_count_mem_write();
	readMem_stride(stride_index_memread1, read1_addr, read_size);
	readMem_stride(stride_index_memread2, read2_addr, read_size);
	writeMem_stride(stride_index_memwrite, write_addr, write_size);
	memOp(read1_addr, read_size); // memfootprint
	memOp(read2_addr, read_size);
	memOp(write_addr, write_size);
	memstackdist_memRead(read1_addr, read_size); // memstackdist
	memstackdist_memRead(read2_addr, read_size);
	//return ilp_buffer_instruction_2reads_write(_e, read1_addr, read2_addr, read_size, write_addr, write_size);
	ilp_buffer_instruction_only(_e);
	ilp_buffer_instruction_read(read1_addr, read_size);
	ilp_buffer_instruction_read2(read2_addr);
	ilp_buffer_instruction_write(write_addr, write_size);
	return ilp_buffer_instruction_next();
}

ADDRINT all_buffer_instruction_read_write(void* _e, ADDRINT read1_addr, ADDRINT read_size, UINT32 stride_index_memread1, ADDRINT write_addr, ADDRINT write_size, UINT32 stride_index_memwrite){

	//itypes_count_mem_read();
	//itypes_count_mem_write();
	readMem_stride(stride_index_memread1, read1_addr, read_size);
	writeMem_stride(stride_index_memwrite, write_addr, write_size);
	memOp(read1_addr, read_size); // memfootprint
	memOp(write_addr, write_size);
	memstackdist_memRead(read1_addr, read_size); // memstackdist
	//return ilp_buffer_instruction_read_write(_e, read1_addr, read_size, write_addr, write_size);
	ilp_buffer_instruction_only(_e);
	ilp_buffer_instruction_read(read1_addr, read_size);
	ilp_buffer_instruction_write(write_addr, write_size);
	return ilp_buffer_instruction_next();
}

ADDRINT all_buffer_instruction_2reads(void* _e, ADDRINT read1_addr, ADDRINT read2_addr, ADDRINT read_size, UINT32 stride_index_memread1, UINT32 stride_index_memread2){

	//itypes_count_mem_read();
	readMem_stride(stride_index_memread1, read1_addr, read_size);
	readMem_stride(stride_index_memread2, read2_addr, read_size);
	memOp(read1_addr, read_size); // memfootprint
	memOp(read2_addr, read_size);
	memstackdist_memRead(read1_addr, read_size); // memstackdist
	memstackdist_memRead(read2_addr, read_size);
	//return ilp_buffer_instruction_2reads(_e, read1_addr, read2_addr, read_size);
	ilp_buffer_instruction_only(_e);
	ilp_buffer_instruction_read(read1_addr, read_size);
	ilp_buffer_instruction_read2(read2_addr);
	return ilp_buffer_instruction_next();
}

ADDRINT all_buffer_instruction_read(void* _e, ADDRINT read1_addr, ADDRINT read_size, UINT32 stride_index_memread1){

	//itypes_count_mem_read();
	readMem_stride(stride_index_memread1, read1_addr, read_size);
	memOp(read1_addr, read_size); // memfootprint
	memstackdist_memRead(read1_addr, read_size); // memstackdist
	//return ilp_buffer_instruction_read(_e, read1_addr, read_size);
	ilp_buffer_instruction_only(_e);
	ilp_buffer_instruction_read(read1_addr, read_size);
	return ilp_buffer_instruction_next();
}

ADDRINT all_buffer_instruction_write(void* _e, ADDRINT write_addr, ADDRINT write_size, UINT32 stride_index_memwrite){

	//itypes_count_mem_write();
	writeMem_stride(stride_index_memwrite, write_addr, write_size);
	memOp(write_addr, write_size); // memfootprint
	//return ilp_buffer_instruction_write(_e, write_addr, write_size);
	ilp_buffer_instruction_only(_e);
	ilp_buffer_instruction_write(write_addr, write_size);
	return ilp_buffer_instruction_next();
}

ADDRINT all_buffer_instruction(void* _e){

	//return ilp_buffer_instruction(_e);
	ilp_buffer_instruction_only(_e);
	return ilp_buffer_instruction_next();
}

VOID all_instr_full(VOID* _e, ADDRINT instrAddr, ADDRINT size){
	reg_instr_full(_e);
	instrMem(instrAddr, size);
}

ADDRINT all_instr_intervals(VOID* _e, ADDRINT instrAddr, ADDRINT size){
	reg_instr_intervals(_e);
	instrMem(instrAddr, size);
	return (ADDRINT)(interval_ins_count_for_hpc_alignment == interval_size);
};

VOID all_instr_interval(){

	/* output per interval for ILP is done by ilp-buffering functions */

	itypes_instr_interval_output();
	itypes_instr_interval_reset();

	ppm_instr_interval_output();
	ppm_instr_interval_reset();

	reg_instr_interval_output();
	reg_instr_interval_reset();

	stride_instr_interval_output();
	stride_instr_interval_reset();

	memfootprint_instr_interval_output();
	memfootprint_instr_interval_reset();

	memstackdist_instr_interval_output();
	memstackdist_instr_interval_reset();

	interval_ins_count = 0;
	interval_ins_count_for_hpc_alignment = 0;
}

VOID all_instr_interval_for_ilp(){

	// save these, because empty_ilp_buffer_all resets them
	INT64 interval_ins_count_backup = interval_ins_count;
	INT64 interval_ins_count_for_hpc_alignment_backup = interval_ins_count_for_hpc_alignment;

	empty_ilp_buffer_all();

	// restore
	interval_ins_count = interval_ins_count_backup;
	interval_ins_count_for_hpc_alignment = interval_ins_count_for_hpc_alignment_backup;
}

VOID instrument_all(INS ins, VOID* v, ins_buffer_entry* e){

	UINT32 i, j, maxNumRegsProd, maxNumRegsCons, regReadCnt, regWriteCnt, opCnt, regOpCnt;
	REG reg;
	BOOL categorized = false;
	char cat[50];
	char opcode[50];

	UINT32 stride_index_memread1;
	UINT32 stride_index_memread2;
	UINT32 stride_index_memwrite;

	/* fetch cateogry and opcode for this instruction */
	strcpy(cat,CATEGORY_StringShort(INS_Category(ins)).c_str());
	strcpy(opcode,INS_Mnemonic(ins).c_str());

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

	if(!e->setRegOpCnt){
		regOpCnt = 0;
		opCnt = INS_OperandCount(ins);
		for(i = 0; i < opCnt; i++){
			if(INS_OperandIsReg(ins,i))
				regOpCnt++;
		}
		/*if(regOpCnt >= MAX_NUM_OPER){
			fprintf(stderr,"BOOM! -> MAX_NUM_OPER is exceeded! (%u)\n", regOpCnt);
			exit(1);
		}*/
		e->regOpCnt = regOpCnt;
		e->setRegOpCnt = true;
	}

	// buffer memory operations (and instruction register buffer) with one single InsertCall
	if(INS_IsMemoryRead(ins)){

		stride_index_memread1 = stride_index_memRead1(INS_Address(ins));

		if(INS_IsMemoryWrite(ins)){

			stride_index_memwrite =  stride_index_memWrite(INS_Address(ins));

			if(INS_HasMemoryRead2(ins)){

				stride_index_memread2 = stride_index_memRead2(INS_Address(ins));

				INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)all_buffer_instruction_2reads_write, IARG_PTR, (void*)e, IARG_MEMORYREAD_EA, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_UINT32, stride_index_memread1, IARG_UINT32, stride_index_memread2, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_UINT32, stride_index_memwrite, IARG_END);
			}
			else{
				INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)all_buffer_instruction_read_write, IARG_PTR, (void*)e, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_UINT32, stride_index_memread1, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_UINT32, stride_index_memwrite, IARG_END);

			}
		}
		else{
			if(INS_HasMemoryRead2(ins)){

				stride_index_memread2 = stride_index_memRead2(INS_Address(ins));

				INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)all_buffer_instruction_2reads, IARG_PTR, (void*)e, IARG_MEMORYREAD_EA, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_UINT32 , stride_index_memread1, IARG_UINT32, stride_index_memread2, IARG_END);
			}
			else{

				INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)all_buffer_instruction_read, IARG_PTR, (void*)e, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_UINT32, stride_index_memread1, IARG_END);
			}
		}
	}
	else{
		if(INS_IsMemoryWrite(ins)){

			stride_index_memwrite =  stride_index_memWrite(INS_Address(ins));

			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)all_buffer_instruction_write, IARG_PTR, (void*)e, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_UINT32, stride_index_memwrite, IARG_END);
		}
		else{
			INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)all_buffer_instruction, IARG_PTR, (void*)e, IARG_END);
		}
	}

	/* InsertIfCall returns true if ILP buffer is full */
	//INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)empty_ilp_buffer_all, IARG_END);
	INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_interval_for_ilp, IARG_END); // wrapper for empty_ilp_buffer_all

	/* +++ ITYPES +++ */

	// go over all groups, increase group count if instruction matches that group
	// group counts are increased at most once per instruction executed,
	// even if the instruction matches multiple identifiers in that group
	for(i=0; i < number_of_groups; i++){
		for(j=0; j < group_ids_cnt[i]; j++){
			if(group_identifiers[i][j].type == identifier_type::ID_TYPE_CATEGORY){
				if(strcmp(group_identifiers[i][j].str, cat) == 0){
					INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count, IARG_UINT32, i, IARG_END);
					categorized = true;
					break;
				}
			}
			else{
				if(group_identifiers[i][j].type == identifier_type::ID_TYPE_OPCODE){
					if(strcmp(group_identifiers[i][j].str, opcode) == 0){
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count, IARG_UINT32, i, IARG_END);
						categorized = true;
						break;
					}
				}
				else{
					if(group_identifiers[i][j].type == identifier_type::ID_TYPE_SPECIAL){
						if(strcmp(group_identifiers[i][j].str, "mem_read") == 0 && INS_IsMemoryRead(ins) ){
							INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count, IARG_UINT32, i, IARG_END);
							categorized = true;
							break;
						}
						else{
							if(strcmp(group_identifiers[i][j].str, "mem_write") == 0 && INS_IsMemoryWrite(ins) ){
								INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count, IARG_UINT32, i, IARG_END);
								categorized = true;
								break;
							}
							else{
							}
						}
					}
					else{
						cerr << "ERROR! Unknown identifier type specified (" << group_identifiers[i][j].type << ")" << endl;
					}
				}
			}
		}
	}

	// count instruction that don't fit in any of the specified categories in the last group
	if( !categorized ){
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count, IARG_UINT32, (unsigned int)number_of_groups, IARG_END);

		// check whether this category is already known in the 'other' group
		for(i=0; i < other_ids_cnt; i++){
			if(strcmp(other_group_identifiers[i].str, cat) == 0)
				break;
		}

		// if a new instruction category is found, add it to the set
		if(i == other_ids_cnt){
			other_group_identifiers[other_ids_cnt].type = identifier_type::ID_TYPE_CATEGORY;
			other_group_identifiers[other_ids_cnt].str = checked_strdup(cat);
			other_ids_cnt++;
		}

		// prepare for (possible) next category
		if(other_ids_cnt == other_ids_max_cnt){
			other_ids_max_cnt *= 2;
			other_group_identifiers = (identifier*)checked_realloc(other_group_identifiers, other_ids_max_cnt*sizeof(identifier));
		}
	}

	/* +++ PPM *** */
	if(strcmp(cat,"COND_BR") == 0){
		instrument_ppm_cond_br(ins);
	}
	/* inserting calls for counting instructions is done in mica.cpp */
	if(interval_size != -1){
		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_intervals, IARG_PTR, (void*)e, IARG_ADDRINT, INS_Address(ins), IARG_ADDRINT, (ADDRINT)INS_Size(ins), IARG_END);
		/* only called if interval is 'full' */
		INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_interval, IARG_END);
	}
	else{
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)all_instr_full, IARG_PTR, (void*)e, IARG_ADDRINT, INS_Address(ins), IARG_ADDRINT, (ADDRINT)INS_Size(ins), IARG_END);
	}

}
