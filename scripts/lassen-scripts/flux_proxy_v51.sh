#!/bin/bash

# Run bquery and save the output
output=$(bquery)
echo $output
# Parse the output to extract the JOBID
# Parse the output to extract the JOBID
#
jobid=$(echo "$output" | awk 'BEGIN{RS=" "; FS=" "} /SUBMIT_TIME/{getline; print}')

echo $jobid
# Check if jobid is null
if [ -z "$jobid" ]; then
    echo "JOBID is null"
    exit 1
fi
flux proxy lsf:output
