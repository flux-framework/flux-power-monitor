# Flux Power Manager

This is an implementation of power management for a cluster specifically focusing on GPU power capping. We implement two power policies:
- FFT Based Power Management(FPP): We use FFT to find period of apps and then use that as an indicator for power capping.
- Uniform Power Distribution Policy: We equally assign the total power budget to each component.

More details about the work can be found here:
```
https://doi.org/10.1109/SCW63240.2024.00231
```


