/* Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 */

#ifndef ROOT_NODE_LEVEL_INFO_H
#define ROOT_NODE_LEVEL_INFO_H
#include <inttypes.h>

#include "retro_queue_buffer.h"
typedef struct {
    char *hostname;
    uint32_t rank;
    retro_queue_buffer_t *power_data;
} root_node_level_info;
root_node_level_info *root_node_data_new (int sender,
                                          const char *recv_from_hostname,
                                          size_t buffer_size,
                                          destructor_fn func);
void root_node_level_info_destroy (root_node_level_info *data);
#endif
