#include "response_power_data.h"
#include "retro_queue_buffer.h"
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "util.h"
#include <assert.h>
#include <jansson.h>
double do_agg(retro_queue_buffer_t *buffer, double current_power_value,
              double old_power_value) {
  if (buffer == NULL)
    return -1.0;
  if (retro_queue_buffer_get_current_size(buffer) == 0)
    return current_power_value;
  if (retro_queue_buffer_get_max_size(buffer) ==
      retro_queue_buffer_get_current_size(buffer)) {
    old_power_value -= *(double *)zlist_first(buffer->list);
  }
  double *p_data = malloc(sizeof(double));
  if (p_data == NULL)
    return -1;
  *p_data = current_power_value;
  old_power_value += current_power_value;
  retro_queue_buffer_push(buffer, (void *)p_data);
  old_power_value /= retro_queue_buffer_get_current_size(buffer);
  return old_power_value;
}
double do_average(retro_queue_buffer_t *buffer) {
  double *data = zlist_first(buffer->list);
  double sum = 0.;
  while (data) {
    sum += *data;
    data = zlist_next(buffer->list);
  }
  return sum / buffer->current_size;
}
int parse_json(char *s, response_power_data *data) {
  if (data == NULL)
    return -1;
  double node_power;
  double gpu_power;
  double cpu_power;
  double mem_power;
  double timestamp;
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
  if (gpu_power > 0)
    data->agg_gpu_power += gpu_power;
  if (mem_power > 0)
    data->agg_mem_power += mem_power;
  json_decref(power_obj);
  return 0;
}
response_power_data *get_agg_power_data(retro_queue_buffer_t *buffer,
                                        const char *hostname,
                                        uint64_t start_time,
                                        uint64_t end_time) {

  if (buffer == NULL)
    return NULL;
  // Create a new list to hold the results.
  response_power_data *power_data =
      response_power_data_new(hostname, start_time, end_time);
  power_data->start_time = UINT64_MAX;
  power_data->end_time = 0;
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
  if (start_time > end_time)
    return power_data;

  if ((start_time > earliest || start_time < latest) &&
      (end_time > earliest && end_time < latest)) {
    power_data->data_presence = FULL;
  }

  if ((start_time < earliest || start_time > latest) &&
      (end_time < earliest || end_time > latest)) {
    power_data->data_presence = NONE;
    return power_data;
  }
  if (start_time < earliest || start_time > latest) {
    start_time = earliest;
    power_data->data_presence = PARTIAL;
  }
  if (end_time < earliest || end_time > latest) {
    end_time = latest;
    power_data->data_presence = PARTIAL;
  }
  // Check if end_time is still greater than start_time after adjustment
  if (start_time > end_time) {
    return NULL;
  }

  for (node = zlist_first(buffer->list); node != NULL;
       node = zlist_next(buffer->list)) {
    // If the item's timestamp is within start_time and end_time range, add the
    // item to the results list.
    if (node->timestamp >= start_time && node->timestamp <= end_time) {
      if (node->timestamp < power_data->start_time)
        power_data->start_time = node->timestamp;

      if (node->timestamp > power_data->end_time)
        power_data->end_time = node->timestamp;
      if (parse_json(node->power_info, power_data) < 0) {
        return NULL;
      }
    }
  }
  if (power_data->number_of_samples == 0) {
    return NULL;
  }
  // Iterate over each item in the buffer again.
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

int getNodeList(char *nodeData, char ***hostList, int *size) {
  char *hostname;
  char *ranges;
  printf("%s \n", nodeData);
  // If nodeData doesn't contain '[', it's a single node
  if (strchr(nodeData, '[') == NULL) {
    *hostList = realloc(*hostList, (*size + 1) * sizeof(char *));
    if (*hostList == NULL) {
      fprintf(stderr, "Failed to allocate memory.\n");
      return -1;
    }

    (*hostList)[*size] = malloc(strlen(nodeData) + 1); // +1 for null terminator
    if ((*hostList)[*size] == NULL) {
      fprintf(stderr, "Failed to allocate memory.\n");
      return -1;
    }

    strcpy((*hostList)[*size], nodeData);
    (*size)++;
    return -1;
  }

  // Split the nodeData
  hostname = strtok(nodeData, "[");

  if (hostname == NULL) {
    fprintf(stderr, "Node Data is NULL \n");
    return -1;
  }
  ranges = strtok(NULL, "[");
  if (ranges == NULL) {
    fprintf(stderr, "Failed to split nodeData by '['.\n");
    return -1;
  }

  // Trim the trailing ']' from ranges
  if (ranges[strlen(ranges) - 1] != ']') {
    fprintf(stderr, "Failed to parse range correctly.\n");
    return -1;
  }
  ranges[strlen(ranges) - 1] = 0;

  // Split ranges by comma
  char *range = strtok(ranges, ",");
  while (range != NULL) {
    if (strchr(range, '-') != NULL) {
      int start, end;
      if (sscanf(range, "%d-%d", &start, &end) != 2) {
        fprintf(stderr, "Failed to parse range correctly.\n");
        return -1;
      }

      for (int i = start; i <= end; i++) {
        // Resize the hostList array
        *hostList = realloc(*hostList, (*size + 1) * sizeof(char *));
        if (*hostList == NULL) {
          fprintf(stderr, "Failed to allocate memory.\n");
          return -1;
        }

        // Allocate memory for the new string
        (*hostList)[*size] =
            malloc(strlen(hostname) + 10); // enough for the number
        if ((*hostList)[*size] == NULL) {
          fprintf(stderr, "Failed to allocate memory.\n");
          return -1;
        }

        // Create the string
        sprintf((*hostList)[*size], "%s%d", hostname, i);

        // Increment the size
        (*size)++;
      }
    } else {
      // Resize the hostList array
      *hostList = realloc(*hostList, (*size + 1) * sizeof(char *));
      if (*hostList == NULL) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return -1;
      }

      // Allocate memory for the new string
      (*hostList)[*size] =
          malloc(strlen(hostname) + 10); // enough for the number
      if ((*hostList)[*size] == NULL) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return -1;
      }

      // Create the string
      sprintf((*hostList)[*size], "%s%s", hostname, range);

      // Increment the size
      (*size)++;
    }

    range = strtok(NULL, ",");
  }
  return 0;
}
