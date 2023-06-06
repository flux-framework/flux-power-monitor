#!/bin/bash

#Step 2: Identify the nodes in the allocation, eg: 
# jsrun --nrs=4 --rs_per_host=1 --tasks_per_rs=1 hostname
#Then, ssh to the head node in a separate terminal, and run this script

module load cmake/3.18.0
module load gcc/8.3.1
module load spectrum-mpi/rolling-release 
module load python/3.7.2
module load cuda/11.1.1
module load ninja/1.9.0
module use /usr/WS2/patki1/lbann-benson/lbann/build_lbann/install/etc/modulefiles/
module load lbann 
module use /usr/global/tools/flux/blueos_3_ppc64le_ib/modulefiles
module load pmi-shim flux/c0.26.0-s0.16.0

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

echo `date` starting flux-proxy
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/tce/packages/cuda/cuda-10.1.243/lib64 flux proxy $(cat flux.uri)

