#!/bin/bash

# Set up to run on Quartz.

#export PATH=/g/g24/rountree/FLUX/install/bin:$PATH
echo `date` The flux we will be using is: `which flux`

echo `date` running salloc
salloc --nodes=8 --time=12 --partition=pdebug 	

