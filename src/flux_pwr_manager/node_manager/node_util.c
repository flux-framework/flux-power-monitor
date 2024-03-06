#if HAVE_CONFIG_H
#include "config.h"
#endif
#define _POSIX_C_SOURCE 200809L
#include "node_util.h"
#include <jansson.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define HOSTNAME_SIZE 256

node_power *parse_string(const char *input_str) {

  node_power *np = malloc(sizeof(node_power));
  char node_hostname[HOSTNAME_SIZE];
  if (!np) {
    printf("Memory allocation failed\n");
    goto error;
  }
  memset(np, 0, sizeof(node_power));
  const char *json_str_start = strchr(input_str, '{');
  const char *json_str_end = strrchr(input_str, '}');
  if (!json_str_start || !json_str_end) {
    // Error handling if JSON is not found
    printf("Invalid input string\n");
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
    printf("Error parsing JSON: %s\n", error.text);
    goto error;
  }
  json_t *value;

  gethostname(node_hostname, HOSTNAME_SIZE);

  value = json_object_get(root, node_hostname);
  if (value == NULL) {
    printf("Unable to get string object based on hostname");
    goto error;
  }
  json_t *node_power = json_object_get(value, "power_node_watts");
  if (json_is_real(node_power))
    np->node_power = json_real_value(node_power);
  json_t *socket_power;
  const char *key;
  int cpu_count = 0, mem_count = 0, gpu_count = 0;
  json_object_foreach(value, key, socket_power) {
    const char *socket_str = "socket";
    if (strncmp(key, socket_str, strlen(socket_str)) == 0) {
      json_t *cpu_power = json_object_get(socket_power, "power_cpu_watts");
      if (json_is_real(cpu_power)) {
        // There is only one CPU
        np->cpu_power[cpu_count] = json_real_value(cpu_power);
        cpu_count++;
      } else {
        json_t *cpu_power_val;
        const char *cpu_key;
        json_object_foreach(cpu_power, cpu_key, cpu_power_val) {
          const char *cpu_str = "CPU";
          if (strncmp(cpu_key, cpu_str, strlen(cpu_str)) == 0) {
            if (json_is_real(cpu_power_val)) {
              np->cpu_power[cpu_count] = json_real_value(cpu_power_val);
              cpu_count++;
            }
          }
        }
      }
      json_t *mem_power = json_object_get(socket_power, "power_mem_watts");
      if (json_is_real(mem_power)) {
        // There is only one MEM
        np->mem_power[mem_count] = json_real_value(cpu_power);
        mem_count++;
      } else {
        json_t *mem_power_val;
        const char *mem_key;
        json_object_foreach(mem_power, mem_key, mem_power_val) {
          const char *mem_str = "MEM";
          if (strncmp(mem_key, mem_str, strlen(mem_str)) == 0) {
            if (json_is_real(mem_power_val)) {
              np->cpu_power[mem_count] = json_real_value(mem_power_val);
              mem_count++;
            }
          }
        }
      }
      json_t *gpu_power = json_object_get(socket_power, "power_gpu_watts");
      if (json_is_real(gpu_power)) {
        // There is only one GPU
        np->gpu_power[gpu_count] = json_real_value(gpu_power);
        gpu_count++;
      } else {
        json_t *gpu_power_val;
        const char *gpu_key;
        json_object_foreach(gpu_power, gpu_key, gpu_power_val) {
          const char *gpu_str = "GPU";
          if (strncmp(gpu_key, gpu_str, strlen(gpu_str)) == 0) {
            if (json_is_real(gpu_power_val)) {
              np->gpu_power[gpu_count] = json_real_value(gpu_power_val);
              gpu_count++;
            }
          }
        }
      }
    }
  }
  struct timeval tv;
  uint64_t timestamp;
  gettimeofday(&tv, NULL);
  timestamp = (tv.tv_sec * (uint64_t)1000) + (tv.tv_usec / 1000);
  np->timestamp = timestamp;
  snprintf(np->csv_string, MAX_CSV_SIZE - 1,
           "%ld,%lf,%lf,%lf,%lf,%lf,%lf,%lf\n", timestamp, np->node_power,
           np->cpu_power[0], np->gpu_power[0], np->mem_power[0],
           np->cpu_power[1], np->gpu_power[1], np->mem_power[1]);
  np->num_of_gpu = gpu_count;
  json_decref(root);
  return np;
error:
  printf("ERROR: in parsing JSON");
  if (np)
    free(np);
  np = NULL;
  return NULL;
}
// This is for an older fromat of JSON
node_power *parse_string_v0(const char *input_str) {
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
  if (json_is_real(value)) {
    gpu_power_value = json_real_value(value);
    np->gpu_power[0] = gpu_power_value;
  }

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
  timestamp = (tv.tv_sec * (uint64_t)1000) + (tv.tv_usec / 1000);
  np->timestamp = timestamp;
  snprintf(np->csv_string, MAX_CSV_SIZE - 1,
           "%ld,%lf,%lf,%lf,%lf,%lf,%lf,%lf\n", timestamp, np->node_power,
           np->cpu_power[0], np->gpu_power[0], np->mem_power[0],
           np->cpu_power[1], np->gpu_power[1], np->mem_power[1]);

  // Clean up
  json_decref(root);

  return np;
}

// Function to allocate the global temporary buffer based on the circular
// buffer's size
int allocate_global_buffer(char **buffer, size_t buffer_size) {
  *buffer = malloc(MAX_CSV_SIZE * buffer_size);
  if (!*buffer) {
    perror("Failed to allocate global buffer");
    return -1;
  }

  // Initialize the allocated memory to zero.
  memset(*buffer, 0, MAX_CSV_SIZE * buffer_size);

  return 0;
}

int node_power_cmp(void *element, void *target) {
  node_power *element_power = (node_power *)element;
  node_power *target_power = (node_power *)target;

  if (target_power->timestamp == element_power->timestamp)
    return true;
  return false;
}

void sanitize_path(char *path) {
  size_t len = strlen(path);
  if (len > 1 &&
      path[len - 1] == '/') { // Check if len > 1 to preserve root "/"
    path[len - 1] = '\0'; // Replace the last character with a null terminator
  }
}
void parse_idset_node(char *rankidset, int **rank_list, int *rank_list_size) {
  int rank[1000] = {0};
  int num_of_ids = 0;
  if (!rankidset)
    perror("rankidset null");
  if (!rank_list)
    perror("input rank list is empty");
  char *input_data = rankidset;
  if (strncmp(rankidset, "[", 1) == 0) {
    char *input_data = rankidset + 1;
    char *partial_data = strstr(input_data, "]");
    if (!partial_data)
      perror("incomplete rankidset format");
    input_data = partial_data - 1;
  }
  char *token = strtok(input_data, ",");
  do {

    // IF string contains the "-"
    char *result = strstr(token, "-");
    if (result) {
      // A list.
      // Assume the format 5-7 exist
      char *start = result - 1;
      char *end = result + 1;
      if (start <= result && *end != '\0' && *start != '\0') {
        char startStr[10], endStr[10];
        strncpy(startStr, token, result - token);
        startStr[result - token] = '\0';
        strcpy(endStr, result + 1);
        int st = atoi(startStr);
        int en = atoi(endStr);
        for (int i = st; i <= en; i++) {
          rank[num_of_ids] = i;
          num_of_ids++;
        }
      } else {
        perror("Wrong format of idset");
      }
    } else {
      int num = 0;
      num = atoi(token);
      rank[num_of_ids] = num;
      num_of_ids++;
    }
  } while ((token = strtok(NULL, ",")) != NULL);
  *rank_list_size=num_of_ids;
  *rank_list = malloc(sizeof(int) * num_of_ids);
  for (int i = 0; i < num_of_ids; i++) {
    (*rank_list)[i] = rank[i];
  }
}
void update_device_info_from_json_node(json_t *json, node_job_info *job_info,
                                  uint32_t rank) {

  json_t *execution;
  json_t *json_r_lite_array;
  size_t index;
  json_t *value;
  execution = json_object_get(json, "execution");
  if (!execution)
    perror("Failed to parse R string");
  json_r_lite_array = json_object_get(execution, "R_Lite");
  if (!json_r_lite_array)
    perror("Failed to parse R_Lite array");
  json_array_foreach(json_r_lite_array, index, value) {
    int *rank_list = NULL;
    int rank_list_size = 0;
    json_t *rank_str = json_object_get(value, "rank");
    const char *rankidset = NULL;
    if (json_is_string(rank_str)) {
      rankidset = json_string_value(rank_str);
    }
    char *str = strdup(rankidset);
    parse_idset_node(str, &rank_list, &rank_list_size);
    if (rank_list != NULL)
      for (int i = 0; i < rank_list_size; i++) {
        if (rank == rank_list[i]) {
          json_t *gpu = json_object_get(value, "gpu");
          if (json_is_string(gpu)) {
            int *gpu_list = NULL;
            int num_of_gpus = 0;
            const char *gpus_str_const = json_string_value(gpu);
            char *gpus_str = strdup(gpus_str_const);
            parse_idset_node(gpus_str, &gpu_list, &num_of_gpus);
            if (num_of_gpus > 10) {
              perror("Too Many devices to handle");
              return;
            }
            for (int i = 0; i < job_info->num_of_devices; i++) {
              job_info->deviceId[i] = gpu_list[i];
              job_info->device_type[i] = 1;
            }

            job_info->num_of_devices = num_of_gpus + 2;
            job_info->deviceId[num_of_gpus - 2] = 0;
            job_info->deviceId[num_of_gpus - 2] = 0;
            job_info->device_type[num_of_gpus - 1] = 0;
            job_info->device_type[num_of_gpus - 1] = 0;
          } else {
            perror("GPUs not found");
          }
        }
      }
  }
}
