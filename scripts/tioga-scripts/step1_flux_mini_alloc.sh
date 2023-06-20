#!/bin/bash
# Set up to run on Tioga interactively
# We use System Flux here.
echo `date` running flux mini alloc
flux  alloc -N 2 -q=pdebug

