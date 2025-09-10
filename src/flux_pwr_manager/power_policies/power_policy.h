/* Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 */

#ifndef FLUX_PWR_MANAGER_POWER_POLICY_H
#define FLUX_PWR_MANAGER_POWER_POLICY_H
typedef enum {
    GPU_FOCUSED,
    UTILIZATION_AWARE,
    DYNAMIC_POWER,
    UNIFORM,
    FFT
} POWER_POLICY_TYPE;
#endif
