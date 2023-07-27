#ifndef FLUX_NODE_CAPABILITY_H
#define FLUX_NODE_CAPABILITY_H
#include "device_type.h"
typedef struct{
  char *hostname;
  int number_of_control_devices;
  double **max_power;
  double **min_power;
  device_type *device_list;
}node_capability;

#endif
