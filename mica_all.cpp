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
#include "mica_itypes.h" // needed for itypes_count_* , itypes_instr_interval_output and itypes_instr_interval_reset
#include "mica_ppm.h" // needed for instrument_ppm_cond_br, ppm_instr_interval_output and ppm_instr_interval_reset
#include "mica_reg.h" // needed for reg_instr_full, reg_instr_intervals, reg_instr_interval_output and reg_instr_interval_reset
#include "mica_stride.h" // needed for stride_index_mem*, readMem_stride, writeMem_stride, stride_instr_interval_output and stride_instr_interval_reset
#include "mica_memfootprint.h" // needed for memOp, memfootprint_instr_interval_output and memfootprint_instr_interval_reset
#include "mica_memreusedist.h" // needed for memreusedist_memRead, memreusedist_instr_interval_output and memreusedist_instr_interval_reset

#define PROGRESS_THRESHOLD 10000000 // 10M

extern INT64 total_ins_count;
extern INT64 interval_ins_count;

extern INT64 interval_size;

void init_all(){

	init_ilp_all();
	init_itypes();
	init_ppm();
	init_reg();
	init_stride();
	init_memfootprint();
	init_memreusedist();
}


VOID all_instr_full_count(){
	total_ins_count++;

	if(total_ins_count % PROGRESS_THRESHOLD == 0){
		FILE* f = fopen("mica_progress.txt","w");
		fprintf(f,"%lld*10^9 instructions analyzed\n", total_ins_count/PROGRESS_THRESHOLD);
		fclose(f);
	}
}

VOID all_instr_intervals_count(){
	total_ins_count++;
	interval_ins_count++;

	if(total_ins_count % PROGRESS_THRESHOLD == 0){
		FILE* f = fopen("mica_progress.txt","w");
		fprintf(f,"%lld*10^9 instructions analyzed\n", total_ins_count/PROGRESS_THRESHOLD);
		fclose(f);
	}
}

ADDRINT all_buffer_instruction_2reads_write(void* _e, ADDRINT read1_addr, ADDRINT read2_addr, ADDRINT read_size, UINT32 stride_index_memread1, UINT32 stride_index_memread2, ADDRINT write_addr, ADDRINT write_size, UINT32 stride_index_memwrite){

	itypes_count_mem_read();
	itypes_count_mem_write();
	readMem_stride(stride_index_memread1, read1_addr, read_size);
	readMem_stride(stride_index_memread2, read2_addr, read_size);
	writeMem_stride(stride_index_memwrite, write_addr, write_size);
	memOp(read1_addr, read_size); // memfootprint
	memOp(read2_addr, read_size);
	memOp(write_addr, write_size);
	memreusedist_memRead(read1_addr, read_size); // memreusedist
	memreusedist_memRead(read2_addr, read_size);
	return ilp_buffer_instruction_2reads_write(_e, read1_addr, read2_addr, read_size, write_addr, write_size);
}

ADDRINT all_buffer_instruction_read_write(void* _e, ADDRINT read1_addr, ADDRINT read_size, UINT32 stride_index_memread1, ADDRINT write_addr, ADDRINT write_size, UINT32 stride_index_memwrite){

	itypes_count_mem_read();
	itypes_count_mem_write();
	readMem_stride(stride_index_memread1, read1_addr, read_size);
	writeMem_stride(stride_index_memwrite, write_addr, write_size);
	memOp(read1_addr, read_size); // memfootprint
	memOp(write_addr, write_size);
	memreusedist_memRead(read1_addr, read_size); // memreusedist
	return ilp_buffer_instruction_read_write(_e, read1_addr, read_size, write_addr, write_size);
}

ADDRINT all_buffer_instruction_2reads(void* _e, ADDRINT read1_addr, ADDRINT read2_addr, ADDRINT read_size, UINT32 stride_index_memread1, UINT32 stride_index_memread2){

	itypes_count_mem_read();
	readMem_stride(stride_index_memread1, read1_addr, read_size);
	readMem_stride(stride_index_memread2, read2_addr, read_size);
	memOp(read1_addr, read_size); // memfootprint
	memOp(read2_addr, read_size);
	memreusedist_memRead(read1_addr, read_size); // memreusedist
	memreusedist_memRead(read2_addr, read_size);
	return ilp_buffer_instruction_2reads(_e, read1_addr, read2_addr, read_size);
}

ADDRINT all_buffer_instruction_read(void* _e, ADDRINT read1_addr, ADDRINT read_size, UINT32 stride_index_memread1){

	itypes_count_mem_read();
	readMem_stride(stride_index_memread1, read1_addr, read_size);
	memOp(read1_addr, read_size); // memfootprint
	memreusedist_memRead(read1_addr, read_size); // memreusedist
	return ilp_buffer_instruction_read(_e, read1_addr, read_size);
}

ADDRINT all_buffer_instruction_write(void* _e, ADDRINT write_addr, ADDRINT write_size, UINT32 stride_index_memwrite){

	itypes_count_mem_write();
	writeMem_stride(stride_index_memwrite, write_addr, write_size);
	memOp(write_addr, write_size); // memfootprint
	return ilp_buffer_instruction_write(_e, write_addr, write_size);
}

ADDRINT all_buffer_instruction(void* _e){

	return ilp_buffer_instruction(_e);
}

VOID all_instr_full(VOID* _e, ADDRINT instrAddr, ADDRINT size){
	reg_instr_full(_e);
	instrMem(instrAddr, size);	
}

ADDRINT all_instr_intervals(VOID* _e, ADDRINT instrAddr, ADDRINT size){
	reg_instr_intervals(_e);
	instrMem(instrAddr, size);
	return (ADDRINT)(total_ins_count % interval_size == 0);
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

	memreusedist_instr_interval_output();
	memreusedist_instr_interval_reset();

	interval_ins_count = 0;
}

VOID instrument_all(INS ins, VOID* v, ins_buffer_entry* e){

	UINT32 i, maxNumRegsProd, maxNumRegsCons, regReadCnt, regWriteCnt, opCnt, regOpCnt;
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
			assert((UINT32)reg < MAX_NUM_REGS);
			// only consider valid general-purpose registers (any bit-width) and floating-point registers,
			// i.e. exlude branch, segment and pin registers, among others
			if(REG_valid(reg) && (REG_is_fr(reg) || REG_is_mm(reg) || REG_is_xmm(reg) || REG_is_gr(reg) || REG_is_gr8(reg) || REG_is_gr16(reg) || REG_is_gr32(reg) || REG_is_gr64(reg))){
				regReadCnt++;
			}
		}
		
		e->regReadCnt = regReadCnt;
		if((e->regsRead = (REG*)malloc(regReadCnt*sizeof(REG))) == (REG*)NULL){
			fprintf(stderr,"ERROR: Could not allocate regsRead memory for ins 0x%x\n", e->insAddr);
			exit(1);
		}

		regReadCnt = 0;
		for(i=0; i < maxNumRegsCons; i++){
	
			reg = INS_RegR(ins, i);

			assert((UINT32)reg < MAX_NUM_REGS);
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

			assert((UINT32)reg < MAX_NUM_REGS);
			// only consider valid general-purpose registers (any bit-width) and floating-point registers,
			// i.e. exlude branch, segment and pin registers, among others */
			if(REG_valid(reg) && (REG_is_fr(reg) || REG_is_mm(reg) || REG_is_xmm(reg) || REG_is_gr(reg) || REG_is_gr8(reg) || REG_is_gr16(reg) || REG_is_gr32(reg) || REG_is_gr64(reg))){
				regWriteCnt++;
			}
		}

		e->regWriteCnt = regWriteCnt;
		if((e->regsWritten = (REG*)malloc(regWriteCnt*sizeof(REG))) == (REG*)NULL){
			fprintf(stderr,"ERROR: Could not allocate regsRead memory for ins 0x%x\n", e->insAddr);
			exit(1);
		}	

		regWriteCnt = 0;
		for(i=0; i < maxNumRegsProd; i++){

			reg = INS_RegW(ins, i);

			assert((UINT32)reg < MAX_NUM_REGS);
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
		if(regOpCnt >= MAX_NUM_OPER){
			fprintf(stderr,"BOOM! -> MAX_NUM_OPER is exceeded! (%u)\n", regOpCnt);
			exit(1);
		}
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
	INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)empty_ilp_buffer_all, IARG_END);

	/* +++ ITYPES +++ */
	// control flow instructions
	if(strcmp(cat,"COND_BR") == 0 || strcmp(cat,"UNCOND_BR") == 0 || strcmp(opcode,"LEAVE") == 0 || strcmp(opcode,"RET_NEAR") == 0 || strcmp(opcode,"CALL_NEAR") == 0){
		categorized = true;
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_control,IARG_END);
	}
	else{
		// arithmetic instructions (integer)
		if( strcmp(cat,"LOGICAL") == 0 || strcmp(cat,"DATAXFER") == 0 || strcmp(cat,"BINARY") == 0 || strcmp(cat,"FLAGOP") == 0 || strcmp(cat,"BITBYTE") == 0){
			if(categorized){
				fprintf(stderr, "ERROR: Already categorized! (cat: %s, opcode: %s)\n", cat, opcode);
				exit(1);
			}
			categorized = true;
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_arith,IARG_END);
		} 
		else{   
			// floating point instructions
			if(strcmp(cat,"X87_ALU") == 0 || strcmp(cat,"FCMOV") == 0){
				if(categorized){
					fprintf(stderr, "ERROR: Already categorized! (cat: %s, opcode: %s)\n", cat, opcode);
					exit(1);
				}
				categorized = true;
				INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_fp,IARG_END);
			}    
			else{
				// pop/push instructions (stack usage)
				if( (strcmp(cat,"POP") == 0) || (strcmp(cat,"PUSH") == 0)){
					if(categorized){
						fprintf(stderr, "ERROR: Already categorized! (cat: %s, opcode: %s)\n", cat, opcode);
						exit(1);
					}
					categorized = true;
					INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_stack,IARG_END);
				}
				else{
					// [!] shift instructions (bitwise)
					if(strcmp(cat,"SHIFT") == 0){
						if(categorized){
							fprintf(stderr, "ERROR: Already categorized! (cat: %s, opcode: %s)\n", cat, opcode);
							exit(1);
						}
						categorized = true;
						INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_shift,IARG_END);
					}
					else{    
						// [!] string instructions
						if(strcmp(cat,"STRINGOP") == 0){
							if(categorized){
								fprintf(stderr, "ERROR: Already categorized! (cat: %s, opcode: %s)\n", cat, opcode);
								exit(1);
							}
							categorized = true;
							INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_string,IARG_END);
						}
						else{
							// [!] MMX/SSE instructions
							if(strcmp(cat,"MMX") == 0 || strcmp(cat,"SSE") == 0){
								if(categorized){
									fprintf(stderr, "ERROR: Already categorized! (cat: %s, opcode: %s)\n", cat, opcode);
									exit(1);
								}
								categorized = true;
								INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_sse,IARG_END);
							}    
							else{
								// other (interrupts, rotate instructions, semaphore, conditional move, system)
								if(strcmp(cat,"INTERRUPT") == 0 || strcmp(cat,"ROTATE") == 0 || strcmp(cat,"SEMAPHORE") == 0 || strcmp(cat,"CMOV") == 0 || strcmp(cat,"SYSTEM") == 0 || strcmp(cat,"MISC") == 0 || strcmp(cat,"PREFETCH") == 0 ){
									if(categorized){
										fprintf(stderr, "ERROR: Already categorized! (cat: %s, opcode: %s)\n", cat, opcode);
										exit(1);
									}
									categorized = true;
									INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_other,IARG_END);
								}   
								else{
									// [!] NOP instructions
									if(strcmp(cat,"NOP") == 0){
										if(categorized){
											fprintf(stderr, "ERROR: Already categorized! (cat: %s, opcode: %s)\n", cat, opcode);
											exit(1);
										}
										categorized = true;
										INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_count_nop,IARG_END);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if(!categorized){
		fprintf(stderr,"What the hell ?!? I don't know this one yet! (cat: %s, opcode: %s)\n", cat, opcode);
		exit(1);
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
