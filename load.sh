#!/bin/bash

export LD_LIBRARY_PATH=/g/g90/patki1/src/variorum_install:$LD_LIBRARY_PATH

#flux kvs dir -R .

flux exec -r all flux module load ./flux_pwr_mgr.so
#flux getattr local-uri 
#sleep inf &
#hostname

 
