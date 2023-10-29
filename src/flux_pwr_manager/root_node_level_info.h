#ifndef ROOT_NODE_LEVEL_INFO_H
#define ROOT_NODE_LEVEL_INFO_H
#include <inttypes.h>
#include "circular_buffer.h"
typedef struct {
  char *hostname;
  uint32_t rank;
  circular_buffer_t *power_data;
} root_node_level_info;
root_node_level_info *root_node_data_new(int sender,
                                         const char *recv_from_hostname,
                                         size_t buffer_size,
                                         destructor_fn func);
void root_node_level_info_destroy(root_node_level_info *data);
#endif
