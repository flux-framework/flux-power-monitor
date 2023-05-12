#!/bin/bash

#Step 1: Start by getting an allocation with flux mini alloc, and then launch local flux with this script. 

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${CRAY_LD_LIBRARY_PATH}:/g/g90/patki1/src/variorum_install/lib 
export PATH=/g/g90/patki1/src/flux-framework/tioga-flux-install/bin:$PATH

echo `date` The flux we will be using is: `which flux`
echo `date` starting flux

# remove old flux.uri if it exists.
rm flux.uri

flux mini run -N=4 flux start bash -c 'echo "ssh://$(hostname)$(flux getattr local-uri | sed -e 's!local://!!')">> flux.uri; sleep inf' 

# -- next step: flux proxy $(cat flux.uri)
# -- to analyze KVS: flux kvs dir -R . 
#Login node/single node, 8 ranks.
#flux start -s 1


