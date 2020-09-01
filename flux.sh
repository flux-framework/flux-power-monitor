#!/bin/bash
#export PATH=/g/g24/rountree/FLUX/install/bin:$PATH
export PATH=/g/g90/patki1/src/flux-framework/flux-install/bin:$PATH
echo `date` The flux we will be using is: `which flux`

echo `date` starting flux
srun --pty --mpi=none --ntasks=4 --nodes=4 -ppdebug flux start	
#flux start -s 8


