#ifndef FLUX_PWR_MANAGER_JOB_POWER_UTIL_H
#define FLUX_PWR_MANAGER_JOB_POWER_UTIL_H
#include "dynamic_job_map.h"
#include "flux/core.h"
#include "job_data.h"
#include "node_capabilities.h"
#include <jansson.h>
job_data *find_job(dynamic_job_map *job_map, uint64_t jobId);

node_power_profile *find_node(job_data *data, char *hostname);
int parse_node_capabilities(char *json_str, node_capabilities *result);

device_power_profile *find_device(node_power_profile *node, device_type type,
                                  int device_id);
int parse_power_payload(json_t *payload, job_data *job, uint64_t timestamp);
int parse_jobs(flux_t *h, json_t *jobs, dynamic_job_map *job_map);
#endif
