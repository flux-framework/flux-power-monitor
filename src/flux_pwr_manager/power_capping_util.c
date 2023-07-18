#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "power_capping_util.h"
#include "stdlib.h"
#include <math.h>

void set_node_level_power_cap(job_data *data) {}
void get_job_level_powercap(dynamic_job_map *job_map,
                            double global_power_budget,
                            POWER_POLICY power_policy) {
  for (int i = 0; i < job_map->size; i++) {
    job_data *data = job_map->entries[i].data;
      double power_gap = fabs(data->job_power_cap - data->job_power_current);
    if (power_policy == CURRENT_POWER) {

      if(power_gap>10){
        
      }
      else if(power_gap<5){}

  }
}
}
void set_job_level_power_cap(int number_of_nodes, double node_power_avg) {}
