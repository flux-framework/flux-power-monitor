#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "flux_pwr_logging.h"
#include "retro_queue_buffer.h"
#include "system_config.h"
#include "uniform_pwr_policy.h"
#include <czmq.h>
#include <math.h>

double powercap_data[3] = {10.f, 25.f, 50.f};
double converge_time = 2.0f;
double decrease_threashold = 50;
void *get_second_last_zlist_element(zlist_t *powercap_zlist) {
  // Get the size of the zlist
  size_t size = zlist_size(powercap_zlist);

  if (size < 2) {
    return NULL;
  }

  void *element = zlist_first(powercap_zlist);
  for (size_t i = 1; i < size - 1; ++i) {
    element = zlist_next(powercap_zlist);
  }

  return element;
}
double get_new_powercap(pwr_policy_t *mgr, double powerlimit,
                        double new_period) {
  double new_powercap = powerlimit;
  if (mgr == NULL)
    return new_powercap;
  if (mgr->powercap_history == NULL)
    return new_powercap;
  size_t current_size =
      retro_queue_buffer_get_current_size(mgr->powercap_history);
  zlist_t *powercap_zlist = mgr->powercap_history->list;
  if (current_size == 0) {
    return new_powercap;
  }
  log_message("Size of buffer %d", current_size);
  // Let the program run for long time now, we know the period.
  if (current_size == 1) {
    return (new_powercap - decrease_threashold);
  }

  double *temp = (double *)zlist_last(powercap_zlist);
  double current_powercap = -1;
  if (temp == NULL)
    current_powercap = powerlimit; // There is no history, something messy
                                   // happend, prior case should handled this;
  else
    current_powercap = *temp;

  if (mgr->converged) {
    new_powercap = current_powercap;
    return new_powercap;
  }
  double old_period = 0;
  if (retro_queue_buffer_get_current_size(mgr->time_history) == 0) {
    old_period = 0;
  } else {
    double *c = (double *)zlist_last(mgr->time_history->list);
    if (c == NULL)
      old_period = 0;
    else
      old_period = *c;
  }
  double time_delta = new_period - old_period;
  double abs_delta = fabs(time_delta);
  if (abs_delta < converge_time) {
    double *temp_2 = get_second_last_zlist_element(powercap_zlist);
    mgr->converged = true;
    if (temp_2 == NULL)
      new_powercap = powerlimit;
    else
      new_powercap = *temp_2;
    return new_powercap;
  }
  if (time_delta < 0) {
    if (abs_delta < 5 && abs_delta > converge_time) {
      new_powercap = current_powercap - decrease_threashold;
    }
  }
  if (abs_delta < 5.0f) {
    new_powercap = current_powercap + powercap_data[0];
  } else if (abs_delta > 5.0f && abs_delta < 10.0f) {
    new_powercap = current_powercap + powercap_data[1];
  } else if (abs_delta > 10.0f) {
    new_powercap = current_powercap + powercap_data[2];
  }
  return new_powercap;
}
/** We are going to increment power in multiple of 50W.
 *
 *
 * */
double fft_get_powercap(pwr_policy_t *mgr, double powerlimit,
                        double current_powercap, void *data) {
  if (!data)
    return powerlimit;
  retro_queue_buffer_t *fft_result = (retro_queue_buffer_t *)data;
  int counter = 0;
  double new_powercap = 0;
  double new_period = 0, old_period = 0;
  int size = retro_queue_buffer_get_current_size(fft_result);
  double *list_data = zlist_first(fft_result->list);
  int power_case = 0;
  while (list_data != NULL) {
    counter++;
    double power_data = *list_data;
    if (size > 1 && counter == size - 1)
      old_period = power_data;
    if (counter == size)
      new_period = power_data;
    list_data = zlist_next(fft_result->list);
  }
  double new_powercap_1 = 0;
  new_powercap_1 = get_new_powercap(mgr, powerlimit, new_period);
  new_powercap = new_powercap_1;
  double old_period_1 = 0;
  if (retro_queue_buffer_get_current_size(mgr->time_history) == 0) {
    old_period_1 = 0;
  } else {
    double *c = zlist_last(mgr->time_history->list);
    if (c == NULL)
      old_period_1 = 0;
    else
      old_period_1 = *c;
  }
  if (new_powercap < MIN_GPU_POWER) {
    new_powercap = MIN_GPU_POWER;
  }
  if (new_powercap > powerlimit) {
    new_powercap = powerlimit;
  }
  double *p_cap_data = malloc(sizeof(double));
  double *p_limit_data = malloc(sizeof(double));
  double *period = malloc(sizeof(double));
  *p_cap_data = new_powercap;
  *p_limit_data = powerlimit;
  *period = new_period;
  retro_queue_buffer_push(mgr->powercap_history, p_cap_data);
  retro_queue_buffer_push(mgr->powerlimit_history, p_limit_data);
  retro_queue_buffer_push(mgr->time_history, period);
  log_message("Poilcy: %f, PL: %f, O_p: %f, n_p:%f, O_P_cap:%f, New P_cap:%f, ",
              new_powercap_1, powerlimit, old_period_1, new_period,
              current_powercap, new_powercap);
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
