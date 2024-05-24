#ifndef FLUX_PWR_MGR_PARSE_UTIL_H
#define FLUX_PWR_MGR_PARSE_UTIL_H
#include <jansson.h>
// Responsible for dealing with all things releated to job power manager
typedef struct {
  char *hostname;
  int device_id_gpus[256];
  int device_id_cores[256];
  int num_of_cores;
  int num_of_gpus;
  int flux_rank;
} node_device_info_t;
/**
 * @brief This function parses and returbs idset in the form of RFC22
 *
 * @param rankidset input string
 * @param rank_list pointer to output ids
 * @param rank_list_size size of idset
 */
void parse_idset(char *rankidset, int **idset_list, int *idset_list_size);
/**
 * @brief Takes R json from flux's job-info.loopup RPC and populates the job
 * detail. More detail about the JSON format:
 * https://flux-framework.readthedocs.io/projects/flux-rfc/en/latest/spec_20.html
 *
 * @param json JSON returned from RPC.
 * @param job_info The job in question.r data
 * @param power_data_size size of the power data
 * @param num_of_nodes Nodes count in the c
 * @param rank  The rank whose details we need to get.
 */
int update_device_info_from_json(json_t *json,
                                 node_device_info_t ***node_device_info_list,
                                 int *length,
                                 size_t num_of_nodes);

/**
 * @brief Takes a device_data and outputs a json object
 *
 * @param device_data input device_data
 * @param power_data power_data for device power capping
 * @return the pointer to the new json object
 */
json_t *node_device_info_to_json(node_device_info_t *device_data,
                                 double *power_data);
/**
 * @brief Takes a json and ouptuts a node_device_data_t
 *
 * @param device_data input device_data
 * @param power_data array to store the poweurrent job
 * @return the newely created node_device_info_t
 */
node_device_info_t *json_to_node_device_info(json_t *device_data,
                                             double **power_data,
                                             int* power_data_size);
#endif
