#include "json_utility.h"
json_t *create_device_json(device_type type, int device_id, double powercap) {
  return json_pack("{s:i,s:i,s:f}", "type", type, "id", device_id, "pcap_set",
                   powercap);
}

json_t *get_device_from_array(json_t *device_array, int index) {
  return json_array_get(device_array, index);
}

void append_to_device_info(json_t *device_info, json_t *new_device_info) {
  json_array_append_new(device_info, new_device_info);
}

void append_single_device_to_json(device_capability *device, json_t *devices) {
  if (device->count > 0) {
    json_array_append_new(
        devices, json_pack("{s:i,s:i,s:{s:f,s:f}}", "type", device->type,
                           "count", device->count, "power_capping", "min",
                           device->min_power, "max", device->max_power));
  }
}
void fill_device_capability_from_json(json_t *device_json,
                                      device_capability *device,
                                      device_type type) {
  device->type = type;
  device->count = json_integer_value(json_object_get(device_json, "count"));

  json_t *power_capping = json_object_get(device_json, "power_capping");
  device->min_power = json_real_value(json_object_get(power_capping, "min"));
  device->max_power = json_real_value(json_object_get(power_capping, "max"));
  device->powercap_allowed = (device->min_power != 0 && device->max_power != 0);
}
