# Flux Power Monitor

The Flux Power Monitor is a lightweight telemetry module for the Flux resource manager designed to provide job-level power statistics. It aggregates power data over time for each node and its respective hardware components based on the provided `jobId`.

---

## Dependencies

Flux Power Monitor relies on the following software:

1. **Flux-core**: Must be installed on the system.
2. **Variorum**: For power data collection. Installation steps are available [here](https://variorum.readthedocs.io/en/latest/BuildingVariorum.html).
3. **Flux Python Bindings**: Install with the command:
   
   ```
   pip install flux-python
   ```
The module is built using Autotools.

## Building the Module

To build the Flux Power Monitor, follow these steps:
1.`./autogen.sh`
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

### license

SPDX-License-Identifier: LGPL-3.0

LLNL-CODE-764420
