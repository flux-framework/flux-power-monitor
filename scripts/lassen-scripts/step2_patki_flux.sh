#!/bin/bash

#Step 1: Start by getting an allocation with lalloc, and then launch flux with this script. 
# module use /g/g92/namankul/local/flux_lassen_install/
#export PATH=$HOME/local/flux_lassen/bin:$PATH

module use /usr/global/tools/flux/blueos_3_ppc64le_ib/modulefiles
module load pmi-shim flux/c0.45.0-s0.25.0
export PATH=/usr/global/tools/flux/blueos_3_ppc64le_ib/flux-c0.45.0-s0.25.0/bin/:$PATH
export LD_LIBRARY_PATH=/usr/global/tools/flux/blueos_3_ppc64le_ib/flux-c0.45.0-s0.25.0/lib:$LD_LIBRARY_PATH

echo `date` The flux we will be using is: `which flux`
echo `date` starting flux

# remove old flux.uri if it exists.
rm flux.uri

LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/tce/packages/cuda/cuda-10.1.243/lib64 jsrun --nrs=2 --rs_per_host=1 --tasks_per_rs=1 -c ALL_CPUS -g ALL_GPUS --bind=none --smpiargs="-disable_gpu_hooks" flux start -o,-Sbroker.boot-method=pmix bash -c 'echo "ssh://$(hostname)$(flux getattr local-uri | sed -e 's!local://!!')">> flux.uri; sleep inf' 

#PMIX_MCA_gds="^ds12,ds21" LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/tce/packages/cuda/cuda-10.1.243/lib64 jsrun --nrs=4 --rs_per_host=1 --tasks_per_rs=1 -c ALL_CPUS -g ALL_GPUS --bind=none --smpiargs="-disable_gpu_hooks" sh -c "LD_LIBRARY_PATH=$HOME/local/pmix/lib:$LD_LIBRARY_PATH FLUX_PMI_DEBUG=1 flux start -o,-v,-v -o,-Sbroker.boot-method=pmix bash -c 'flux resource list;echo "ssh://$(hostname)$(flux getattr local-uri | sed -e 's!local://!!')">> flux.uri; sleep inf'"

# PMIX_MCA_gds="^ds12,ds21" LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/tce/packages/cuda/cuda-10.1.243/lib64 jsrun --nrs=4 --rs_per_host=1 --tasks_per_rs=1 -c ALL_CPUS -g ALL_GPUS --bind=none --smpiargs="-disable_gpu_hooks" sh -c "LD_LIBRARY_PATH=$HOME/local/pmix/lib:$LD_LIBRARY_PATH FLUX_PMI_DEBUG=1 flux start -o,-v,-v -o,-Sbroker.boot-method=pmix bash -c 'flux resource list;echo "ssh://$(hostname)$(flux getattr local-uri | sed -e 's!local://!!')">> flux.uri; sleep inf'"
#PMIX_MCA_gds="^ds12,ds21" jsrun -a 1 -c ALL_CPUS -g ALL_GPUS -n 4  --bind=none --smpiargs="-disable_gpu_hooks" flux start bash -c 'echo "ssh://$(hostname)$(flux getattr broker.rundir)/local" >> flux.uri; sleep inf' 

# -- next step: flux proxy $(cat flux.uri)
# -- to analyze KVS: flux kvs dir -R . 
#Login node/single node, 8 ranks.
#flux start -s 1

