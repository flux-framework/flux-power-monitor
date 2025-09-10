/* Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 */

#ifndef FLUX_PWR_MANAGER_POWER_STATS_H
#define FLUX_PWR_MANAGER_POWER_STATS_H
typedef struct {
    double max_pwr;
    double min_pwr;
    double avg_pwr;
    double median_pwr;
    double duration_perc;
    double powerlimit;
} pwr_stats_t;
#endif
