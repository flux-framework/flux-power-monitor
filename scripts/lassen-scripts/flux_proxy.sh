#!/bin/bash


#Step 2: Identify the nodes in the allocation, eg: 
# jsrun --nrs=4 --rs_per_host=1 --tasks_per_rs=1 hostname
#Then, ssh to the head node in a separate terminal, and run this script

#export PATH=$PATH:$HOME/local/flux_lassen_install/bin
#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$HOME/local/flux_lassen_install/lib

 module use /usr/global/tools/flux/blueos_3_ppc64le_ib/modulefiles
 module load pmi-shim flux/c0.45.0-s0.25.0
export PATH=/usr/global/tools/flux/blueos_3_ppc64le_ib/flux-c0.45.0-s0.25.0/bin/:$PATH
export LD_LIBRARY_PATH=/usr/global/tools/flux/blueos_3_ppc64le_ib/flux-c0.45.0-s0.25.0/lib:$LD_LIBRARY_PATH

echo `date` starting flux-proxy
flux proxy $(cat flux.uri)

