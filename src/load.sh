#!/bin/bash

export LD_LIBRARY_PATH=/g/g90/patki1/src/variorum_install/lib:$LD_LIBRARY_PATH

#flux kvs dir -R .

#LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/tce/packages/cuda/cuda-10.1.243/lib64 
flux exec -r all flux module load ./flux_pwr_mgr.so
#flux getattr local-uri 
#sleep inf &
#hostname

 
