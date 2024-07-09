#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "flux_pwr_logging.h"
#include "response_power_data.h"
#include "retro_queue_buffer.h"
#include "util.h"
#include <assert.h>
#include <jansson.h>

int parse_json(response_power_data *p_data, const char *input_str,
              const char *node_hostname) {
  if (!p_data) {
    log_error("Empty object p_data\n");
    goto error;
  }
  const char *json_str_start = strchr(input_str, '{');
  const char *json_str_end = strrchr(input_str, '}');
  if (!json_str_start || !json_str_end) {
    // Error handling if JSON is not found
    log_error("Invalid input string\n");
    goto error;
  }

  size_t json_len = json_str_end - json_str_start + 1;
  char *json_buffer = malloc(json_len + 1);
  strncpy(json_buffer, json_str_start, json_len);
  json_buffer[json_len] = '\0';

  json_error_t error;
  json_t *root = json_loads(json_buffer, 0, &error);
  free(json_buffer);

  if (!root) {
    // Error handling for JSON parsing
    log_error("Error parsing JSON: %s\n", error.text);
    goto error;
  }
  json_t *value;

  value = json_object_get(root, node_hostname);
  if (value == NULL) {
    log_error("Unable to get string object based on hostname");
    goto error;
  }
  json_t *node_power = json_object_get(value, "power_node_watts");
  if (json_is_real(node_power))
    p_data->agg_node_power += json_real_value(node_power);
  json_t *socket_power;
  const char *key;
  int cpu_count = 0, mem_count = 0, gpu_count = 0;
  json_object_foreach(value, key, socket_power) {
    const char *socket_str = "socket";
    if (strncmp(key, socket_str, strlen(socket_str)) == 0) {
      json_t *cpu_power = json_object_get(socket_power, "power_cpu_watts");
      if (json_is_real(cpu_power)) {
        // There is only one CPU
        p_data->agg_cpu_power[cpu_count] += json_real_value(cpu_power);
        cpu_count++;
      } else {
        json_t *cpu_power_val;
        const char *cpu_key;
        json_object_foreach(cpu_power, cpu_key, cpu_power_val) {
          const char *cpu_str = "CPU";
          if (strncmp(cpu_key, cpu_str, strlen(cpu_str)) == 0) {
            if (json_is_real(cpu_power_val)) {
              p_data->agg_cpu_power[cpu_count] = json_real_value(cpu_power_val);
              cpu_count++;
            }
          }
        }
      }
      json_t *mem_power = json_object_get(socket_power, "power_mem_watts");
      if (json_is_real(mem_power)) {
        // There is only one MEM
        p_data->agg_mem_power[mem_count] += json_real_value(mem_power);
        mem_count++;
      } else {
        json_t *mem_power_val;
        const char *mem_key;
        json_object_foreach(mem_power, mem_key, mem_power_val) {
          const char *mem_str = "MEM";
          if (strncmp(mem_key, mem_str, strlen(mem_str)) == 0) {
            if (json_is_real(mem_power_val)) {
              log_message("mem power %lf", json_real_value(mem_power_val));
              p_data->agg_mem_power[mem_count] +=
                  json_real_value(mem_power_val);
              mem_count++;
            }
          }
        }
      }
      json_t *gpu_power = json_object_get(socket_power, "power_gpu_watts");
      if (json_is_real(gpu_power)) {
        // There is only one GPU
        p_data->agg_gpu_power[gpu_count] += json_real_value(gpu_power);
        gpu_count++;
      } else {
        json_t *gpu_power_val;
        const char *gpu_key;
        json_object_foreach(gpu_power, gpu_key, gpu_power_val) {
          const char *gpu_str = "GPU";
          if (strncmp(gpu_key, gpu_str, strlen(gpu_str)) == 0) {
            if (json_is_real(gpu_power_val)) {
              p_data->agg_gpu_power[gpu_count] += json_real_value(gpu_power_val);
              gpu_count++;
            }
          }
        }
      }
    }
  }
  p_data->num_of_mem = mem_count;
  p_data->num_of_gpus = gpu_count;
  p_data->num_of_cpus = cpu_count;
  p_data->number_of_samples+=1;
  json_decref(root);
  return 0;
error:
  log_error("ERROR: in parsing JSON");
  return -1;
}

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
// int parse_json(char *s, response_power_data *data) {
//   if (data == NULL)
//     return -1;
//   double node_power;
//   double gpu_power;
//   double cpu_power;
//   double mem_power;
//   double timestamp;
//   json_t *power_obj = json_object();
//   if (power_obj == NULL)
//     return -1;
//   power_obj = json_loads(s, JSON_DECODE_ANY, NULL);
//   node_power =
//       json_real_value(json_object_get(power_obj, "power_node_watts"));
//   gpu_power =
//       json_real_value(json_object_get(power_obj,
//       "power_gpu_watts_socket_0"));
//   // + json_real_value(json_object_get(power_obj,
//   "power_gpu_watts_socket_1"))
//   // ;
//   cpu_power =
//       json_real_value(json_object_get(power_obj,
//       "power_cpu_watts_socket_0"));
//   // + json_real_value(json_object_get(power_obj,
//   "power_cpu_watts_socket_1"))
//   // ;
//   mem_power =
//       json_real_value(json_object_get(power_obj,
//       "power_mem_watts_socket_0"));
//
//   data->number_of_samples++;
//   if (cpu_power > 0)
//     data->agg_cpu_power += cpu_power;
//   if (node_power > 0)
//     data->agg_node_power += node_power;
//   if (gpu_power > 0)
//     data->agg_gpu_power += gpu_power;
//   if (mem_power > 0)
//     data->agg_mem_power += mem_power;
//   json_decref(power_obj);
//   return 0;
// }
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

  // If startTime or endTime are not within the range of timestamps currently
  // in the buffer, adjust them to represent the earliest and latest
  // timestamps, respectively.
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
    log_error("start time greater then end_time");
    return NULL;
  }

  for (node = zlist_first(buffer->list); node != NULL;
       node = zlist_next(buffer->list)) {
    // If the item's timestamp is within start_time and end_time range, add
    // the item to the results list.
    if (node->timestamp >= start_time && node->timestamp <= end_time) {
      if (node->timestamp < power_data->start_time)
        power_data->start_time = node->timestamp;

      if (node->timestamp > power_data->end_time)
        power_data->end_time = node->timestamp;
      if (parse_json(power_data, node->power_info, hostname) < 0) {
        log_error("Parsing error got NULL");
        return NULL;
      }
    }
  }
  if (power_data->number_of_samples == 0) {
    log_error("NUmber of samples is zero");
    return NULL;
  }
  log_message("number of samples %ld",power_data->number_of_samples);
  for (int i=0;i<power_data->num_of_gpus;i++){
    log_message("GPU power %f",power_data->agg_gpu_power[i]);
    power_data->agg_gpu_power[i]/=power_data->number_of_samples;
  }
  for (int i=0;i<power_data->num_of_mem;i++)
    power_data->agg_mem_power[i]/=power_data->number_of_samples;
  for (int i=0;i<power_data->num_of_cpus;i++)
    power_data->agg_cpu_power[i]/=power_data->number_of_samples;
  power_data->agg_node_power /=power_data->number_of_samples;
  // Iterate over each item in the buffer again.
  // if (power_data->agg_cpu_power > 0)
  //   power_data->agg_cpu_power /= power_data->number_of_samples;
  // if (power_data->agg_node_power > 0)
  //   power_data->agg_node_power /= power_data->number_of_samples;
  // if (power_data->agg_gpu_power > 0)
  //   power_data->agg_gpu_power /= power_data->number_of_samples;
  // if (power_data->agg_mem_power > 0)
  //   power_data->agg_mem_power /= power_data->number_of_samples;
  return power_data;
}

int getNodeList(char *nodeData, char ***hostList, int *size) {
  char *hostname;
  char *ranges;
  printf("This is node data %s \n", nodeData);

  if (strchr(nodeData, '[') == NULL) {
    *hostList = realloc(*hostList, (*size + 1) * sizeof(char *));
    if (*hostList == NULL) {
      log_error("Failed to allocate memory.\n");
      return -1;
    }

    printf("This is node data 1 %s \n", nodeData);
    (*hostList)[*size] = malloc(strlen(nodeData) + 1); // +1 for null terminator
    if ((*hostList)[*size] == NULL) {
      log_error("Failed to allocate memory.\n");
      return -1;
    }
    printf("This is node data 2 %s \n", nodeData);

    strcpy((*hostList)[*size], nodeData);
    (*size)++;
  } else {

    // Split the nodeData
    hostname = strtok(nodeData, "[");

    if (hostname == NULL) {
      log_error("Node Data is NULL \n");
      return -1;
    }
    ranges = strtok(NULL, "[");
    if (ranges == NULL) {
      log_error("Failed to split nodeData by '['.\n");
      return -1;
    }

    // Trim the trailing ']' from ranges
    if (ranges[strlen(ranges) - 1] != ']') {
      log_error("Failed to parse range correctly.\n");
      return -1;
    }

    ranges[strlen(ranges) - 1] = 0;

    // Split ranges by comma
    char *range = strtok(ranges, ",");
    while (range != NULL) {
      if (strchr(range, '-') != NULL) {
        int start, end;
        if (sscanf(range, "%d-%d", &start, &end) != 2) {
          log_error("Failed to parse range correctly.\n");
          return -1;
        }

        for (int i = start; i <= end; i++) {
          // Resize the hostList array
          *hostList = realloc(*hostList, (*size + 1) * sizeof(char *));
          if (*hostList == NULL) {
            log_error("Failed to allocate memory.\n");
            return -1;
          }

          // Allocate memory for the new string
          (*hostList)[*size] =
              malloc(strlen(hostname) + 10); // enough for the number
          if ((*hostList)[*size] == NULL) {
            log_error("Failed to allocate memory.\n");
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
          log_error("Failed to allocate memory.\n");
          return -1;
        }

        // Allocate memory for the new string
        (*hostList)[*size] =
            malloc(strlen(hostname) + 10); // enough for the number
        if ((*hostList)[*size] == NULL) {
          log_error("Failed to allocate memory.\n");
          return -1;
        }

        // Create the string
        sprintf((*hostList)[*size], "%s%s", hostname, range);

        // Increment the size
        (*size)++;
      }

      range = strtok(NULL, ",");
    }
  }
  return 0;
}
