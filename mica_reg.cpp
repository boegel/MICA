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
#include "mica_reg.h"

/* Global variables */

extern INT64 interval_size;
extern INT64 interval_ins_count;
extern INT64 interval_ins_count_for_hpc_alignment;
extern INT64 total_ins_count;
extern INT64 total_ins_count_for_hpc_alignment;

ofstream output_file_reg;

UINT64* opCounts; // array which keeps track of number-of-operands-per-instruction stats
BOOL* regRef; // register references
INT64* PCTable; // production addresses of registers
INT64* regUseCnt; // usage counters for each register
INT64* regUseDistr; // distribution of register usage
INT64* regAgeDistr; // distribution of register ages

/* initializing */
void init_reg(){

	int i;

	/* initializing total instruction counts is done in mica.cpp */

	/* allocate memory */
	opCounts = (UINT64*) checked_malloc(MAX_NUM_OPER * sizeof(UINT64));
	regRef = (BOOL*) checked_malloc(MAX_NUM_REGS * sizeof(BOOL));
	PCTable = (INT64*) checked_malloc(MAX_NUM_REGS * sizeof(INT64));
	regUseCnt = (INT64*) checked_malloc(MAX_NUM_REGS * sizeof(INT64));
	regUseDistr = (INT64*) checked_malloc(MAX_REG_USE * sizeof(INT64));
	regAgeDistr = (INT64*) checked_malloc(MAX_COMM_DIST * sizeof(INT64));

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

	if(interval_size != -1){
		output_file_reg.open(mkfilename("reg_phases_int"), ios::out|ios::trunc);
		output_file_reg.close();
	}
}

/* read register operand */
VOID readRegOp_reg(UINT32 regId){

	/* *** REG *** */


	/* register age */
	INT64 age = total_ins_count - PCTable[regId]; // dependency distance
	if(age >= MAX_COMM_DIST){
		age = MAX_COMM_DIST - 1; // trim if needed
	}
	//assert(age >= 0);
	regAgeDistr[age]++;

	/* register usage */
	regUseCnt[regId]++;
	regRef[regId] = 1; // (operand) register was referenced
}

VOID writeRegOp_reg(UINT32 regId){

	/* *** REG *** */
	UINT32 num;

	/* if register was referenced before, adjust use distribution */
	if(regRef[regId]){
		num = regUseCnt[regId];
		if(num >= MAX_REG_USE) // trim if needed
			num = MAX_REG_USE - 1;
		//assert(num >= 0);
		regUseDistr[num]++;
	}

	/* reset register stuff because of new value produced */

	PCTable[regId] = total_ins_count; // last production = now
	regUseCnt[regId] = 0; // new value is never used (yet)
	regRef[regId] = true; // (destination) register was referenced (for tracking use distribution)
}

VOID reg_instr_full(VOID* _e){

	/* counting instructions is done in all_instr_full() */

	ins_buffer_entry* e = (ins_buffer_entry*)_e;

	INT32 i;

	for(i=0; i < e->regReadCnt; i++){
		readRegOp_reg((UINT32)e->regsRead[i]);
	}
	for(i=0; i < e->regWriteCnt; i++){
		writeRegOp_reg((UINT32)e->regsWritten[i]);
	}

	opCounts[e->regOpCnt]++;
}

ADDRINT reg_instr_intervals(VOID* _e) {

	/* counting instructions is done in all_instr_intervals() */

	ins_buffer_entry* e = (ins_buffer_entry*)_e;

	INT32 i;

	for(i=0; i < e->regReadCnt; i++){
		readRegOp_reg((UINT32)e->regsRead[i]);
	}
	for(i=0; i < e->regWriteCnt; i++){
		writeRegOp_reg((UINT32)e->regsWritten[i]);
	}

	opCounts[e->regOpCnt]++;

	return (ADDRINT) (interval_ins_count_for_hpc_alignment == interval_size);
}

VOID reg_instr_interval_output(){
	int i;

	output_file_reg.open(mkfilename("reg_phases_int"), ios::out|ios::app);

	UINT64 totNumOps = 0;
	UINT64 num;

	/* total number of operands */
	for(i = 1; i < MAX_NUM_OPER; i++){
		totNumOps += opCounts[i]*i;
	}
	output_file_reg << interval_size << " " << totNumOps;

	/* average degree of use */
	num = 0;
	for(i = 0; i < MAX_REG_USE; i++){
		num += regUseDistr[i];
	}
	output_file_reg << " " << num;
	num = 0;
	for(i = 0; i < MAX_REG_USE; i++){
		num += i * regUseDistr[i];
	}
	output_file_reg << " " << num;

	/* register dependency distributions */
	num = 0;
	for(i = 0; i < MAX_COMM_DIST; i++){
		num += regAgeDistr[i];
	}
	output_file_reg << " " << num;
	num = 0;
	for(i = 0; i < MAX_COMM_DIST; i++){
		num += regAgeDistr[i];
		if( (i == 1) || (i == 2) || (i == 4) || (i == 8) || (i == 16) || (i == 32) || (i == 64)){
			output_file_reg << " " << num;
		}
	}
	output_file_reg << endl;

	output_file_reg.close();
}

VOID reg_instr_interval_reset(){

	int i;

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

VOID reg_instr_interval() {

	reg_instr_interval_output();
	reg_instr_interval_reset();
	interval_ins_count = 0;
	interval_ins_count_for_hpc_alignment = 0;

}

VOID instrument_reg(INS ins, ins_buffer_entry* e){


	UINT32 i, maxNumRegsProd, maxNumRegsCons, regReadCnt, regWriteCnt, opCnt, regOpCnt;
	REG reg;

	if(!e->setRead){

		maxNumRegsCons = INS_MaxNumRRegs(ins); // maximum number of register consumations (reads)

		regReadCnt = 0;
		for(i = 0; i < maxNumRegsCons; i++){ // finding all register operands which are read
			reg = INS_RegR(ins,i);
			//assert(((UINT32)reg) < MAX_NUM_REGS);
			/* only consider valid general-purpose registers (any bit-width) and floating-point registers,
			 * i.e. exlude branch, segment and pin registers, among others */
			if(REG_valid(reg) && (REG_is_fr(reg) || REG_is_mm(reg) || REG_is_xmm(reg) || REG_is_gr(reg) || REG_is_gr8(reg) || REG_is_gr16(reg) || REG_is_gr32(reg) || REG_is_gr64(reg))){
				regReadCnt++;
			}
		}

		e->regReadCnt = regReadCnt;
		e->regsRead = (REG*) checked_malloc(regReadCnt*sizeof(REG));

		regReadCnt = 0;
		for(i = 0; i < maxNumRegsCons; i++){ // finding all register operands which are read
			reg = INS_RegR(ins,i);
			//assert(((UINT32)reg) < MAX_NUM_REGS);
			/* only consider valid general-purpose registers (any bit-width) and floating-point registers,
			 * i.e. exlude branch, segment and pin registers, among others */
			if(REG_valid(reg) && (REG_is_fr(reg) || REG_is_mm(reg) || REG_is_xmm(reg) || REG_is_gr(reg) || REG_is_gr8(reg) || REG_is_gr16(reg) || REG_is_gr32(reg) || REG_is_gr64(reg))){
				e->regsRead[regReadCnt++] = reg;
			}
		}
		e->setRead = true;
	}
	if(!e->setWritten){

		maxNumRegsProd = INS_MaxNumWRegs(ins);

		regWriteCnt = 0;
		for(i=0; i < maxNumRegsProd; i++){

			reg = INS_RegW(ins, i);
			//assert(((UINT32)reg) < MAX_NUM_REGS);
			/* only consider valid general-purpose registers (any bit-width) and floating-point registers,
			 * i.e. exlude branch, segment and pin registers, among others */
			if(REG_valid(reg) && (REG_is_fr(reg) || REG_is_mm(reg) || REG_is_xmm(reg) || REG_is_gr(reg) || REG_is_gr8(reg) || REG_is_gr16(reg) || REG_is_gr32(reg) || REG_is_gr64(reg))){
				regWriteCnt++;
			}
		}

		e->regWriteCnt = regWriteCnt;
		e->regsWritten = (REG*)checked_malloc(regWriteCnt*sizeof(REG));

		regWriteCnt = 0;
		for(i=0; i < maxNumRegsProd; i++){

			reg = INS_RegW(ins, i);
			//assert(((UINT32)reg) < MAX_NUM_REGS);
			/* only consider valid general-purpose registers (any bit-width) and floating-point registers,
			 * i.e. exlude branch, segment and pin registers, among others */
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
			cerr << "BOOM! -> MAX_NUM_OPER is exceeded! (" << regOpCnt << ")" << endl;
			exit(1);
		}*/
		e->regOpCnt = regOpCnt;
		e->setRegOpCnt = true;
	}

	if(interval_size == -1){
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)reg_instr_full, IARG_PTR, (void*)e, IARG_END);
	}
	else{
		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)reg_instr_intervals, IARG_PTR, (void*)e, IARG_END);
		/* only called if interval is full */
		INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)reg_instr_interval, IARG_END);
	}
}

/* finishing... */
VOID fini_reg(INT32 code, VOID* v){

	if(interval_size == -1){
		output_file_reg.open(mkfilename("reg_full_int"), ios::out|ios::trunc);
		output_file_reg << total_ins_count;
	}
	else{
		output_file_reg.open(mkfilename("reg_phases_int"), ios::out|ios::app);
		output_file_reg << interval_ins_count;
	}

	int i;
	UINT64 totNumOps = 0;
	UINT64 num;
	/* total number of operands */
	for(i = 1; i < MAX_NUM_OPER; i++){
		totNumOps += opCounts[i]*i;
	}
	output_file_reg << " " << totNumOps;

	// ** average degree of use **
	num = 0;
	for(i = 0; i < MAX_REG_USE; i++){
		num += regUseDistr[i];
	}
	output_file_reg << " " << num;
	num = 0;
	for(i = 0; i < MAX_REG_USE; i++){
		num += i * regUseDistr[i];
	}
	output_file_reg << " " << num;

	// ** register dependency distributions **
	num = 0;
	for(i = 0; i < MAX_COMM_DIST; i++){
		num += regAgeDistr[i];
	}
	output_file_reg << " " << num;
	num = 0;
	for(i = 0; i < MAX_COMM_DIST; i++){
		num += regAgeDistr[i];
		if( (i == 1) || (i == 2) || (i == 4) || (i == 8) || (i == 16) || (i == 32) || (i == 64)){
			output_file_reg << " " << num;
		}
	}
	output_file_reg << endl;
	output_file_reg << "number of instructions: " << total_ins_count_for_hpc_alignment << endl;
	output_file_reg.close();
}
