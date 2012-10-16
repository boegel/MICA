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
#include "mica_memstackdist.h"

enum MODE { UNKNOWN_MODE, MODE_ALL, MODE_ILP, MODE_ILP_ONE, MODE_ITYPES, MODE_PPM, MODE_REG, MODE_STRIDE, MODE_MEMFOOTPRINT, MODE_MEMSTACKDIST, MODE_CUSTOM };

void setup_mica_log(ofstream *log);

void read_config(ofstream *log, INT64* interval_size, MODE* mode, UINT32* _ilp_win_size, UINT32* _block_size, UINT32* _page_size, char** _itypes_spec_file, int* append_pid);
