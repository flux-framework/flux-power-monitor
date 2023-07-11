#include "power_data.h"
#include <stdlib.h>
power_data *power_data_new() {
  power_data *data = malloc(sizeof(power_data));
  data->cpu_power = 0.0;
  data->mem_power = 0.0;
  data->node_power = 0.0;
  data->gpu_power = 0.0;
  data->timestamp = 0;
  return data;
}
