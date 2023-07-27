#ifndef FLUX_POWER_INFO_H
#define FLUX_POWER_INFO_H
#include "circular_buffer.h"
#include "device_type.h"
#include "power_data.h"
#include "power_policies/power_policy.h"
typedef struct {
  double agg_power;
  double max_power;
  double min_power;
  POWER_POLICY_TYPE device_power_policy;
  device_type type;
  uint64_t device_id;
  double current_power;
  double powercap;
  double max_powercap;
  circular_buffer_t *power_history;
} device_power_profile;


device_power_profile *device_power_profile_new(device_type type,
                                               uint64_t device_id,
                                               size_t buffer_size);
void device_power_profile_destroy(device_power_profile *device_data);
int device_power_profile_add_power_data_to_device_history(
    device_power_profile *device_data, power_data *data);
void device_power_profile_set_max_power(device_power_profile *device,
                                        double max_power);
void device_power_profile_set_min_power(device_power_profile *device,
                                        double min_power);
#endif
