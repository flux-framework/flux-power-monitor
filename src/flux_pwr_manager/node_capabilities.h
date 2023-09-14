#ifndef FLUX_NODE_CAPABILIITIES_H
#define FLUX_NODE_CAPABILIITIES_H
#include "device_type.h"
#include <stdbool.h>
#include <unistd.h>

typedef struct {
  int count;
  device_type type;
  bool powercap_allowed;
  double min_power;
  double max_power;
} device_capability;

typedef struct {
  device_capability gpus;
  device_capability mem;
  device_capability sockets;
  device_capability cpus;
  device_capability node;
} node_capabilities;
#endif
