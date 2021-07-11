/* 
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework. 
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

#include "mica.h"

#ifndef MICA_ITYPES_H
#define MICA_ITYPES_H

typedef struct identifier_type{
	// type of identifier
	// SPECIAL includes stuff like memory reads/writes
	enum {ID_TYPE_CATEGORY = 1, ID_TYPE_OPCODE, ID_TYPE_SPECIAL} type;
	// string identifier for category/opcode
	char* str;
} identifier;

VOID init_itypes();
VOID init_itypes_default_groups();

/**
 * _instrument_itypes instrument the code without handling the intervals.
 */
VOID _instrument_itypes(INS ins, VOID* v);

/**
 * Call _instrument_itypes and add also handle the intervals.
 */
VOID instrument_itypes(INS ins, VOID* v);
VOID fini_itypes(INT32 code, VOID* v);


VOID itypes_count(UINT32 gid);

VOID itypes_instr_interval_output();
VOID itypes_instr_interval_reset();

#endif
