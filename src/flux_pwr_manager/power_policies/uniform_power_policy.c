#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "uniform_power_policy.h"
#include <math.h>

static double get_powercap(double powercap, double power_current,
                    double max_powercap) {
  double power_diff = powercap - power_current;
  // No load balancing.
return max_powercap;
}

double uniform_power_policy_get_job_powercap(job_data *job_data) {
  if (job_data == NULL) {
    return -1;
  }

  job_data->powercap = get_powercap(job_data->powercap, job_data->power_current,
                                    job_data->powerlimit);

  return job_data->powercap;
}

double
uniform_power_policy_get_device_powercap(device_power_profile *device_data) {
  if (device_data == NULL) {
    return -1;
  }

  if (device_data->powercap_allowed)
    device_data->powercap =
        get_powercap(device_data->powercap, device_data->current_power,
                     device_data->powerlimit);

  return device_data->powercap;
}

double uniform_power_policy_get_node_powercap(node_power_profile *node_data) {
  if (node_data == NULL) {
    return -1;
  }
  if (node_data->powercap_allowed)
    node_data->powercap = get_powercap(
        node_data->powercap, node_data->power_current, node_data->powerlimit);

  return node_data->powercap;
}
