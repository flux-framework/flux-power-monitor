#!/bin/bash

LBANN_USE_CUBLAS_TENSOR_OPS=1 LBANN_USE_CUDNN_TENSOR_OPS=1 MV2_USE_RDMA_CM=0 AL_PROGRESS_RANKS_PER_NUMA_NODE=2 OMP_NUM_THREADS=32 IBV_FORK_SAFE=1 HCOLL_ENABLE_SHARP=0 OMPI_MCA_coll_hcoll_enable=0 PAMI_MAX_NUM_CACHED_PAGES=0 NVSHMEM_MPI_LIB_NAME=libmpi_ibm.so LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/tce/packages/cuda/cuda-10.1.243/lib64 flux mini submit -N8 -n8 -c40 -g4 --output=covid19.out /usr/WS2/patki1/lbann-benson/lbann/build_lbann/install/bin/lbann --vocab=/p/gpfs1/brainusr/datasets/atom/mpro_inhib/enamine_all2018q1_2020q1-2_mpro_inhib_kekulized.vocab --data_filedir=/p/gpfs1/brainusr/datasets/atom/mpro_inhib --data_filename_train=mpro_inhib_kekulized_train_smiles.txt --sequence_length=100 --num_io_threads=11 --no_header=True --delimiter=0 --use_data_store --preload_data_store --ltfb --prototext=/usr/WS2/patki1/lbann-benson/lbann/applications/ATOM/20210830_164257_atom_wae/experiment.prototext

