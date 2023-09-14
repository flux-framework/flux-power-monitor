echo `date` The flux we will be using is: `which flux`
echo `date` starting flux

# remove old flux.uri if it exists.
rm flux.uri

# LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/tce/packages/cuda/cuda-10.1.243/lib64 jsrun --nrs=2 --rs_per_host=1 --tasks_per_rs=1 -c ALL_CPUS -g ALL_GPUS --bind=none --smpiargs="-disable_gpu_hooks" flux start -o,-Sbroker.boot-method=pmix --wrap=valgrind bash -c 'sleep inf' 
#LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/tce/packages/cuda/cuda-10.1.243/lib64 jsrun --nrs=4 --rs_per_host=1 --tasks_per_rs=1 -c ALL_CPUS -g ALL_GPUS --bind=none --smpiargs="-disable_gpu_hooks" flux start --wrap=valgrind -o,-Sbroker.boot-method=pmix,--setattr=log-stderr-level=7 bash -c 'echo "ssh://$(hostname)$(flux getattr local-uri | sed -e 's!local://!!')">> flux.uri; sleep inf' 
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/tce/packages/cuda/cuda-10.1.243/lib64 jsrun --nrs=4 --rs_per_host=1 --tasks_per_rs=1 -c ALL_CPUS -g ALL_GPUS --bind=none --smpiargs="-disable_gpu_hooks" flux start  -o,-Sbroker.boot-method=pmix,--setattr=log-stderr-level=7 bash -c 'echo "ssh://$(hostname)$(flux getattr local-uri | sed -e 's!local://!!')">> flux.uri; sleep inf' 


