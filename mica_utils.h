/* 
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework. 
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

#include "mica.h"

#ifndef MICA_UTILS

#define MICA_UTILS

/* *** struct definitions *** */

/* memory node struct */
typedef struct memNode_type{
	/* ilp */
	INT32 timeAvailable[MAX_MEM_ENTRIES];
	/* memfootprint */
	bool numReferenced [MAX_MEM_BLOCK];
} memNode;

/* linked list struct */
typedef struct nlist_type {
	ADDRINT id;
	memNode* mem;
	struct nlist_type* next;
} nlist;

memNode* lookup(nlist** table, ADDRINT key);
memNode* install(nlist** table, ADDRINT key);

typedef struct ins_buffer_entry_type {
	ADDRINT insAddr;
	BOOL setRead;
	BOOL setWritten;
	BOOL setRegOpCnt;
	INT32 regOpCnt;
	INT32 regReadCnt;
	REG* regsRead;
	INT32 regWriteCnt;
	REG* regsWritten;
	ins_buffer_entry_type* next;
} ins_buffer_entry;

#endif
