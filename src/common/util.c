#include "response_power_data.h"
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "util.h"
#include <assert.h>
#include <jansson.h>
int parse_json(char *s, response_power_data *data) {
  if (data == NULL)
    return -1;
  double node_power;
  double gpu_power;
  double cpu_power;
  double mem_power;
  json_t *power_obj = json_object();
  if (power_obj == NULL)
    return -1;
  power_obj = json_loads(s, JSON_DECODE_ANY, NULL);
  node_power = json_real_value(json_object_get(power_obj, "power_node_watts"));
  gpu_power =
      json_real_value(json_object_get(power_obj, "power_gpu_watts_socket_0"));
  // + json_real_value(json_object_get(power_obj, "power_gpu_watts_socket_1")) ;
  cpu_power =
      json_real_value(json_object_get(power_obj, "power_cpu_watts_socket_0"));
  // + json_real_value(json_object_get(power_obj, "power_cpu_watts_socket_1")) ;
  mem_power =
      json_real_value(json_object_get(power_obj, "power_mem_watts_socket_0"));
  data->number_of_samples++;
  if (cpu_power > 0)
    data->agg_cpu_power += cpu_power;
  if (node_power > 0)
    data->agg_node_power += node_power;
  if (node_power > 0)
    data->agg_gpu_power += gpu_power;
  if (mem_power > 0)
    data->agg_mem_power += mem_power;
  json_decref(power_obj);
  return 0;
}
response_power_data *get_agg_power_data(circular_buffer_t *buffer,
                                        const char *hostname,
                                        uint64_t start_time, uint64_t end_time,
                                        int sampling_rate) {

  if (buffer == NULL)
    return NULL;
  // Create a new list to hold the results.
  response_power_data *power_data =
      response_power_data_new(hostname, start_time, end_time);
  // Variables to keep track of the earliest and latest timestamps in the
  // buffer.
  uint64_t earliest = UINT64_MAX;
  uint64_t latest = 0;

  // Iterate over each item in the buffer to determine the earliest and latest
  // timestamps.
  node_power_info *node;
  for (node = zlist_first(buffer->list); node != NULL;
       node = zlist_next(buffer->list)) {
    if (node->timestamp < earliest) {
      earliest = node->timestamp;
    }
    if (node->timestamp > latest) {
      latest = node->timestamp;
    }
  }

  // If startTime or endTime are not within the range of timestamps currently in
  // the buffer, adjust them to represent the earliest and latest timestamps,
  // respectively.
  if (start_time < earliest || start_time > latest) {
    start_time = earliest;
    power_data->full_data_present = false;
  }
  if (end_time < earliest || end_time > latest) {
    end_time = latest;
    power_data->full_data_present = false;
  }

  // Iterate over each item in the buffer again.
  for (node = zlist_first(buffer->list); node != NULL;
       node = zlist_next(buffer->list)) {
    // If the item's timestamp is within the sampling_rate of startTime and
    // endTime, add the item to the results list.
    if ((node->timestamp >= start_time &&
         node->timestamp <= start_time + sampling_rate) ||
        (node->timestamp >= end_time - sampling_rate &&
         node->timestamp <= end_time)) {
      if (parse_json(node->power_info, power_data) < 0) {
        return NULL;
      }
    }
  }
  if (power_data->number_of_samples == 0) {
    return NULL;
  }
  if (power_data->agg_cpu_power > 0)
    power_data->agg_cpu_power /= power_data->number_of_samples;
  if (power_data->agg_node_power > 0)
    power_data->agg_node_power /= power_data->number_of_samples;
  if (power_data->agg_gpu_power > 0)
    power_data->agg_gpu_power /= power_data->number_of_samples;
  if (power_data->agg_mem_power > 0)
    power_data->agg_mem_power /= power_data->number_of_samples;
  return power_data;
}
