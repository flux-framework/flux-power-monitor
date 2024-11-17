#!/bin/bash

default_buffer_size=100000
default_sampling_rate=2

# Usage function to display help information
usage() {
    echo "Usage: $0 [-s <buffer_size>] [-r <sampling_rate>]"
    echo "Defaults: -s (buffer size) defaults to $default_buffer_size, -r (sampling rate) defaults to $default_sampling_rate"
    echo "-s: Buffer size (amount of power data each node stores)."
    echo "-r: Sampling rate for data collection."
    exit 1
}

# Argument parsing with default values
buffer_size=$default_buffer_size
sampling_rate=$default_sampling_rate

while getopts ":s:r:" opt; do
    case ${opt} in
        s )
            buffer_size=$OPTARG
            ;;
        r )
            sampling_rate=$OPTARG
            ;;
        \? )
            echo "Invalid option: $OPTARG" 1>&2
            usage
            ;;
        : )
            echo "Invalid option: $OPTARG requires an argument" 1>&2
            usage
            ;;
    esac
done

# Inform user about default values being used if applicable
if [ "$buffer_size" == "$default_buffer_size" ]; then
    echo "No buffer size provided, using default value of $default_buffer_size."
fi

if [ "$sampling_rate" == "$default_sampling_rate" ]; then
    echo "No sampling rate provided, using default value of $default_sampling_rate."
fi
 
flux exec -r all flux module load ../src/flux_pwr_monitor/.libs/flux_pwr_monitor.so -s $buffer_size -r $sampling_rate

 
