#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "device_power_info.h"
#include "job_data.h"
#include "job_power_util.h"
#include "node_power_profile.h"
#include "util.h"
int parse_power_payload(json_t *payload, job_data *job, uint64_t timestamp) {
  int rc = -1;
  size_t index;
  json_t *value;
  double job_power_data_avg;
  int num_of_valid_hostname;
  json_array_foreach(payload, index, value) {
    power_data data = {0};
    const char *hostname;
    int num_of_gpus = 0;
    int num_of_sockets = 0;
    int num_of_devices = 0;
    bool mem = false;
    double node_power;
    double cpu_power;
    double gpu_power;
    double mem_power;
    uint64_t end_time, start_time;
    int found = 0;

    if (json_unpack(value, "{s:s, s:{s:F, s:F, s:F, s:F, s:I, s:I}}",
                    "hostname", &hostname, "node_power_data", "node_power",
                    &node_power, "cpu_power", &cpu_power, "gpu_power",
                    &gpu_power, "mem_power", &mem_power, "result_start_time",
                    &start_time, "result_end_time", &end_time) < 0) {
      fprintf(stderr, "Error unpacking power data");
    }
    if (gpu_power != -1)
      num_of_gpus = 1;
    if (mem_power != -1)
      mem = 1;
    num_of_sockets = 1;
    num_of_devices = num_of_sockets + num_of_gpus + mem;
    power_data **p_data = malloc(sizeof(power_data *) * num_of_devices);
    if (p_data == NULL)
      fprintf(stderr, "Unable to allocate memory for power_data while "
                      "parsing json response");
    for (int i = 0; i < num_of_sockets; i++) {
      power_data_new(SOCKETS, cpu_power, i);
    }
    for (int i = num_of_sockets; i < num_of_sockets + num_of_gpus; i++) {
      power_data_new(GPU, cpu_power, i);
    }
    for (int i = num_of_gpus + num_of_sockets;
         i < num_of_sockets + num_of_gpus + mem; i++) {
      power_data_new(MEM, cpu_power, i);
    }
    power_data *node_p_data = malloc(sizeof(power_data));
    node_p_data->device_id = index;
    node_p_data->power_value = node_power;
    node_p_data->type = NODE;
    if (job_node_power_update(job, (char *)hostname, p_data, num_of_gpus,
                              num_of_sockets, mem, num_of_devices, node_p_data,
                              timestamp) < 0) {
      fprintf(stderr, "Unable to add power_data for %s\n", hostname);

      power_data_destroy(node_p_data);
      free_power_data_list(p_data, num_of_devices);
      continue;
    }
    job_power_data_avg += node_power;
    num_of_valid_hostname++;
    power_data_destroy(node_p_data);
    free_power_data_list(p_data, num_of_devices);

    if (!found) {
      fprintf(stderr, "Hostname not found in job data: %s", hostname);
    }
  }
  power_data *j_data = malloc(sizeof(power_data));
  j_data->type = JOB;
  j_data->power_value = job_power_data_avg / num_of_valid_hostname;
  job_power_update(job, j_data);
  free(j_data);
  rc = 0;

cleanup:
  return rc;
}
void free_resources(char **node_hostname_list, int size,
                    job_data **job_data_list, int jobs_list_size) {
  for (int i = 0; i < size; i++) {
    free(node_hostname_list[i]);
  }
  free(node_hostname_list);

  for (int j = 0; j < jobs_list_size; j++) {
    job_data_destroy(job_data_list[j]);
  }
  free(job_data_list);
}

job_map_entry *find_job(job_map_entry *job_map, const uint64_t jobId,
                        size_t job_map_size) {
  for (size_t i = 0; i < job_map_size; i++) {
    if (job_map[i].jobId == jobId) {
      return &job_map[i];
    }
  }
  return NULL;
}

int handle_new_job(json_t *value, dynamic_job_map *job_map, flux_t *h) {
  char **node_hostname_list = NULL;
  int size = 0;

  json_t *nodelist = json_object_get(value, "nodelist");
  json_t *jobId_json = json_object_get(value, "id");

  if (!json_is_string(nodelist) || !json_is_integer(jobId_json)) {
    flux_log(h, LOG_CRIT, "Unable get nodeList or jobId from job");
    return -1;
  }

  const char *str = json_string_value(nodelist);
  const uint64_t jobId = json_integer_value(jobId_json);
  if (!str || !jobId) {
    flux_log(h, LOG_CRIT, "Error in sending job-list or jobId Request");
    return -1;
  }
  flux_log(h, LOG_CRIT, "jobId is %ld\n", jobId);
  getNodeList((char *)str, &node_hostname_list, &size);

  job_map_entry job_entry = {
      .jobId = jobId, .data = job_data_new(jobId, node_hostname_list, size)};

  if (!job_entry.jobId || !job_entry.data) {
    flux_log(h, LOG_CRIT, "Failed to allocate memory for job entry");
    free_resources(node_hostname_list, size, NULL,
                   0); // Modify as needed for your context
    return -1;
  }

  int add_result = add_to_job_map(job_map, job_entry);

  if (add_result != 0) {
    flux_log(h, LOG_CRIT, "Failed to add job to map");
    job_data_destroy(job_entry.data);
    free_resources(node_hostname_list, size, NULL,
                   0); // Modify as needed for your context
    return -1;
  }

  for (int i = 0; i < size; i++) {
    free(node_hostname_list[i]);
  }
  free(node_hostname_list);

  return 0;
}

int parse_jobs(flux_t *h, json_t *jobs, dynamic_job_map *job_map) {
  size_t index;
  json_t *value;

  json_array_foreach(jobs, index, value) {
    if (handle_new_job(value, job_map, h) != 0) {
      flux_log(h, LOG_ERR, "Failed to handle job at index %zu", index);
    }
  }

  for (size_t i = 0; i < job_map->size; i++) {
    const uint64_t jobId = job_map->entries[i].jobId;
    bool found = false;

    json_array_foreach(jobs, index, value) {
      json_t *jobId_json = json_object_get(value, "jobId");
      if (jobId_json && json_is_integer(jobId_json) &&
          (jobId == json_integer_value(jobId_json))) {
        found = true;
        break;
      }
    }

    if (!found) {
      remove_from_job_map(job_map, i);
      i--; // We need to decrease i as we have removed an element, so next
           // element has shifted to current i
    }
  }

  return 0;
}
