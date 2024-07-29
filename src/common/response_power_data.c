#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "response_power_data.h"

const char *data_presence_string_literal[STATUS_COUNT] = {"FULL", "PARTIAL",
                                                          "NONE"};
response_power_data *response_power_data_new(const char *hostname,
                                             uint64_t start_time,
                                             uint64_t end_time) {

  response_power_data *power_data;
  power_data = calloc(1,sizeof(response_power_data));
  if (power_data == NULL)
    return NULL;
  power_data->hostname = strdup(hostname);
  if (power_data->hostname == NULL) {
    free(power_data);
    return NULL;
  }
  power_data->start_time = UINT64_MAX;
  power_data->end_time = 0;
  power_data->data_presence = NONE;
  return power_data;
}
// Function to create a JSON object from a response_power_data struct
json_t *response_power_data_to_json(response_power_data *data) {
    if (!data) return NULL;

    // Creating JSON arrays for cpu, gpu, and memory powers
    json_t *cpu_powers = json_array();
    json_t *gpu_powers = json_array();
    json_t *mem_powers = json_array();

    // Populate JSON arrays
    for (int i = 0; i < data->num_of_cpus; i++) {
        json_array_append_new(cpu_powers, json_real(data->agg_cpu_power[i]));
    }
    for (int i = 0; i < data->num_of_gpus; i++) {
        json_array_append_new(gpu_powers, json_real(data->agg_gpu_power[i]));
    }
    for (int i = 0; i < data->num_of_mem; i++) {
        json_array_append_new(mem_powers, json_real(data->agg_mem_power[i]));
    }

    // Pack all data into a single JSON object
    return json_pack("{s:s s:f s:o s:o s:o s:I s:I s:i}",
                     "hostname", data->hostname,
                     "node_power", data->agg_node_power,
                     "cpu_power", cpu_powers,
                     "gpu_power", gpu_powers,
                     "mem_power", mem_powers,
                     "result_start_time", data->start_time,
                     "result_end_time", data->end_time,
                     "data_presence", (int)data->data_presence);
}

const char *get_data_presence_string(DATA_PRESENCE_STATUS status) {
  if (status < 0 || status > STATUS_COUNT)
    status = 0;
  return data_presence_string_literal[status];
};

void response_power_data_destroy(response_power_data *data) {
  if (data == NULL)
    return;
  free((void *)data->hostname);
  free(data);
}
