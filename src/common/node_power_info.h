#ifndef NODE_POWER_INFO_H
#define NODE_POWER_INFO_H
#include <sys/time.h>
#include <inttypes.h>
typedef struct {

  char *hostname;
  char *power_info;
  uint64_t timestamp;
} node_power_info;

node_power_info *node_power_info_new(const char *hostname,
                                     const char *power_info, uint64_t timestamp);
void node_power_info_destroy(void *power_data);
#endif
