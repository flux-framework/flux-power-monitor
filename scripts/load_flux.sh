#!/bin/bash
set -x
#Step 1: Start by getting an allocation with lalloc, and then launch flux with this script. 
# module use /g/g92/namankul/local/flux_lassen_install/
#export PATH=$HOME/local/flux_lassen/bin:$PATH
#
# module use /usr/global/tools/flux/blueos_3_ppc64le_ib/modulefiles
# module load pmi-shim flux/c0.45.0-s0.25.0
# export PATH=/usr/global/tools/flux/blueos_3_ppc64le_ib/flux-c0.45.0-s0.25.0/bin/:$PATH
# export LD_LIBRARY_PATH=/usr/global/tools/flux/blueos_3_ppc64le_ib/flux-c0.45.0-s0.25.0/lib:$LD_LIBRARY_PATH

echo `date` The flux we will be using is: `which flux`
echo `date` starting flux

rm flux.uri

jsrun --nrs=$1 --rs_per_host=1 --tasks_per_rs=1 -c ALL_CPUS -g ALL_GPUS --bind=none --smpiargs="-disable_gpu_hooks" flux start  -o,-Sbroker.boot-method=pmix bash -c 'echo "ssh://$(hostname)$(flux getattr local-uri | sed -e 's!local://!!')">> flux.uri; sleep inf' 
#
# jsrun --nrs=$1 --rs_per_host=1 --tasks_per_rs=1 -c ALL_CPUS -g ALL_GPUS --bind=none --smpiargs="-disable_gpu_hooks" flux start  -o,-Sbroker.boot-method=pmix,--setattr=log-stderr-level=7 --wrap=valgrind bash -c 'echo "ssh://$(hostname)$(flux getattr local-uri | sed -e 's!local://!!')">> flux.uri; sleep inf' 

