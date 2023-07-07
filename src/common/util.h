#ifndef FLUX_PWR_UTIL_H
#define FLUX_PWR_UTIL_H
#include "circular_buffer.h"
#include "node_power_info.h"
#include "response_power_data.h"
#include <czmq.h>
#include <inttypes.h>

response_power_data *get_agg_power_data(circular_buffer_t *buffer,
                                        const char *hostname,
                                        uint64_t start_time, uint64_t end_time);
void response_power_data_destroy(response_power_data *data);

void getNodeList(char *nodeData, char ***hostList, int *size);
#endif
