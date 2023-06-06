#!/bin/bash


#Step 2: Identify the nodes in the allocation, eg: 
# jsrun --nrs=4 --rs_per_host=1 --tasks_per_rs=1 hostname
#Then, ssh to the head node in a separate terminal, and run this script

export PATH=/g/g90/patki1/src/flux-framework/tioga-flux-install/bin:$PATH 

echo `date` starting flux-proxy
flux proxy $(cat flux.uri)
