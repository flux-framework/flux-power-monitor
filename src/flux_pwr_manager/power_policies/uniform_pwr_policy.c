#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "uniform_pwr_policy.h"

int uniform_get_powercap(pwr_stats_t *stats) { return stats->avg_pwr; }

int uniform_get_power_dist(int num_of_elements, pwr_stats_t *stats,
                           double pwr_avail, double pwr_used, double *result) {
  double data = pwr_avail / num_of_elements;
  for (int i = 0; i < num_of_elements; i++) {
    result[i] = data;
  }
  return 0;
}

void uniform_pwr_policy_init(pwr_policy_t *pwr_policy) {
  pwr_policy->get_power_dist = uniform_get_power_dist;
  pwr_policy->get_powercap = uniform_get_powercap;
}
