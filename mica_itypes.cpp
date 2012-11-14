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
#include "mica_itypes.h"

/* Global variables */

extern INT64 interval_size;
extern INT64 interval_ins_count;
extern INT64 interval_ins_count_for_hpc_alignment;
extern INT64 total_ins_count;
extern INT64 total_ins_count_for_hpc_alignment;
extern char* _itypes_spec_file;

ofstream output_file_itypes;

identifier** group_identifiers;
INT64* group_ids_cnt;
INT64* group_counts;
INT64 number_of_groups;

INT64 other_ids_cnt;
INT64 other_ids_max_cnt;
identifier* other_group_identifiers;

/* counter functions */
ADDRINT itypes_instr_intervals(){
	return (ADDRINT)(interval_ins_count_for_hpc_alignment == interval_size);
};

VOID itypes_instr_interval_output(){
	int i;
	output_file_itypes.open(mkfilename("itypes_phases_int"), ios::out|ios::app);
	output_file_itypes << interval_size;
	for(i=0; i < number_of_groups+1; i++){
		output_file_itypes << " " << group_counts[i];
	}
	output_file_itypes << endl;
	output_file_itypes.close();
}

VOID itypes_instr_interval_reset(){
	int i;
	for(i=0; i < number_of_groups+1; i++){
		group_counts[i] = 0;
	}
}

VOID itypes_instr_interval(){

	itypes_instr_interval_output();
	itypes_instr_interval_reset();
	interval_ins_count = 0;
	interval_ins_count_for_hpc_alignment = 0;
}

VOID itypes_count(UINT32 gid){
	group_counts[gid]++;
};

// initialize default groups
VOID init_itypes_default_groups(){

	number_of_groups = 11;

	group_identifiers = (identifier**)checked_malloc((number_of_groups+1)*sizeof(identifier*));
	group_ids_cnt = (INT64*)checked_malloc((number_of_groups+1)*sizeof(INT64));
	group_counts = (INT64*)checked_malloc((number_of_groups+1)*sizeof(INT64));
	for(int i=0; i < number_of_groups+1; i++){
		group_counts[i] = 0;
	}

	// memory reads
	group_ids_cnt[0] = 1;
	group_identifiers[0] = (identifier*)checked_malloc(group_ids_cnt[0]*sizeof(identifier));
	group_identifiers[0][0].type = identifier_type::ID_TYPE_SPECIAL;
	group_identifiers[0][0].str = checked_strdup("mem_read");

	// memory writes
	group_ids_cnt[1] = 1;
	group_identifiers[1] = (identifier*)checked_malloc(group_ids_cnt[1]*sizeof(identifier));
	group_identifiers[1][0].type = identifier_type::ID_TYPE_SPECIAL;
	group_identifiers[1][0].str = checked_strdup("mem_write");

	// control flow instructions
	group_ids_cnt[2] = 5;
	group_identifiers[2] = (identifier*)checked_malloc(group_ids_cnt[2]*sizeof(identifier));
	group_identifiers[2][0].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[2][0].str = checked_strdup("COND_BR");
	group_identifiers[2][1].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[2][1].str = checked_strdup("UNCOND_BR");
	group_identifiers[2][2].type = identifier_type::ID_TYPE_OPCODE;
	group_identifiers[2][2].str = checked_strdup("LEAVE");
	group_identifiers[2][3].type = identifier_type::ID_TYPE_OPCODE;
	group_identifiers[2][3].str = checked_strdup("RET_NEAR");
	group_identifiers[2][4].type = identifier_type::ID_TYPE_OPCODE;
	group_identifiers[2][4].str = checked_strdup("CALL_NEAR");

	// arithmetic instructions (integer)
	group_ids_cnt[3] = 5;
	group_identifiers[3] = (identifier*)checked_malloc(group_ids_cnt[3]*sizeof(identifier));
	group_identifiers[3][0].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[3][0].str = checked_strdup("LOGICAL");
	group_identifiers[3][1].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[3][1].str = checked_strdup("DATAXFER");
	group_identifiers[3][2].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[3][2].str = checked_strdup("BINARY");
	group_identifiers[3][3].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[3][3].str = checked_strdup("FLAGOP");
	group_identifiers[3][4].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[3][4].str = checked_strdup("BITBYTE");

	// floating point instructions
	group_ids_cnt[4] = 2;
	group_identifiers[4] = (identifier*)checked_malloc(group_ids_cnt[4]*sizeof(identifier));
	group_identifiers[4][0].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[4][0].str = checked_strdup("X87_ALU");
	group_identifiers[4][1].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[4][1].str = checked_strdup("FCMOV");

	// pop/push instructions (stack usage)
	group_ids_cnt[5] = 2;
	group_identifiers[5] = (identifier*)checked_malloc(group_ids_cnt[5]*sizeof(identifier));
	group_identifiers[5][0].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[5][0].str = checked_strdup("POP");
	group_identifiers[5][1].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[5][1].str = checked_strdup("PUSH");

	// [!] shift instructions (bitwise)
	group_ids_cnt[6] = 1;
	group_identifiers[6] = (identifier*)checked_malloc(group_ids_cnt[6]*sizeof(identifier));
	group_identifiers[6][0].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[6][0].str = checked_strdup("SHIFT");

	// [!] string instructions
	group_ids_cnt[7] = 1;
	group_identifiers[7] = (identifier*)checked_malloc(group_ids_cnt[7]*sizeof(identifier));
	group_identifiers[7][0].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[7][0].str = checked_strdup("STRINGOP");

	// [!] MMX/SSE instructions
	group_ids_cnt[8] = 2;
	group_identifiers[8] = (identifier*)checked_malloc(group_ids_cnt[8]*sizeof(identifier));
	group_identifiers[8][0].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[8][0].str = checked_strdup("MMX");
	group_identifiers[8][1].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[8][1].str = checked_strdup("SSE");

	// other (interrupts, rotate instructions, semaphore, conditional move, system)
	group_ids_cnt[9] = 8;
	group_identifiers[9] = (identifier*)checked_malloc(group_ids_cnt[9]*sizeof(identifier));
	group_identifiers[9][0].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[9][0].str = checked_strdup("INTERRUPT");
	group_identifiers[9][1].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[9][1].str = checked_strdup("ROTATE");
	group_identifiers[9][2].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[9][2].str = checked_strdup("SEMAPHORE");
	group_identifiers[9][3].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[9][3].str = checked_strdup("CMOV");
	group_identifiers[9][4].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[9][4].str = checked_strdup("SYSTEM");
	group_identifiers[9][5].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[9][5].str = checked_strdup("MISC");
	group_identifiers[9][6].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[9][6].str = checked_strdup("PREFETCH");
	group_identifiers[9][7].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[9][7].str = checked_strdup("SYSCALL");

	// [!] NOP instructions
	group_ids_cnt[10] = 2;
	group_identifiers[10] = (identifier*)checked_malloc(group_ids_cnt[10]*sizeof(identifier));
	group_identifiers[10][0].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[10][0].str = checked_strdup("WIDENOP");
	group_identifiers[10][1].type = identifier_type::ID_TYPE_CATEGORY;
	group_identifiers[10][1].str = checked_strdup("NOP");
}

/* initializing */
VOID init_itypes(){

	int i, j;
	int gid, sgid;
	char type[100];
	char str[100];
	string line;

	/* try and open instruction groups specification file */
	if(_itypes_spec_file != NULL){
		ifstream f(_itypes_spec_file);
		if(f){
			// count number of groups
			number_of_groups = 0;
			while( getline(f,line)){
				sscanf(line.c_str(), "%d, %d, %[^,], %[^\n]\n", &gid, &sgid, type, str);
				if(gid > number_of_groups)
					number_of_groups++;
			}
			f.close();
			number_of_groups++;
			cerr << "==> found " << number_of_groups << " groups" << endl;

			group_identifiers = (identifier**)checked_malloc((number_of_groups+1)*sizeof(identifier*));
			group_ids_cnt = (INT64*)checked_malloc((number_of_groups+1)*sizeof(INT64));
			group_counts = (INT64*)checked_malloc((number_of_groups+1)*sizeof(INT64));
			for(i=0; i < number_of_groups+1; i++){
				group_counts[i] = 0;
			}

			// count number of subgroups per group
			f.open(_itypes_spec_file);
			i=0;
			while( getline(f,line)){
				sscanf(line.c_str(), "%d, %d, %[^,], %[^\n]\n", &gid, &sgid, type, str);
				if(gid == i){
					group_ids_cnt[i]++;
				}
				else{
					group_identifiers[i] = (identifier*)checked_malloc(group_ids_cnt[i]*sizeof(identifier));
					i++;
					group_ids_cnt[i]++;
				}
			}
			group_identifiers[i] = (identifier*)checked_malloc(group_ids_cnt[i]*sizeof(identifier));
			f.close();

			// save subgroup types and identifiers
			f.open(_itypes_spec_file);
			i=0;
			while( getline(f,line)){
				sscanf(line.c_str(), "%d, %d, %[^,], %[^\n]\n", &gid, &sgid, type, str);
				if(strcmp(type, "CATEGORY") == 0){
					group_identifiers[gid][sgid].type = identifier_type::ID_TYPE_CATEGORY;
				}
				else{
					if(strcmp(type, "OPCODE") == 0){
						group_identifiers[gid][sgid].type = identifier_type::ID_TYPE_OPCODE;
					}
					else{
						if(strcmp(type, "SPECIAL") == 0){
							group_identifiers[gid][sgid].type = identifier_type::ID_TYPE_SPECIAL;
						}
						else{
							cerr << "ERROR! Unknown subgroup type found (\"" << type << "\")." << endl;
							cerr << "   Known subgroup types: {CATEGORY, OPCODE, SPECIAL}." << endl;
							exit(-1);
						}
					}
				}
				group_identifiers[gid][sgid].str = checked_strdup(str);
			}
			f.close();

			// print out groups read
			for(i=0; i < number_of_groups; i++){
				cerr << "   group " << i << " (#: " << group_ids_cnt[i] << "): ";
				for(j=0; j < group_ids_cnt[i]; j++){
					cerr << group_identifiers[i][j].str << " ";
					switch(group_identifiers[i][j].type){
						case identifier_type::ID_TYPE_CATEGORY:
							cerr << "[CAT]; ";
							break;
						case identifier_type::ID_TYPE_OPCODE:
							cerr << "[OPCODE]; ";
							break;
						case identifier_type::ID_TYPE_SPECIAL:
							cerr << "[SPECIAL]; ";
							break;
						default:
							cerr << "ERROR! Unknown subgroup type found for [" << i << "][" << j << "] (\"" << group_identifiers[i][j].type << "\")." << endl;
							cerr << "   Known subgroup types: {CATEGORY, OPCODE, SPECIAL}." << endl;
							exit(-1);
							break;
					}
				}
				cerr << endl;
			}
		}
		else{
			cerr << "ERROR! Failed to open file \"" << _itypes_spec_file << "\" containing instruction groups specification." << endl;
			exit(-1);
		}
	}
	else{
		// if no specification file was found, just use defaults (compatible with MICA v0.23 and older)
		init_itypes_default_groups();
	}

	// allocate space for identifiers of 'other' group
	other_ids_cnt = 0;
	other_ids_max_cnt = 2;
	other_group_identifiers = (identifier*)checked_malloc(other_ids_max_cnt*sizeof(identifier));

	// (initializing total instruction counts is done in mica.cpp)

	if(interval_size != -1){
		output_file_itypes.open(mkfilename("itypes_phases_int"), ios::out|ios::trunc);
		output_file_itypes.close();
	}
}

VOID _instrument_itypes(INS ins, VOID* v) {
	int i,j;
	char cat[50];
	char opcode[50];
	strcpy(cat,CATEGORY_StringShort(INS_Category(ins)).c_str());
	strcpy(opcode,INS_Mnemonic(ins).c_str());
	BOOL categorized = false;

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
						cerr << "ERROR! Unknown identifier type specified (" << group_identifiers[i][j].type << ")." << endl;
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
		if(other_ids_cnt >= other_ids_max_cnt){
			other_ids_max_cnt *= 2;
			other_group_identifiers = (identifier*)checked_realloc(other_group_identifiers, other_ids_max_cnt*sizeof(identifier));
		}
	}


}

/* instrumenting (instruction level) */
VOID instrument_itypes(INS ins, VOID* v){
	_instrument_itypes(ins, v);

	/* inserting calls for counting instructions is done in mica.cpp */
	if(interval_size != -1){
		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_instr_intervals,IARG_END);
		/* only called if interval is 'full' */
		INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)itypes_instr_interval,IARG_END);
	}
}

/* finishing... */
VOID fini_itypes(INT32 code, VOID* v){
	int i;

	if(interval_size == -1){
		output_file_itypes.open(mkfilename("itypes_full_int"), ios::out|ios::trunc);
		output_file_itypes << total_ins_count;
		for(i=0; i < number_of_groups+1; i++){
			output_file_itypes << " " << group_counts[i];
		}
		output_file_itypes << endl;
	}
	else{
		output_file_itypes.open(mkfilename("itypes_phases_int"), ios::out|ios::app);
		output_file_itypes << interval_ins_count;
		for(i=0; i < number_of_groups+1; i++){
			output_file_itypes << " " << group_counts[i];
		}
		output_file_itypes << endl;
	}
	output_file_itypes << "number of instructions: " << total_ins_count_for_hpc_alignment << endl;
	output_file_itypes.close();

	// print instruction categories in 'other' group of instructions
	ofstream output_file_other_group_categories;
	output_file_other_group_categories.open("itypes_other_group_categories.txt", ios::out|ios::trunc);
	for(i=0; i < other_ids_cnt; i++){
		output_file_other_group_categories << other_group_identifiers[i].str << endl;
	}
}
