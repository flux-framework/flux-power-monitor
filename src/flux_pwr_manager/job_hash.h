/* Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 */

#ifndef FLUX_PWR_MANAGER_JOB_HASH_H
#define FLUX_PWR_MANAGER_JOB_HASH_H
#include <czmq.h>
zhashx_t* job_hash_create (void);
#endif
