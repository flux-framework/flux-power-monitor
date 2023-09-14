#ifndef FLUX_POWER_JSON_UTILITY_H
#define FLUX_POWER_JSON_UTILITY_H
#include "device_type.h"
#include "node_capabilities.h"
#include <jansson.h>
#include <string.h>
#include <unistd.h>
json_t *create_device_json(device_type type, int device_id, double powercap);
json_t *get_device_from_array(json_t *device_array, int index);
void append_to_device_info(json_t *device_info, json_t *new_device_info);
void fill_device_capability_from_json(json_t *device_json,
                                             device_capability *device,
                                             device_type type) ;

void append_single_device_to_json(device_capability *device,
                                         json_t *devices) ;
#endif
