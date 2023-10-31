#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "jansson.h"
#include "node_util.h"
node_power *parse_string(const char *input_str) {
  node_power *np = malloc(sizeof(node_power));
  if (!np) {
    printf("Memory allocation failed\n");
    return NULL;
  }
  memset(np, 0, sizeof(node_power));
  const char *json_str_start = strchr(input_str, '{');
  const char *json_str_end = strrchr(input_str, '}');
  if (!json_str_start || !json_str_end) {
    // Error handling if JSON is not found
    printf("Invalid input string\n");
    return np;
  }

  // Copy the JSON substring to a new buffer
  size_t json_len = json_str_end - json_str_start + 1;
  char *json_buffer = malloc(json_len + 1);
  strncpy(json_buffer, json_str_start, json_len);
  json_buffer[json_len] = '\0';

  // Parse JSON
  json_error_t error;
  json_t *root = json_loads(json_buffer, 0, &error);
  free(json_buffer);

  if (!root) {
    // Error handling for JSON parsing
    printf("Error parsing JSON: %s\n", error.text);
    return np;
  }

  // Extract values from JSON
  json_t *value;
  value = json_object_get(root, "power_node_watts");
  if (json_is_real(value))
    np->node_power = json_real_value(value);

  value = json_object_get(root, "power_cpu_watts_socket_0");
  if (json_is_real(value))
    np->cpu_power[0] = json_real_value(value);

  value = json_object_get(root, "power_mem_watts_socket_0");
  if (json_is_real(value))
    np->mem_power[0] = json_real_value(value);

  value = json_object_get(root, "power_cpu_watts_socket_1");
  if (json_is_real(value))
    np->cpu_power[1] = json_real_value(value);

  double gpu_power_value = 0.0;
  value = json_object_get(root, "power_gpu_watts_socket_0");
  if (json_is_real(value))
    gpu_power_value = json_real_value(value);

  value = json_object_get(root, "power_mem_watts_socket_1");
  if (json_is_real(value))
    np->mem_power[1] = json_real_value(value);

  value = json_object_get(root, "power_gpu_watts_socket_1");
  if (json_is_real(value))
    np->gpu_power[1] = json_real_value(value);
  for (int i = 2; i < 8; i++) {
    np->gpu_power[i] = (i < 4) ? gpu_power_value : 0.0;
  }
  np->num_of_gpu = 4;

  // Generate CSV string
  struct timeval tv;
  uint64_t timestamp;
  gettimeofday(&tv, NULL);
  timestamp = tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
  snprintf(np->csv_string, MAX_CSV_SIZE - 1,
           "%ld,%lf,%lf,%lf,%lf,%lf,%lf,%lf\n", timestamp, np->node_power,
           np->cpu_power[0], np->mem_power[0], np->gpu_power[0],
           np->cpu_power[1], np->mem_power[1], np->gpu_power[1]);

  // Clean up
  json_decref(root);

  return np;
}

// Function to allocate the global temporary buffer based on the circular
// buffer's size
int allocate_global_buffer(char *buffer, size_t *buffer_size,
                            size_t circular_buffer_size) {
  *buffer_size = circular_buffer_size * MAX_CSV_SIZE;
  buffer = malloc(*buffer_size);
  if (!buffer) {
    perror("Failed to allocate global buffer");
    *buffer_size = 0;
    return -1;
  }
  memset(buffer, 0, *buffer_size);
  return 0;
}
