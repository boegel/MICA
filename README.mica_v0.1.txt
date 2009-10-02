MICA: Microarchitecture-Independent Characterization of Applications
====================================================================

Kenneth Hoste & Lieven Eeckhout (Ghent University, Belgium)

website: http://www.elis.ugent.be/~kehoste/mica

A set of tutorial slides on MICA, which were presented at IISWC-2007 are
available from the MICA website.

* Compilation
--------------

The easiest way to compile MICA is to add mica-vX.Y.cpp to the ManualExamples
directory of the Pin kit you are using, and adjust the makefile accordingly
(i.e. add mica-vX.Y to the TOOL_ROOTS variable).  That will allow you to just
run 'make mica-vX.Y' to build the tool. Don't forget to replace the X and Y by
the correct numbers, depending on the version of MICA you are using.

* Output files
---------------

(I realize the output file names are a bit strange, but that's just the way I
chose them... It's easy to adjust them, just look in the main() function ;-) ).

ilp: ilp_win_phases32-64-128-256_int_pin.out
itypes: itypes_phases_int_pin.out
ppm: ppm_phases_int_pin.out
reg: reg_phases_int_pin.out
stride: stride_phases_int_pin.out
memfootprint: memfootprint_phases_pin.out

* Full execution metrics
-----------------------------------

+++ ilp +++

Instruction-Level Parallellism (ILP) available for four different instruction
window sizes (32, 64, 128, 256).  
This is measured by assuming perfect caches, perfect branch prediction, etc.  
The only limitations are the instruction window size and the data dependences.

Usage: pin -t mica full ilp -- <benchmark command line>

+++ itypes +++

Instruction mix.

The instruction mix is evaluated by categorizing the executed instructions.
Because the x86 architecture isn't a load-store architecture, we count memory
reads/writes seperately. The following categories are used (in order of
output):

- memory read (instruction which read from memory)
- memory write (instruction which write to memory)
- control flow
- arithmetic
- floating-point
- stack
- shift
- string
- sse
- other

Usage: pin -t mica full itypes -- <benchmarks command line>

+++ ppm +++

Branch predictability. 

The branch predictability of the conditional branches in the program is
evaluated using a Prediction-by-Partial-Match (PPM) predictor, in 4 different
configurations (global/local branch history, shared/seperate prediction
table(s)), using 3 different history length (4,8,12 bits).  Additionally,
average taken and transition count are also being measured.

Usage: pin -t mica full ppm -- <benchmarks command line>

+++ reg +++

Register traffic.

The register traffic is analyzed in different aspects:

- average number of register operands
- average degree of use
- dependency distances (prob. <= D)

Dependency distances are chosen in powers of 2, i.e. 1, 2, 4, 8, 16, 32, 64

Usage: pin -t mica full reg -- <benchmarks command line>

+++ stride +++

Data stream strides.

The distances between subsequent memory accesses are characterised by:

- local load (memory read) strides
- global load (memory read) strides
- local store (memory write) strides
- global store (memory write) strides

Local means per static instruction accesses, global means over all
instructions. The strides are characterized by powers of 8 (prob. <= 0, 8, 64,
512, 4096, 32768, 262144)

Usage: pin -t mica full stride -- <benchmarks command line>

+++ memfootprint +++

Instruction and data memory footprint.

The size of the instruction and data memory footprint is characterized by
counting the number of pages (4KB) and blocks (32 byte) touched.

Usage: pin -t mica memfootprint -- <benchmarks command line>

* Interval metrics
-------------------

Besides characterization total program execution, the tool is also capable of
characterizing interval behavior.  The analysis are identical to the tools
above, but flush the state for each new each interval.

+++ ilp_phases +++

RESET: instruction and cycle counters (per interval), free memory used for
memory address stuff (to avoid huge memory requirements for large workloads)

DON'T TOUCH: instruction window contents; global instruction and cycle counters

+++ itypes_phases +++

RESET: instruction type counters

+++ ppm_phases +++

RESET: misprediction counts, taken/transition counts

DON'T TOUCH: branch history tables

+++ reg_phases +++

RESET: operand counts, register use distribution and register age distribution

DON'T TOUCH: register use counts (i.e. keep track of register use counts across
interval boundaries); register definition addresses

+++ stride_phases +++

RESET: instruction counts (mem.read, mem.write, interval), distribution counts

DON'T TOUCH: last (global/local) read/write memory addresses

+++ memfootprint_phases +++

RESET: reference counters, free memory used for memory address stuff (to avoid
huge memory requirements for large workloads) 

DON'T TOUCH: -

* Measured in integer values, convert to floating-point
-------------------------------------------------------

Because of historical reasons (problems with printing out floating-point
numbers in certain situations with previous Pin kits), we only print out
integer values and convert to floating-point metrics offline. This also allows
aggregating data measured per interval to larger intervals or full execution
for most characteristics.

S: interval size
N: number of intervals
I: number of instructions

+++ ilp_phases +++

FORMAT:

instruction_count<space>cycle_count_win_size_1<space>cycle_count_win_size_2<space>...<space>cycle_count_win_size_n

CONVERSION:

instruction_count/cycle_count

i.e.
1 to (N-1)th line: S/cycle_count_win_size_i
Nth line: (I-N*S)/cycle_count_win_size_i

+++ itypes_phases +++

FORMAT:  

instruction_cnt<space>mem_read_cnt<space>mem_write_cnt<space>control_cnt<space>arith_cnt<space>fp_cnt<space>stack_cnt<space>shift_cnt<space>string_cnt<space>sse_cnt<space>other_cnt

CONVERSION:

mem_write_cnt/instruction_cnt
...
other_cnt/instruction_cnt

+++ ppm_phases +++

FORMAT:

instr_cnt<space>GAg_mispred_cnt_4bits<space>PAg_mispred_cnt_4bits<space>GAs_mispred_cnt_4bits<space>PAs_mispred_cnt_4bits<space>...<space>PAs_mispred_cnt_12bits

CONVERSION:

GAg_mispred_cnt_Kbits/instr_cnt
...
PAs_mispred_cnt_Kbits/instr_cnt

+++ reg_phases +++

FORMAT:

instr_cnt<space>total_oper_cnt<space>instr_reg_cnt<space>total_reg_use_cnt<space>total_reg_age<space>reg_age_cnt_1<space>reg_age_cnt_2<space>reg_age_cnt_4<space>...<space>reg_age_cnt_64

CONVERSION:

total_oper_cnt/instr_cnt
total_reg_use_cnt/instr_reg_cnt 
reg_age_cnt_1/total_reg_age 
reg_age_cnt_2/total_reg_age 
...
reg_age_cnt_64/total_reg_age

+++ stride_phases +++

FORMAT:

mem_read_cnt<space>mem_read_local_stride_0<space>mem_read_local_stride_8<space>...<space>mem_read_local_stride_262144<space>mem_read_global_stride_0<space>...<space>mem_read_global_stride_262144<space>mem_write_cnt<space>mem_write_local_stride_0<space>...<space>mem_write_global_stride_262144

CONVERSION:

mem_read_local_stride_0/mem_read_cnt
...
mem_read_global_stride_262144/mem_read_cnt
mem_write_local_stride_0/mem_write_cnt
...
mem_write_global_stride_262144/mem_write_cnt

+++ memfootprint_phases +++

Integer output (no conversion needed).
