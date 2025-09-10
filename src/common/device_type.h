/* Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 */

#ifndef FLUX_DEVICE_TYPE_H
#define FLUX_DEVICE_TYPE_H
typedef enum { GPU, CPU, MEM, NODE, SOCKETS, JOB } device_type;
#endif
