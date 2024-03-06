#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "parse_util.h"
#include "util.h"
#include <jansson.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _POSIX_C_SOURCE 200809L
void parse_idset(char *rankidset, int **idset_list, int *idset_list_size) {
  int rank[1000] = {0};
  int num_of_ids = 0;
  if (!rankidset)
    perror("rankidset null");
  if (!idset_list)
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
  *idset_list_size = num_of_ids;
  *idset_list = malloc(sizeof(int) * num_of_ids);
  for (int i = 0; i < num_of_ids; i++) {
    (*idset_list)[i] = rank[i];
  }
}

int update_device_info_from_json(json_t *json,
                                 node_device_info_t **node_device_info_list,
                                 int *length) {

  json_t *execution;
  json_t *json_r_lite_array;
  size_t index;
  json_t *value;
  char **nodelist_name;
  int num_of_nodes;
  execution = json_object_get(json, "execution");
  if (!execution) {
    perror("Failed to parse R string");
    return -1;
  }
  json_r_lite_array = json_object_get(execution, "R_Lite");
  if (!json_r_lite_array) {
    perror("Failed to parse R_Lite array");
    return -1;
  }
  json_t *json_nodelist = json_object_get(execution, "nodelist");
  if (!json_nodelist) {
    perror("JSON parse of nodelist failed");
    return -1;
  }
  const char *nodelist_const = json_string_value(json_nodelist);
  char *nodelist = strdup(nodelist_const);
  if (!nodelist) {
    perror("nodelist copying failed");
    return -1;
  }
  if (getNodeList(nodelist, &nodelist_name, &num_of_nodes) != 0) {
    perror("nodelist parsing failed");
    return -1;
  }
  free(nodelist);
  *node_device_info_list = malloc(sizeof(node_device_info_t) * num_of_nodes);
  json_array_foreach(json_r_lite_array, index, value) {
    int *rank_list = NULL;
    int rank_list_size = 0;
    json_t *rank_str = json_object_get(value, "rank");
    const char *rankidset = NULL;
    if (json_is_string(rank_str)) {
      rankidset = json_string_value(rank_str);
    }
    char *str = strdup(rankidset);
    parse_idset(str, &rank_list, &rank_list_size);
    if (rank_list != NULL)
      for (int i = 0; i < rank_list_size; i++) {
        node_device_info_t *data = *node_device_info_list + (*length + 1);
        data->hostname = strdup(nodelist_name[*length]);
        data->flux_rank = rank_list[i];
        json_t *gpu = json_object_get(value, "gpu");
        if (!gpu && json_is_string(gpu)) {
          int *gpu_list = NULL;
          int num_of_gpus = 0;

          const char *gpus_str_const = json_string_value(gpu);
          char *gpus_str = strdup(gpus_str_const);

          parse_idset(gpus_str, &gpu_list, &num_of_gpus);
          free(gpus_str);

          data->num_of_gpus = num_of_gpus;
          for (int j = 0; j < num_of_gpus; j++)
            data->device_id_gpus[j] = gpu_list[j];
        }

        json_t *core = json_object_get(value, "core");
        if (!core && json_is_string(core)) {
          int *core_list = NULL;
          int num_of_core = 0;
          const char *core_str_const = json_string_value(core);
          char *core_str = strdup(core_str_const);

          parse_idset(core_str, &core_list, &num_of_core);
          free(core_str);

          data->num_of_cores = num_of_core;
          for (int j = 0; j < num_of_core; j++)
            data->device_id_gpus[j] = core_list[j];
        }
        (*length)++;
      }
  }
  if (nodelist_name) {
    for (int i = 0; i < num_of_nodes; i++) {
      if (nodelist_name)
        free(nodelist_name[i]);
    }
    free(nodelist_name);
  }
  return 0;
}
json_t *node_device_info_to_json(node_device_info_t *device_data,
                                 double *power_data) {
  if (!device_data || !power_data)
    return NULL;

  json_t *root = json_object();
  json_object_set_new(root, "r", json_integer(device_data->flux_rank));
  json_object_set_new(root, "g", json_integer(device_data->num_of_gpus));

  // Create and fill array of GPU IDs
  json_t *gpus_array = json_array();
  for (int i = 0; i < device_data->num_of_gpus; ++i) {
    json_array_append_new(gpus_array,
                          json_integer(device_data->device_id_gpus[i]));
  }
  json_object_set_new(root, "i", gpus_array);

  // Create and fill array of power limits
  json_t *power_array = json_array();
  for (int i = 0; i < device_data->num_of_gpus; ++i) {
    json_array_append_new(power_array, json_real(power_data[i]));
  }
  json_object_set_new(root, "p", power_array);

  return root; // Caller should decrease reference count when done
}

node_device_info_t *json_to_node_device_info(json_t *device_data,
                                             double *power_data,
                                             int *power_data_size) {
  node_device_info_t *info =
      (node_device_info_t *)malloc(sizeof(node_device_info_t));

  // Extracting values from JSON
  json_t *rank = json_object_get(device_data, "r");
  json_t *num_of_gpus = json_object_get(device_data, "ng");
  json_t *gpus_ids = json_object_get(device_data, "gi");
  json_t *powerlimits = json_object_get(device_data, "pl");

  if (!json_is_integer(rank) || !json_is_integer(num_of_gpus) ||
      !json_is_array(gpus_ids) || !json_is_array(powerlimits)) {
    // Handle error
    free(info);
    return NULL;
  }

  info->flux_rank = (int)json_integer_value(rank);
  info->num_of_gpus = (int)json_integer_value(num_of_gpus);

  int num_gpus = json_array_size(gpus_ids);
  if (num_gpus > 256)
    num_gpus = 256; // or handle error

  for (int i = 0; i < num_gpus; i++) {
    json_t *gid = json_array_get(gpus_ids, i);
    if (!json_is_integer(gid)) {
      // Handle error, e.g., set default value or skip
      continue;
    }
    info->device_id_gpus[i] = (int)json_integer_value(gid);
  }

  for (int i = 0; i < info->num_of_gpus; i++) {
    json_t *pl = json_array_get(powerlimits, i);
    if (!json_is_real(pl)) {
      // Handle error
      continue;
    }
    power_data[i] = json_real_value(pl);
    *power_data_size += 1;
  }

  return info; // Remember to free this structure later
}
