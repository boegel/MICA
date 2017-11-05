/*
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework.
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

/* standard library includes */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <iostream>
using namespace std;

/* Pin includes */
#include "pin.H"


#ifndef MICA
#define MICA

/* *** global configurations *** */
extern int append_pid;

/* *** conditional debugging *** */

#define LOG_MSG(x) _log << x << endl;
#define DEBUG_MSG(x) cerr << "DEBUG: " << x << endl;

#define WARNING_MSG(x) cerr << "WARNING: " << x << endl;
#define ERROR_MSG(x) cerr << "ERROR: " << x << endl;


/* *** utility macros *** */

#define BITS_TO_MASK(x) ((1ull << (x)) - 1ull)
#define BITS_TO_COUNT(x) (1ull << (x))


/* *** defines *** */

#define CHAR_CNT 69

/* ILP/MEMFOOTPRINT */

#define ILP_WIN_SIZE_BASE 32

// number of stack entries in single hash table item
#define LOG_MAX_MEM_ENTRIES     16
#define MAX_MEM_ENTRIES         BITS_TO_COUNT(LOG_MAX_MEM_ENTRIES)
#define MASK_MAX_MEM_ENTRIES    BITS_TO_MASK(LOG_MAX_MEM_ENTRIES)

#define LOG_MAX_MEM_BLOCK LOG_MAX_MEM_ENTRIES
#define MAX_MEM_BLOCK MAX_MEM_ENTRIES

#define MAX_MEM_BLOCK_ENTRIES 65536
#define MAX_MEM_TABLE_ENTRIES 12289 // hash table size, should be a prime number (769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 196613, 393241, 786433)

/* PPM */
#define MAX_HIST_LENGTH 12
#define NUM_HIST_LENGTHS 3
const UINT32 history_lengths[NUM_HIST_LENGTHS] = {12,8,4};

/* REG */
#define MAX_NUM_REGS 4096
#define MAX_NUM_OPER 7
#define MAX_DIST 128
#define MAX_COMM_DIST MAX_DIST
#define MAX_REG_USE MAX_DIST

/* STRIDE */
#define MAX_DISTR 524288 // 2^21

/* MEMREUSEDIST */

#define BUCKET_CNT 19 // number of reuse distance buckets to use

const char *mkfilename(const char *name);

#endif
