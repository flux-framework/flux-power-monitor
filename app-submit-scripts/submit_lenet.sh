#!/bin/bash

# export MV2_USE_RDMA_CM=0                                                           
# export AL_PROGRESS_RANKS_PER_NUMA_NODE=2                                           
# export OMP_NUM_THREADS=8                                                           
# export IBV_FORK_SAFE=1                                                             
# export HCOLL_ENABLE_SHARP=0                                                        
# export OMPI_MCA_coll_hcoll_enable=0                                                
# export PAMI_MAX_NUM_CACHED_PAGES=0                                                 
# export NVSHMEM_MPI_LIB_NAME=libmpi_ibm.so  

MV2_USE_RDMA_CM=0 AL_PROGRESS_RANKS_PER_NUMA_NODE=2 OMP_NUM_THREADS=8 IBV_FORK_SAFE=1 HCOLL_ENABLE_SHARP=0 OMPI_MCA_coll_hcoll_enable=0 PAMI_MAX_NUM_CACHED_PAGES=0 NVSHMEM_MPI_LIB_NAME=libmpi_ibm.so LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/tce/packages/cuda/cuda-10.1.243/lib64  flux mini submit -N 4 -n 4 -g 4 --output=lenet.out /usr/WS2/patki1/lbann-benson/lbann/build_lbann/install/bin/lbann --prototext=/usr/WS2/patki1/lbann-benson/lbann/applications/vision/20210628_170844_lbann_lenet/experiment.prototext
