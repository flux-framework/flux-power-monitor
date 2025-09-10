# Flux Power Monitor

Flux Power Monitor is a lightweight telemetry module for reporting job-level power data with Flux. 
It aggregates power data over time for each node in the instance, and when requested, aggregates this data given a Flux `jobId`.


## Dependencies

Flux Power Monitor depends on:

1. **Flux-core**: Must be installed on the system.
2. **Variorum**: For power data collection. Installation steps are available [here](https://variorum.readthedocs.io/en/latest/BuildingVariorum.html).
3. **Flux Python Bindings**: Install with the command:
   
   ```
   pip install flux-python
   ```
The module is built using `Autotools`.

## Building the Module

To build the Flux Power Monitor, follow these steps:
1. `./autogen.sh`
2. Configure the build specifying the Variorum and Flux paths:
```
./configure --with-variorum=<path_to_variorum> --prefix=<path_to_flux>
```
3. `make -j32`

## Usage 
	
To utilize the Flux Power Monitor:

1. Load the module:
```
./scripts/load_power_monitor.sh -s <buffer_size> -r <sampling_rate>
```
* `-s`: Buffer size (amount of power data each node stores).
* `-r`: Sampling rate for data collection.
2. Confirm the module is loaded with:
```
flux module list
```
3. Retrieve power data for a specific job using:
```
python power_query.py -j {jobId}
```
4. To unload the module:
```
./scripts/unload_power_monitor.sh
```

## Example
The example below shows the power data output for the `GEMM` kernel running on two Tioga nodes, as collected by Flux Power Monitor:

```

# 1. Load the module with a buffer size of 2 samples and a 1-second sampling rate
./scripts/load_power_monitor.sh -s 2 -r 1

# 2. Confirm the module is active
flux module list

# 3. Launch the job
flux run -N 2 -n 16 \
  ./raja-perf.exe \
  -k GEMM -v RAJA_HIP --sizefact 100


# 4. After the job completes, query the power data (example job ID 12345)
python power_query.py -j $(flux job last)

```
The resulting power data looks like this:

```
$ python scripts/power_query.py -j $(flux job last)
power data for job f3tqYbGq5
  hostname  node_power  result_start_time   result_end_time data_presence  cpu_power_0  gpu_power_0  gpu_power_1  gpu_power_2  gpu_power_3  gpu_power_4  gpu_power_5  gpu_power_6  gpu_power_7  mem_power_0
0  tioga25  605.311667   1757463787820050  1757463828720022      Complete   139.049762   192.047619          0.0    94.595238          0.0    89.833333          0.0    89.785714          0.0         -1.0
1  tioga26  596.946738   1757463787766259  1757463828770918      Complete   137.208643   185.071429          0.0    96.309524          0.0    90.047619          0.0    88.309524          0.0         -1.0
```
The `result_start_{end}_time` fields show when sampling began and ended (in microseconds since the epoch). Power is reported in Watts. Node power, CPU, GPU and Memory power is reported based on the available platform. If a particular measurement is not supported by the underlying hardware (e.g. memory power in the above example), a value of `-1.0` is reported in that column. 

The `data_presence` field indicates whether all samples were retained. This is because the implementation utilizes a circular buffer. If a job runs longer than the buffer’s capacity, the oldest measurements are overwritten, and this can result in incomplete data for the job during aggregation.

## Architectures and Hardware Supported

Flux Power Monitor utilizes the `variorum_get_power_json` API from Variorum and supports all architectures where this API is supported. Details on architectures supported by Variorum can be found [here](https://github.com/llnl/variorum).

## Auspices

Part of this work was supported by the LLNL-LDRD Program under Project No. 24-SI-005.

## License

SPDX-License-Identifier: LGPL-3.0

LLNL-CODE-764420

