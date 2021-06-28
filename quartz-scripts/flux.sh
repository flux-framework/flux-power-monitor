#!/bin/bash
#export PATH=/g/g24/rountree/FLUX/install/bin:$PATH

#Quartz has my own build of Flux
export PATH=/g/g90/patki1/src/flux-framework/flux-install/bin:$PATH

#Lassen build of Flux


echo `date` The flux we will be using is: `which flux`

echo `date` starting flux
#Quartz
srun --pty --mpi=none --ntasks=4 --nodes=4 -ppdebug flux start	

#Lassen 
#jsrun -n 4 flux start

#Login node/single node, 8 ranks.
#flux start -s 1


