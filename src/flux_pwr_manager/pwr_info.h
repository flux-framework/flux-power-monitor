/* Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 */

#ifndef FLUX_PWR_MANAGER_PWR_INFO_H
#define FLUX_PWR_MANAGER_PWR_INFO_H
typedef struct {
  double max_power;
  double min_power;
  double current_power;

} pwr_info;
#endif
