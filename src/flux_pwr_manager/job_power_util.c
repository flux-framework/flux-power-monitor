#include "job_power_util.h"
#include "util.h"
int parse_power_payload(flux_t *h, json_t *payload, job_data *job) {
  int rc = -1;
  size_t index;
  json_t *value;

  json_array_foreach(payload, index, value) {
    power_data data = {0};
    const char *hostname;
    int found = 0;

    if (json_unpack(value, "{s:s, s:{s:F, s:F, s:F, s:F, s:I, s:I}}",
                    "hostname", &hostname, "node_power_data", "node_power",
                    &data.node_power, "cpu_power", &data.cpu_power, "gpu_power",
                    &data.gpu_power, "mem_power", &data.mem_power,
                    "result_start_time", &data.timestamp, "result_end_time",
                    &data.timestamp) < 0) {
      flux_log_error(h, "Error unpacking power data");
    }

    for (int i = 0; i < job->num_of_nodes; i++) {
      if (strcmp(job->node_hostname_list[i], hostname) == 0) {
        if (add_power_data_to_node_history(job->node_power_profile_data[i],
                                           &data) < 0) {
          flux_log_error(h, "Error adding power data to node history");
        }
        found = 1;
        break;
      }
    }

    if (!found) {
      flux_log(h, LOG_ERR, "Hostname not found in job data: %s", hostname);
    }
  }

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

job_map_entry *find_job(job_map_entry *job_map, const char *jobId,
                        size_t job_map_size) {
  for (size_t i = 0; i < job_map_size; i++) {
    if (strcmp(job_map[i].jobId, jobId) == 0) {
      return &job_map[i];
    }
  }
  return NULL;
}

void handle_new_job(json_t *value, dynamic_job_map *job_map, flux_t *h) {
  char **node_hostname_list = NULL;
  int size = 0;

  json_t *nodelist = json_object_get(value, "nodelist");
  json_t *jobId_json = json_object_get(value, "jobId");

  if (!json_is_string(nodelist) || !json_is_string(jobId_json)) {
    flux_log(h, LOG_CRIT, "Unable get nodeList or jobId from job");
    return;
  }

  const char *str = json_string_value(nodelist);
  const char *jobId = json_string_value(jobId_json);
  if (!str || !jobId) {
    flux_log(h, LOG_CRIT, "Error in sending job-list or jobId Request");
    return;
  }

  getNodeList((char *)str, &node_hostname_list, &size);

  job_map_entry job_entry = {
      .jobId = strdup(jobId),
      .data = job_data_new((char *)jobId, node_hostname_list, size)};
  add_to_job_map(job_map, job_entry);

  for (int i = 0; i < size; i++) {
    free(node_hostname_list[i]);
  }
  free(node_hostname_list);
}

void parse_jobs( flux_t *h,json_t* jobs, dynamic_job_map *job_map,size_t job_map_size) {
  size_t index;
  json_t *value;

  // Create a new job map
  dynamic_job_map *new_job_map = init_job_map(job_map_size);

  // Now handle each new job and add them into new_job_map
  json_array_foreach(jobs, index, value) {
    handle_new_job(value, new_job_map, h);
  }

  // Free the memory used by the old job_map
  for (size_t i = 0; i < job_map->size; i++) {
    free(job_map->entries[i].jobId);
    job_data_destroy(job_map->entries[i].data);
  }
  free(job_map->entries);
  free(job_map);

  // Assign the new_job_map to job_map
  job_map = new_job_map;
}
