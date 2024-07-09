#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "root_node_level_info.h"
root_node_level_info *root_node_data_new(int sender,
                                         const char *recv_from_hostname,
                                         size_t buffer_size,
                                         destructor_fn func) {
  if (recv_from_hostname == NULL)
    return NULL;
  if (func == NULL)
    return NULL;
  root_node_level_info *root_all_node_data;
  root_all_node_data = malloc(sizeof(root_node_level_info));
  if (root_all_node_data == NULL)
    return NULL;
  root_all_node_data->rank = sender;
  root_all_node_data->hostname = strdup(recv_from_hostname);
  if (root_all_node_data->hostname == NULL) {
    free(root_all_node_data);
    return NULL;
  }
  root_all_node_data->power_data = retro_queue_buffer_new(buffer_size, func);
  if (root_all_node_data->power_data == NULL) {
    free(root_all_node_data->hostname);
    free(root_all_node_data);
    return NULL;
  }
  return root_all_node_data;
}
void root_node_level_info_destroy(root_node_level_info *data) {
  if (data == NULL)
    return;
  free(data->hostname);
  retro_queue_buffer_destroy(data->power_data);
  free(data);
}
