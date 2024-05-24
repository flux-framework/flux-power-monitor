#ifndef FLUX_PWR_MANAGER_NODE_UTIL_H
#define FLUX_PWR_MANAGER_NODE_UTIL_H
#include "jansson.h"
#include "node_data.h"
#include "node_job_info.h"
#include "tracker.h"
node_power *parse_string(const char *input_str);
int allocate_global_buffer(char **buffer, size_t buffer_size);
int node_power_cmp(void *element, void *target);
void sanitize_path(char *path);
int write_device_specific_node_power_data(char *data,
                                          node_power *full_node_data,
                                          node_job_info *job_data);
void get_header(char **header, node_job_info *job_data);
void reset_buffer(char *buffer,size_t buffer_size);
#endif
