#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "flux_pwr_logging.h"
#include "retro_queue_buffer.h"
#include "uniform_pwr_policy.h"
#include "system_config.h"
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
  int power_case=0;
  while (list_data != NULL) {
    counter++;
    double power_data = *list_data;
    if (size > 1 && counter == size - 1)
      old_period = power_data;
    if (counter == size)
      new_period = power_data;
    list_data = zlist_next(fft_result->list);
  }
  if (old_period == 0 || new_period == 0){
    new_powercap = current_powercap;
    power_case=0;
  }
  else{
  double period_diff = old_period - new_period;

  if (period_diff > 5) {
    new_powercap = current_powercap + 50;
      power_case=1;
    // Reduce some power, application not that effected by powercap.
  }
    if (period_diff > -5 && period_diff < 5) {
      power_case=2;
      new_powercap = current_powercap - 50;
    }
    // Too much efffect, application is quite slow, increase power.
    if (period_diff < -5) {
      new_powercap = current_powercap + 50;
      power_case=3;
    }
  }
  if (new_powercap < MIN_GPU_POWER){
    power_case=4;
    new_powercap=MIN_GPU_POWER;}
  if (new_powercap > powerlimit){
    power_case=5;
    new_powercap = powerlimit;}
  log_message("case:%d, PL: %f, O_p: %f, n_p:%f, O_P_cap:%f, New P_cap:%f, ",power_case,powerlimit,old_period,new_period,current_powercap,new_powercap);
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
void fft_pwr_policy_destroy(){

}
