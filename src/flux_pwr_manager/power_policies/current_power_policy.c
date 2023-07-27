#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "current_power_policy.h"
#include <math.h>

double get_powercap(double powercap, double power_current,
                    double max_powercap) {
  double power_diff = powercap - power_current;

  if (power_diff < 0) {
    return powercap;
  }

  if (power_diff < 5) {
    powercap += 5;
  } else if (power_diff > 5) {
    powercap = power_current;
  }

  if (powercap > max_powercap) {
    powercap = max_powercap;
  }

  return powercap;
}

double current_power_policy_get_job_powercap(job_data *job_data) {
  if (job_data == NULL) {
    return -1;
  }

  job_data->powercap = get_powercap(job_data->powercap, job_data->power_current,
                                    job_data->max_powercap);

  return job_data->powercap;
}

double
current_power_policy_get_device_powercap(device_power_profile *device_data) {
  if (device_data == NULL) {
    return -1;
  }

  device_data->powercap =
      get_powercap(device_data->powercap, device_data->current_power,
                   device_data->max_powercap);

  return device_data->powercap;
}

double current_power_policy_get_node_powercap(node_power_profile *node_data) {
  if (node_data == NULL) {
    return -1;
  }

  node_data->powercap = get_powercap(
      node_data->powercap, node_data->power_current, node_data->max_powercap);

  return node_data->powercap;
}
