#ifndef FLUX_JOB_POWER_UTIL_H
#define FLUX_JOB_POWER_UTIL_H
#include <jansson.h>
#include "job_data.h"
power_data* parse_power_data(json_t* json);
void add_power_data_to_job(job_data* job_data,power_data* power_data);
#endif
