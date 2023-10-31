#ifndef FLUX_PWR_MANAGER_NODE_UTIL_H
#define FLUX_PWR_MANAGER_NODE_UTIL_H
#include "node_data.h"
node_power *parse_string(const char *input_str) ;
int allocate_global_buffer(char *buffer, size_t *buffer_size,
                            size_t circular_buffer_size);
#endif
