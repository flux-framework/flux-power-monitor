/* Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 */

#ifndef FLUX_PWR_MANAGER_NODE_CAPABILIITIES_H
#define FLUX_PWR_MANAGER_NODE_CAPABILIITIES_H
#include <stdbool.h>
#include <unistd.h>

#include "device_type.h"

typedef struct {
    int count;
    device_type type;
    bool powercap_allowed;
    double min_power;
    double max_power;
} device_capability;

typedef struct {
    device_capability gpus;
    device_capability mem;
    device_capability sockets;
    device_capability cpus;
    device_capability node;
} node_capabilities;
#endif
