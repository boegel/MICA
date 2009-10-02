/* 
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework. 
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

#include "mica.h"
#include "mica_ilp.h"
#include "mica_itypes.h"
#include "mica_ppm.h"
#include "mica_reg.h"
#include "mica_stride.h"
#include "mica_memfootprint.h"
#include "mica_memreusedist.h"

enum MODE { MODE_ALL, MODE_ILP, MODE_ILP_ONE, MODE_ITYPES, MODE_PPM, MODE_REG, MODE_STRIDE, MODE_MEMFOOTPRINT, MODE_MEMREUSEDIST, MODE_MYTYPE };

void setup_mica_log(FILE* *log);

void read_config(FILE* log, INT64* interval_size, MODE* mode, UINT32* _win_size);
