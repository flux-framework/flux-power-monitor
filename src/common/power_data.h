#ifndef FLUX_POWER_DATA_H
#define FLUX_POWER_DATA_H
#include <stdint.h>
#include "device_type.h"
typedef struct {
  double power_value;
  uint64_t device_id;
  device_type type;
} power_data;
void free_power_data_list(power_data **p_data,int num_of_devices);
power_data* power_data_new(device_type type,double power_value,uint64_t device_id);
void power_data_destroy(power_data *data);
#endif
