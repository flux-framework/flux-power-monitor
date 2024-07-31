#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "uniform_pwr_policy.h"
#include "retro_queue_buffer.h"
#include "system_config.h"
double uniform_get_powercap(double powerlimit, double current_powercap,
                        void *data) {
  if (!data)
    return -1;
  retro_queue_buffer_t *fft_result = (retro_queue_buffer_t *)data;}
double uniform_get_power_dist(int num_of_elements, pwr_stats_t *stats,
                               double pwr_avail, double pwr_used,
                               double *result) {
  double data = pwr_avail / num_of_elements;
  if (data > MAX_GPU_POWER)
    data=MAX_GPU_POWER;
  for (int i = 0; i < num_of_elements; i++) {
    result[i] = data;
  }
  return 0;
}

void uniform_pwr_policy_init(pwr_policy_t *pwr_policy) {
  pwr_policy->get_power_dist = uniform_get_power_dist;
  pwr_policy->get_powercap = uniform_get_powercap;
}
