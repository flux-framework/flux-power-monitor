#ifndef FLUX_JOB_POWER_UTIL_H
#define FLUX_JOB_POWER_UTIL_H
#include "dynamic_job_map.h"
#include "flux/core.h"
#include "job_data.h"
#include <jansson.h>
#include "node_capability.h"
int parse_power_payload( json_t *payload, job_data *job,uint64_t timestamp);
int parse_jobs(flux_t *h, json_t* jobs, dynamic_job_map *job_map) ;
int parse_node_capabilities(flux_t* h,json_t* data,node_capability* node_data);
#endif
