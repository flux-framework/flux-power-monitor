#ifndef FLUX_POWER_DATA_H
#define FLUX_POWER_DATA_H
#include <stdint.h>
typedef struct {
  double mem_power;
  double node_power;
  double cpu_power;
  double gpu_power;
  uint64_t timestamp;
} power_data;
power_data* power_data_new();
#endif
