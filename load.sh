#!/bin/bash

export LD_LIBRARY_PATH=/g/g90/patki1/src/variorum_install:$LD_LIBRARY_PATH

#flux exec -r all flux module load ./reduce.so
flux exec -r all flux module load ./reduce_json.so
#flux exec -r all flux module load ./exegetical.so

