#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "device_power_info.h"
#include "util.h"

device_power_profile *device_power_profile_new(device_type type,
                                               uint64_t device_id,
                                               size_t buffer_size) {
  device_power_profile *device_data = calloc(1, sizeof(device_power_profile));
  if (device_data == NULL)
    return NULL;
  device_data->type = type;
  device_data->device_id = device_id;
  device_data->powercap_allowed = true;
  device_data->power_history = circular_buffer_new(buffer_size, free);
  if (device_data->power_history == NULL) {
    free(device_data);
    return NULL; // Return NULL after freeing device_data
  }
  return device_data;
}

void device_power_profile_destroy(device_power_profile *device_data) {
  if (device_data == NULL)
    return;
  if (device_data->power_history != NULL)
    circular_buffer_destroy(device_data->power_history);
}
int device_power_profile_add_power_data_to_device_history(
    device_power_profile *device_data, power_data *data) {
  double agg = do_agg(device_data->power_history, data->power_value,
                      device_data->agg_power);
  if (agg < 0)
    return -1;
  device_data->agg_power = agg;
  return 0;
}
void device_power_profile_set_max_power(device_power_profile *device,
                                        double max_power) {
  device->max_power = max_power;
}
void device_power_profile_set_min_power(device_power_profile *device,
                                        double min_power) {
  device->min_power = min_power;
}
