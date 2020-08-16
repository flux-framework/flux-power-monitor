#!/bin/bash
export PATH=/g/g24/rountree/FLUX/install/bin:$PATH
echo `date` The flux we will be using is: `which flux`

echo `date` starting flux
srun --pty --mpi=none --ntasks=8 --nodes=8 -ppdebug flux start	
#flux start -s 8


