#!/bin/bash

#Step 1: Start by getting an allocation with lalloc, and then launch LBANN with flux with this script. Make sure to source this script, that's easier. 
# In the future, we want to autogenerate this from LBANN's launcher, by creating
# a new launcher for Flux there. 
 
echo `date` The flux we will be using is: `which flux`
echo `date` starting flux

# remove old flux.uri if it exists.
rm flux.uri

export LBANN_USE_CUBLAS_TENSOR_OPS=1 
export LBANN_USE_CUDNN_TENSOR_OPS=1
export MV2_USE_RDMA_CM=0                                                           
export AL_PROGRESS_RANKS_PER_NUMA_NODE=2                                           
export OMP_NUM_THREADS=8                                                           
export IBV_FORK_SAFE=1                                                             
export HCOLL_ENABLE_SHARP=0                                                        
export OMPI_MCA_coll_hcoll_enable=0                                                
export PAMI_MAX_NUM_CACHED_PAGES=0                                                 
export NVSHMEM_MPI_LIB_NAME=libmpi_ibm.so 

PMIX_MCA_gds="^ds12,ds21" LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/tce/packages/cuda/cuda-10.1.243/lib64 jsrun --nrs=4 --rs_per_host=1 --tasks_per_rs=1 -c ALL_CPUS -g ALL_GPUS --bind=none --smpiargs="-disable_gpu_hooks" sh -c "LD_LIBRARY_PATH=$HOME/local/pmix/lib:$LD_LIBRARY_PATH FLUX_PMI_DEBUG=1 flux start -o,-v,-v -o,-Sbroker.boot-method=pmix bash -c 'flux resource list; echo "ssh://$(hostname)$(flux getattr local-uri | sed -e 's!local://!!')">> flux.uri; sleep inf'"



#PMIX_MCA_gds="^ds12,ds21" jsrun -a 1 -c ALL_CPUS -g ALL_GPUS -n 4  --bind=none --smpiargs="-disable_gpu_hooks" flux start bash -c 'echo "ssh://$(hostname)$(flux getattr broker.rundir)/local" >> flux.uri; sleep inf' 


# -- next step: flux proxy $(cat flux.uri)
# -- to analyze KVS: flux kvs dir -R . 
# -- to submit lenet.py test case: flux mini submit -N4 -n4 --output=lenet.out /usr/WS2/patki1/lbann-benson/lbann/build_lbann/install/bin/lbann --prototext=/usr/WS2/patki1/lbann-benson/lbann/applications/vision/20210628_170844_lbann_lenet/experiment.prototext

#Login node/single node, 8 ranks.
#flux start -s 1



