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


/* *** utility functions *** */

#define WRAP(x) #x
#define REWRAP(x) WRAP(x)
#define LOCATION __BASE_FILE__ ":" __FILE__ ":" REWRAP(__LINE__)

#define checked_malloc(size) ({ void *result = malloc (size); if (__builtin_expect (!result, false)) { ERROR_MSG ("Out of memory at " LOCATION "."); exit (1); }; result; })
#define checked_strdup(string) ({ char *result = strdup (string); if (__builtin_expect (!result, false)) { ERROR_MSG ("Out of memory at " LOCATION "."); exit (1); }; result; })
#define checked_realloc(ptr, size) ({ void *result = realloc (ptr, size); if (__builtin_expect (!result, false)) { ERROR_MSG ("Out of memory at " LOCATION "."); exit (1); }; result; })


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
void free_nlist(nlist*& np);

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
