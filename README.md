MICA: Microarchitecture-Independent Characterization of Applications
====================================================================
version 1.0

[Kenneth Hoste](http://kejo.be/ELIS/) & [Lieven Eeckhout](http://users.elis.ugent.be/~leeckhou/) (Ghent University, Belgium)

Current maintainer: 
[Amir H. Ashouri](http://www.eecg.toronto.edu/~aashouri/) (University of Toronto, Canada)

with contributions by:
- Hamid Fadishei (multi-process support)
- Petr Tuma (code cleanup)
- Maxime Chéramy (cleanup, bug fixes, additional features)

Websites:
(http://boegel.kejo.be/ELIS/MICA) 
(http://www.elis.ugent.be/~kehoste/mica)

A set of tutorial slides on MICA, which were presented at IISWC-2007 are
available from the MICA website.

# Disclaimer
------------

Currently, this software is only tested on Linux/x86 and [Pin.2.10](https://drive.google.com/file/d/0B-AkmAlNRsymNVl1RndzbFVpZEU/view?usp=sharing). We had users reporting the corect installation on [Pin-3.4](https://software.intel.com/en-us/articles/pin-a-binary-instrumentation-tool-downloads) and the details are [here](https://github.com/boegel/MICA/commit/1293082a05e97854e3ccc48490d5b72e765b48bf). Anyone who wants to use it on a different 
platform supported by Pin is free to do so, but should expect problems. We work on adapting MICA on newer Pin versions.

Any problem reports or questions are welcome at kenneth.hoste@ugent.be .

# Compilation
--------------

The easiest way to compile MICA is to add unzip/untar mica_vXYZ.tar.gz to the source/tools
directory of the Pin kit you are using. If you wish to place mica in a different
directory, you'll have to adjust the makefile included accordinly.
Running 'make' should produce the 'mica_v0-X' shared library.

By default, MICA is built using the GCC C++ compiler (g++).
Since Pin kit 39599 (March 2nd 2011), building Pin tools with the Intel compilers is
also supported. To build MICA using the Intel C++ compiler, run "make CXX=icpc".
Make sure /opt/intel/lib is added to the LD_LIBRARY_PATH environment variable to
use MICA built using the Intel compilers.

# Specifying type of analysis
-----------------------------

MICA supports various types of microarchitecture-independent characteristics.
It also allows to measure the characteristics either for the entire execution, or
per interval of N dynamic instructions.

Specifying the parameters is done using the mica.conf configuration file. 
A sample mica.conf file is provided with the distribution, and details
on how to specify the parameters are found below.
```
analysis_type: all | ilp | ilp_one | itypes | ppm | reg | stride | memfootprint | memstackdist | custom
interval_size: full | <size>
[ilp_size: <size>]
[block_size: <2^size>]
[page_size: <2^size>]
[itypes_spec_file: <file>]
```
## example:
```
analysis_type: all
interval_size: 100000000
block_size: 6
page_size: 12
itypes_spec_file: itypes_default.spec
```

specifies to measure all supported characteristics per interval of 100,000,000 instructions,
with block size of 64 (2^6), page size of 4K (2^12), and using the instruction mix categories
described in the file itypes_default.spec

## Usage
-------

Using MICA is very easy; just run:
```
pin -t mica.so -- <program> [<parameter>]
```
The type of analysis is specified in the mica.conf file, and some
logging is written to mica.log.

## Output files
---------------

(I realize the output file names are a bit strange, but that's just the way I
chose them... It's easy to adjust them yourself! ).
```
ilp: 
	full: ilp_full_int_pin.out
	interval: ilp_phases_int_pin.out
ilp_one: 
	full: ilp<size>_full_int_pin.out
	interval: ilp<size>_phases_int_pin.out
itypes: 
	full: itypes_full_int_pin.out
	interval: itypes_phases_int_pin.out
ppm: 
	full: ppm_full_int_pin.out
	interval: ppm_phases_int_pin.out
reg: 
	full: reg_full_int_pin.out
	interval: reg_phases_int_pin.out
stride: 
	full: stride_full_int_pin.out
	interval: stride_phases_int_pin.out
memfootprint: 
	full: memfootprint_full_int_pin.out
	interval: memfootprint_phases_int_pin.out
memstackdist: 
	full: memstackdist_full_int_pin.out
	interval: memstackdist_phases_int_pin.out
```	

## Full execution metrics
-----------------------------------

### +++ ilp +++

Instruction-Level Parallellism (ILP) available for four different instruction
window sizes (32, 64, 128, 256).  
This is measured by assuming perfect caches, perfect branch prediction, etc.  
The only limitations are the instruction window size and the data dependences.
```
analysis_type: ilp
```
Besides measuring these four window sizes at once, MICA also supports
specifying a single window size, which is specified as follows (for 
characterizing the full run using an instruction window of 32 entries):
```
analysis_type: ilp_one
interval_size: full
ilp_size: 32
```
You can tweak the block size used using the block_size configuration parameter.

### +++ itypes +++
```
analysis_type: itypes
```
### +++ Instruction mix +++

The instruction mix is evaluated by categorizing the executed instructions.
Because the x86 architecture isn't a load-store architecture, we count memory
reads/writes seperately. The following categories are used by default (in order 
of output):
```
- memory read (instructions which read from memory)
- memory write (instructions which write to memory)
- control flow
- arithmetic
- floating-point
- stack
- shift
- string
- sse
- other
- nop
```
It is possible to redefine the instruction mix categories, by creating a specification
file and mentioning it in the mica.conf file (itypes_spec_file).

### +++ ppm +++
```
analysis_type: ppm
```
Branch predictability. 

The branch predictability of the conditional branches in the program is
evaluated using a Prediction-by-Partial-Match (PPM) predictor, in 4 different
configurations (global/local branch history, shared/seperate prediction
table(s)), using 3 different history length (4,8,12 bits).  Additionally,
average taken and transition count are also being measured.

### +++ reg +++
```
analysis_type: reg
```
### Register traffic.

The register traffic is analyzed in different aspects:
```
- average number of register operands
- average degree of use
- dependency distances (prob. <= D)

Dependency distances are chosen in powers of 2, i.e. 1, 2, 4, 8, 16, 32, 64
```
### +++ stride +++
```
analysis_type: stride
```
Data stream strides.

The distances between subsequent memory accesses are characterised by:
```
- local load (memory read) strides
- global load (memory read) strides
- local store (memory write) strides
- global store (memory write) strides
```
Local means per static instruction accesses, global means over all
instructions. The strides are characterized by powers of 8 (prob. <= 0, 8, 64,
512, 4096, 32768, 262144)

### +++ memfootprint +++
```
analysis_type: memfootprint
```
Instruction and data memory footprint.

The size of the instruction and data memory footprint is characterized by
counting the number of blocks (64-byte) and pages (4KB) touched. This
is done seperately for data and instruction addresses.

### +++ memstackdist +++
```
analysis_type: memstackdist
```
Memory reuse distances.

This is a highly valuable set of numbers to characterize the cache behavior
of the application of interest. For each memory read, the corresponding
64-byte cache block is determined. For each cache block accessed, the number
of unique cache blocks accessed since the last time it was referenced is 
determined, using a LRU stack. 
The reuse distances for all memory reads are reported in buckets. The first
bucket is used for so called 'cold references'. The subsequent buckets capture reuse 
distances of [2^n, 2^(n+1)[, where n ranges from 0 to 18. The first of these
actually captures [0,2[ (not [1,2[), while the last bucket, [2^18, 2^19[, captures all 
reuse distances larger then or equal to 2^18, so it's in fact [2^18, oo[.
In total, this delivers 20 buckets, and the total number of memory accesses 
(the first number in the output), thus 21 numbers.

For example: the fifth bucket, corresponds to accesses with reuse distance
between 2^3 and 2^4 (or 8 64-byte cache blocks to 16 64-byte cache blocks).

Note: because memory addresses vary over different executions of the same
program, these numbers may vary slightly across multiple runs. Please be aware 
of this when using these metrics for research purposes.

To track the progress of the MICA analysis being run, see the mica_progress.txt tool 
which shows how many dynamic instructions have been analyzed. Disabling this can be
done by removing the -DVERBOSE flag in the Makefile and rebuilding MICA. 

### * Interval metrics
-------------------

Besides characterization total program execution, the tool is also capable of
characterizing interval behavior.  The analysis are identical to the tools
above, but flush the state for each new each interval.

### +++ ilp +++

RESET: instruction and cycle counters (per interval), free memory used for
memory address stuff (to avoid huge memory requirements for large workloads)

DON'T TOUCH: instruction window contents; global instruction and cycle counters

+++ itypes +++

RESET: instruction type counters

+++ ppm +++

RESET: misprediction counts, taken/transition counts

DON'T TOUCH: branch history tables

+++ reg +++

RESET: operand counts, register use distribution and register age distribution

DON'T TOUCH: register use counts (i.e. keep track of register use counts across
interval boundaries); register definition addresses

+++ stride +++

RESET: instruction counts (mem.read, mem.write, interval), distribution counts

DON'T TOUCH: last (global/local) read/write memory addresses

+++ memfootprint +++

RESET: reference counters, free memory used for memory address stuff (to avoid
huge memory requirements for large workloads) 

DON'T TOUCH: -

+++ memstackdist +++

RESET: bucket counts (including cold reference and memory access counts)

DON't TOUCH: LRU stack (keep track of reuse distances over interval boundaries)

* Measured in integer values, convert to floating-point
-------------------------------------------------------

Because of historical reasons (problems with printing out floating-point
numbers in certain situations with previous Pin kits), we only print out
integer values and convert to floating-point metrics offline. This also allows
aggregating data measured per interval to larger intervals or full execution
for most characteristics.
```
S: interval size
N: number of intervals
I: number of instructions
```
+++ ilp +++

FORMAT:

instruction_count<space>cycle_count_win_size_1<space>cycle_count_win_size_2<space>...<space>cycle_count_win_size_n

CONVERSION:

instruction_count/cycle_count
```
i.e.
1 to (N-1)th line: S/cycle_count_win_size_i
Nth line: (I-N*S)/cycle_count_win_size_i
```
+++ itypes +++

FORMAT:  

instruction_cnt<space>mem_read_cnt<space>mem_write_cnt<space>control_cnt<space>arith_cnt<space>fp_cnt<space>stack_cnt<space>shift_cnt<space>string_cnt<space>sse_cnt<space>system_cnt<space>nop_cnt<space>other_cnt

CONVERSION:
```
mem_write_cnt/instruction_cnt
...
other_cnt/instruction_cnt
```
NOTE

Note that simply adding the (n-1) last numbers won't necceseraly yield the first number.
First of all, the memory read and write counts shouldn't be added to the total, because
the x86 architecture is not a load/store architecture (e.g. an instruction can both read
memory and be a floating-point instruction).
Secondly, some instructions may fit in multiple categories, and therefore simply adding the
counts for the various categories will cause instructions to be counted double.

Also note that the (sum of) instruction_cnt value(s) will not match the instruction count 
printed at the last line of the output file ("number of instructions: <int>"). This is because
in the former, each iteration of a REP-prefixed instruction is counted, while in the latter
a REP-prefixed instruction in only counted once.

The other_cnt contains the number of instructions that did not fit in any of the other categories
(excluding mem_read and mem_write). More details on which kind of instructions this includes can
be found in the itypes_other_group_categories.txt output file.

+++ ppm +++

FORMAT:

instr_cnt<space>GAg_mispred_cnt_4bits<space>PAg_mispred_cnt_4bits<space>GAs_mispred_cnt_4bits<space>PAs_mispred_cnt_4bits<space>...<space>PAs_mispred_cnt_12bits

CONVERSION:
```
GAg_mispred_cnt_Kbits/instr_cnt
...
PAs_mispred_cnt_Kbits/instr_cnt
```
+++ reg +++

FORMAT:

instr_cnt<space>total_oper_cnt<space>instr_reg_cnt<space>total_reg_use_cnt<space>total_reg_age<space>reg_age_cnt_1<space>reg_age_cnt_2<space>reg_age_cnt_4<space>...<space>reg_age_cnt_64

CONVERSION:
```
total_oper_cnt/instr_cnt
total_reg_use_cnt/instr_reg_cnt 
reg_age_cnt_1/total_reg_age 
reg_age_cnt_2/total_reg_age 
...
reg_age_cnt_64/total_reg_age
```
+++ stride +++

FORMAT:

mem_read_cnt<space>mem_read_local_stride_0<space>mem_read_local_stride_8<space>...<space>mem_read_local_stride_262144<space>mem_read_global_stride_0<space>...<space>mem_read_global_stride_262144<space>mem_write_cnt<space>mem_write_local_stride_0<space>...<space>mem_write_global_stride_262144

CONVERSION:

mem_read_local_stride_0/mem_read_cnt
...
mem_read_global_stride_262144/mem_read_cnt
mem_write_local_stride_0/mem_write_cnt
...
mem_write_global_stride_262144/mem_write_cnt

+++ memfootprint +++

Integer output (no conversion needed).

FORMAT:

num_64-byte_blocks_data<space>num_4KB_pages_data<space>num_64-byte_blocks_instr<space>num_4KB_pages_instr

+++ memstackdist +++

FORMAT:

mem_access_cnt<space>cold_ref_cnt<space>acc_cnt_0-2<space>acc_cnt_2-2^2<space>acc_cnt_2^2-2^3<space>...<space>acc_cnt_2^17-2^18<space>acc_cnt_over_2^18

CONVERSION:
```
cold_ref_cnt/mem_access_cnt
acc_cnt_0/mem_access_cnt
...
acc_cnt_2^18-2^19/mem_access_cnt
acc_cnt_rest/mem_access_cnt
```
* Multi-process binaries
-----------------------------------

If you want to use MICA on multiprocess binaries which call fork and execv, you should specify this entry in the MICA configuration file:
```
append_pid: yes
```
This will tell MICA to append the current process ID to the report file names so multiple processes do not overwrite each other's output.
Additionally, you should pass "-follow_execv 1" parameter to pin in order to trace multiprocess applications.

------------------------------------------------------------------
# Complete list of Headers - Table Generation
For ease of use, we provide tableGen.sh to automatically look for all mica instrumented output files beloging to a unique Pid. It generates a CSV file having the first row as the headers. Please refer to the headers in the script for the complete set of names.

------------------------------------------------------------------
# Examples of using MICA in the recent literature

You can see an example of using MICA in building prediction models targetted to Compiler optimization problems here at [COBAYN's github page](https://github.com/amirjamez/COBAYN). There is also a provided dataset located at:
```
>>COBAYN/data/ft_MICA_cbench.csv
```
