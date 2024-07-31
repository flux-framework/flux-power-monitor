#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "flux_pwr_logging.h"
#include "retro_queue_buffer.h"
#include "uniform_pwr_policy.h"
/** We are going to increment power in multiple of 50W.
 *
 *
 * */
double fft_get_powercap(double powerlimit, double current_powercap,
                        void *data) {
  if (!data)
    return -1;
  retro_queue_buffer_t *fft_result = (retro_queue_buffer_t *)data;
  if (data == NULL)
    return -1;
  int counter = 0;
  double new_powercap = 0;
  double new_period = 0, old_period = 0;
  int size = retro_queue_buffer_get_current_size(fft_result);
  double *list_data = zlist_first(fft_result->list);
  while (list_data != NULL) {
    counter++;
    double power_data = *list_data;
    if (size > 1 && counter == size - 1)
      old_period = power_data;
    if (counter == size)
      new_period = power_data;
    list_data = zlist_next(fft_result->list);
  }
  if (old_period == 0 || new_period == 0)
    new_powercap = current_powercap;
  log_message("FFT_BASED_POWER:old powercap %f old_period %f new_period %f ",
              current_powercap, old_period, new_period);

  double period_diff = old_period - new_period;
  log_message("period_dff %f",period_diff);
  // Positive feedback loop, give more power.
  if (period_diff > 2) {
    new_powercap = current_powercap + 50;
    log_message("period diff is greater then 2 %f",new_powercap);
    // Reduce some power, application not that effected by powercap.
  }
    if (period_diff > -2 && period_diff < 2) {
      new_powercap = current_powercap - 50;
    log_message("period diff is between  2 %f",new_powercap);
    }
    // Too much efffect, application is quite slow, increase power.
    if (period_diff < -2) {
      new_powercap = current_powercap + 50;
    }

  if (new_powercap > powerlimit)
    new_powercap = powerlimit;
  log_message("New powercap %f", new_powercap);
  return new_powercap;
}

double fft_get_power_dist(int num_of_elements, pwr_stats_t *stats,
                          double pwr_avail, double pwr_used, double *result) {
  double data = pwr_avail / num_of_elements;
  for (int i = 0; i < num_of_elements; i++) {
    result[i] = data;
  }
  return 0;
}

void fft_pwr_policy_init(pwr_policy_t *pwr_policy) {
  pwr_policy->get_power_dist = fft_get_power_dist;
  pwr_policy->get_powercap = fft_get_powercap;
}
