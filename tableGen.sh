#!/bin/bash

# Amir H. Ashouri - 2017 
# (www.eecg.toronto.edu/~aashouri/)
# This script looks for all MICA output files corresponds to a pid and generates a MICA table. The first row is the header and is added as well.
# Tested with MICA v0.40

benchmarks=*  

echo "APPLICATION_NAME DATASET totInstruction ILP32 ILP64 ILP128 ILP256 total_ins_count_for_hpc_alignment totInstruction mem-read mem-write control-flow arithmetic floating-point stack shift string sse other nop  InstrFootprint64 InstrFootprint4k DataFootprint64 DataFootprint4k mem_access memReuseDist0-2 memReuseDist2-4 memReuseDist4-8 memReuseDist8-16 memReuseDist16-32 memReuseDist32-64 memReuseDist64-128 memReuseDist128-256 memReuseDist256-512 memReuseDist512-1k memReuseDist1k-2k memReuseDist2k-4k memReuseDist4k-8k memReuseDist8k-16k memReuseDist16k-32k memReuseDist32k-64k memReuseDist64k-128k memReuseDist128k-256k memReuseDist256k-512k memReuseDist512k-00 GAg_mispred_cnt_4bits PAg_mispred_cnt_4bits GAs_mispred_cnt_4bits PAs_mispred_cnt_4bits GAg_mispred_cnt_8bits PAg_mispred_cnt_8bits GAs_mispred_cnt_8bits PAs_mispred_cnt_8bits GAg_mispred_cnt_12bits PAg_mispred_cnt_12bits GAs_mispred_cnt_12bits PAs_mispred_cnt_12bits total_brCount total_transactionCount total_takenCount total_num_ops instr_reg_cnt total_reg_use_cnt total_reg_age reg_age_cnt_1 reg_age_cnt_2 reg_age_cnt_4 reg_age_cnt_8 reg_age_cnt_16 reg_age_cnt_32 reg_age_cnt_64 mem_read_cnt mem_read_local_stride_0 mem_read_local_stride_8 mem_read_local_stride_64 mem_read_local_stride_512 mem_read_local_stride_4096 mem_read_local_stride_32768 mem_read_local_stride_262144 mem_read_global_stride_0 mem_read_global_stride_8 mem_read_global_stride_64 mem_read_global_stride_512 mem_read_global_stride_4096 mem_read_global_stride_32768 mem_read_global_stride_262144 mem_write_cnt mem_write_local_stride_0 mem_write_local_stride_8 mem_write_local_stride_64 mem_write_local_stride_512 mem_write_local_stride_4096 mem_write_local_stride_32768 mem_write_local_stride_262144 mem_write_global_stride_0 mem_write_global_stride_8 mem_write_global_stride_64 mem_write_global_stride_512 mem_write_global_stride_4096 mem_write_global_stride_32768 mem_write_global_stride_262144" > micaTable.txt

for i in $benchmarks
do
printf "$benchmarks"

if [ -d "$i" ] 
then
 tmp=$PWD
 cd $i
  # *** process directory ***
  echo "**********************************************************"
  echo $i
  j_pid=1
  pidList=$(ls * |grep ilp_full_int_ |sed 's/ilp_full_int_//' |sed 's/_pin.out/ /' | tr -d "\n") 
  for i_pid in $pidList
  do 
	echo -n "\n$i dataset$j_pid " >> ../micaTable.txt
	cat *$i_pid* | tr -d "\n" >> ../micaTable.txt	
	j_pid=$(($j_pid+1))
  done
  echo ""
  echo ""
  # *************************

 cd $tmp
fi

done

