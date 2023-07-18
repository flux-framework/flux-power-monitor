#!/bin/bash

#Step 3: Load the flux power module 

export LD_LIBRARY_PATH=/g/g92/namankul/variorum/install_tioga_both/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=${CRAY_LD_LIBRARY_PATH}:${LD_LIBRARY_PATH}

#flux kvs dir -R .

#LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/tce/packages/cuda/cuda-10.1.243/lib64
 
flux exec -r all flux module load ../../src/flux_pwr_monitor/.libs/flux_pwr_monitor.so -s 10000 -r 2
#flux getattr local-uri 
#sleep inf &
#hostname

 
