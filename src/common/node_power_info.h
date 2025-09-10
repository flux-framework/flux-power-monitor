/* Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 */

#ifndef NODE_POWER_INFO_H
#define NODE_POWER_INFO_H
#include <inttypes.h>
#include <sys/time.h>
typedef struct {
    char *hostname;
    char *power_info;
    uint64_t timestamp;
} node_power_info;

node_power_info *node_power_info_new (const char *hostname,
                                      const char *power_info,
                                      uint64_t timestamp);
void node_power_info_destroy (void *power_data);
#endif
