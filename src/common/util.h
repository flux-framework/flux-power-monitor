/* Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 */

#ifndef FLUX_PWR_UTIL_H
#define FLUX_PWR_UTIL_H
#include <czmq.h>
#include <inttypes.h>

#include "node_power_info.h"
#include "power_data.h"
#include "response_power_data.h"
#include "retro_queue_buffer.h"

response_power_data *get_agg_power_data (retro_queue_buffer_t *buffer,
                                         const char *hostname,
                                         uint64_t start_time,
                                         uint64_t end_time);
void response_power_data_destroy (response_power_data *data);

double do_agg (retro_queue_buffer_t *buffer,
               double current_power_value,
               double old_power_value);
double do_average (retro_queue_buffer_t *buffer);
int getNodeList (char *nodeData, char ***hostList, int *size);
uint64_t get_device_id (char *name);

#endif
