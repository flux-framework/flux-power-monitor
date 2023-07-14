#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "power_data.h"
#include <stdlib.h>
void free_power_data_list(power_data **p_data, int num_of_devices) {
  for (int i = 0; i < num_of_devices; i++) {
    power_data_destroy(p_data[i]);
  }
}
power_data *power_data_new(device_type type, double power_value,
                           uint64_t device_id) {
  power_data *p_data = malloc(sizeof(power_data));
  if (p_data == NULL)
    return NULL;
  p_data->power_value = power_value;
  p_data->type = type;
  p_data->device_id = device_id;
  return p_data;
}
void power_data_destroy(power_data *data) { free(data); }
