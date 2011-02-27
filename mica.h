/* 
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework. 
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

/* standard library includes */
//#include <stdio.h>
#include <iostream>
#include <fstream>
using namespace std;
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Pin includes */
#include "pin.H"

#ifndef MICA

#define MICA
//#define LOG_MSG(str, a...) fprintf(log, str, ##a); fflush(log);
//#define ERROR(str, a...) fprintf(stderr, str, ##a);
//#define DEBUG_MSG(str, a...) fprintf(stderr, str, ##a);

/* *** defines *** */

#define CHAR_CNT 69

/* ILP/MEMFOOTPRINT */

#define ILP_WIN_SIZE_BASE 32

#define MAX_MEM_ENTRIES 65536
#define LOG_MAX_MEM_ENTRIES 16
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

#define BUCKET_CNT 19

#endif
