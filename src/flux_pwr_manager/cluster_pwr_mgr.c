#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "cluster_pwr_mgr.h"
#include "constants.h"
#include "flux_pwr_logging.h"
#include "job_hash.h"
#include "parse_util.h"
#include "power_policies/uniform_pwr_policy.h"
#include "system_config.h"
#define MAX_HOSTNAME_SIZE 256
flux_t *flux_handle;
char **cluster_node_hostname_list;
int nodes_in_cluster;
cluster_mgr_t *cluster_self_ref = NULL;
// Assuming a homogenous cluster.
size_t current_device_utilized = 0;
size_t total_num_of_devices = 0;
size_t current_nodes_utilized = 0;
cluster_mgr_t *cluster_mgr_new(flux_t *h, double global_power_budget,
                               uint64_t cluster_size) {
  if (global_power_budget == 0 || cluster_size == 0)
    return NULL;

  cluster_node_hostname_list = calloc(cluster_size, sizeof(char *));
  if (!cluster_node_hostname_list)
    return NULL;

  cluster_mgr_t *cluster_mgr = calloc(1, sizeof(cluster_mgr_t));
  if (!cluster_mgr) {
    free(cluster_node_hostname_list);
    return NULL;
  }

  cluster_mgr->job_hash_table = job_hash_create();
  if (!cluster_mgr->job_hash_table) {
    free(cluster_node_hostname_list);
    free(cluster_mgr);
    return NULL;
  }

  zhashx_set_destructor(cluster_mgr->job_hash_table, job_map_destroy);
  cluster_mgr->global_power_budget = global_power_budget;
  cluster_mgr->num_of_nodes = cluster_size;
  nodes_in_cluster = cluster_size;
  total_num_of_devices = cluster_size * NUM_OF_GPUS;
  cluster_self_ref = cluster_mgr;
  flux_handle = h;
  return cluster_mgr;
}
// Uniform power distributed
double redistribute_power(cluster_mgr_t *cluster_mgr, int num_of_devices) {
  if (cluster_mgr == NULL || num_of_devices == 0)
    return 0;
  log_message("cluster global powerlimit %f and num_of_devices %d",
              cluster_mgr->global_power_budget, num_of_devices);
  double new_per_device_powerlimit =
      cluster_mgr->global_power_budget / (num_of_devices);

  job_map_t *data = zhashx_first(cluster_mgr->job_hash_table);
  while (data != NULL) {
    job_mgr_t *job_data = data->job_pwr_manager;
    log_message("data->jobId %ld data->num_of_gpus %d", data->jobId,
                job_data->num_of_gpus);
    double new_job_powelimit =
        new_per_device_powerlimit * job_data->num_of_gpus;

    job_mgr_update_powerlimit(job_data, flux_handle, new_job_powelimit);

    data = zhashx_next(cluster_mgr->job_hash_table);
  }
  return new_per_device_powerlimit;
}

// Get device info for each job in the node.
// This is a synchronous call
int get_new_job_device_info(flux_t *h, uint64_t job_id, size_t num_of_nodes,
                            node_device_info_t ***node_data) {
  if (job_id == 0 || h == NULL)
    return -1;
  flux_future_t *f;
  if (!(f = flux_rpc_pack(h, "job-info.lookup", FLUX_NODEID_ANY, 0,
                          "{s:I s:[s] s:i}", "id", job_id, "keys", "R", "flags",
                          0))) {
    log_error("RPC_ERROR:Unable to send RPC to get device info for the job %s",
              flux_future_error_string(f));
    flux_future_destroy(f);
  }

  const char *R;
  ;
  int version;
  if (flux_rpc_get_unpack(f, "{s:s}", "R", &R) < 0) {
    log_error("RPC_INFO:Unable to parse RPC data for device info %s",
              flux_future_error_string(f));
    flux_future_destroy(f);
    return -1;
  }

  log_message("R %s\n", R);
  json_t *json_r;
  json_error_t err;
  json_r = json_loads(R, 0, &err);
  if (json_r) {
    if (cluster_self_ref) {
      int length = 0;
      if (update_device_info_from_json(json_r, node_data, &length,
                                       num_of_nodes) < 0) {
        return -1;
      }
    } else
      log_error("cluster_self_ref of job_pwr_mgr does not exist when parsing "
                "device info");
  } else {
    log_error("error in parsing R string");
  }
  flux_future_destroy(f);

  return 0;
}
double get_new_job_powerlimit(cluster_mgr_t *cluster_mgr,
                              double num_of_requested_nodes_count,
                              node_device_info_t **node_data) {

  double excess_power =
      (cluster_mgr->global_power_budget - cluster_mgr->current_power_usage);
  int num_of_requested_devices = 0;
  for (int i = 0; i < num_of_requested_nodes_count; i++)
    num_of_requested_devices += node_data[i]->num_of_gpus;
  double theortical_power_per_device = excess_power / num_of_requested_devices;

  double powerlimit_job = 0;
  // This is the case when power is access or just at max
  //
  if (theortical_power_per_device >= MAX_GPU_POWER) {
    powerlimit_job = MAX_GPU_POWER * num_of_requested_devices;
  }
  // Not sufficient power for each node, each job has to suffer now.
  // Each job is getting total_pow/total_num_of_nodes.
  else if (theortical_power_per_device < MAX_GPU_POWER) {
    double new_per_device_powerlimit = redistribute_power(
        cluster_mgr, current_device_utilized + num_of_requested_devices);
    powerlimit_job = new_per_device_powerlimit * num_of_requested_devices;
  }
  cluster_mgr->current_power_usage += powerlimit_job;
  return powerlimit_job;
}

void cluster_mgr_add_hostname(int rank, char *hostname) {
  cluster_node_hostname_list[rank] = strdup(hostname);
}

int cluster_mgr_add_new_job(cluster_mgr_t *cluster_mgr, uint64_t jobId,
                            char **nodelist, int requested_nodes_count,
                            char *cwd, char *job_name) {
  if (jobId == 0 || !nodelist || !cwd || !cluster_mgr ||
      requested_nodes_count == 0)
    return -1;
  char **node_hostname_list;

  job_map_t *map = malloc(sizeof(job_map_t));
  if (!map)
    return -1;
  map->jobId = jobId;
  node_device_info_t **node_data = NULL;
  if (get_new_job_device_info(flux_handle, jobId, requested_nodes_count,
                              &node_data) < 0) {
    log_error("Device Found failed");
    return -1;
  }
  if (!node_data) {
    log_error("Unable to get device data for job \n");
    return -1;
  }
  log_message("cluster_mgr requested node %d and jobId %ld",
              requested_nodes_count, jobId);
  int *ranks = calloc(requested_nodes_count, sizeof(int));
  // Quite inefficent but should be fine for newer systems
  for (int i = 0; i < requested_nodes_count; i++) {
    for (int j = 0; j < nodes_in_cluster; j++) {
      if (strcmp(nodelist[i], cluster_node_hostname_list[j]) == 0) {
        ranks[i] = j;
        break;
      }
    }
  }
  double job_pl =
      get_new_job_powerlimit(cluster_mgr, requested_nodes_count, node_data);
  log_message("new job %ld powerlimit %f", jobId, job_pl);
  map->job_pwr_manager =
      job_mgr_new(jobId, nodelist, requested_nodes_count, cwd, job_name,
                  UNIFORM, job_pl, node_data, ranks, flux_handle);
  if (!map->job_pwr_manager)
    return -1;
  if (!cluster_mgr->job_hash_table)
    return -1;
  log_message("Inserting data");
  zhashx_insert(cluster_mgr->job_hash_table, &map->jobId, (void *)map);
  current_nodes_utilized += requested_nodes_count;
  int num_of_requested_devices = 0;
  for (int i = 0; i < requested_nodes_count; i++)
    num_of_requested_devices += node_data[i]->num_of_gpus;
  current_device_utilized += num_of_requested_devices;
  free(ranks);
  return 0;
}

int cluster_mgr_remove_job(cluster_mgr_t *cluster_mgr, uint64_t jobId) {
  log_message("cluster_mgr:remove job %ld", jobId);
  if (jobId == 0 || !cluster_mgr || !cluster_mgr->job_hash_table)
    return -1;
  job_map_t *job_map =
      (job_map_t *)zhashx_lookup(cluster_mgr->job_hash_table, &jobId);
  if (!job_map) {
    log_message("lookup failure");
    log_message("hash table size %d", zhashx_size(cluster_mgr->job_hash_table));
    log_message(
        "hash table first_element %ld",
        (uint64_t)zlistx_first(zhashx_keys(cluster_mgr->job_hash_table)));
    return -1;
  }
  //  Right now just focusing on GPUs.
  log_message("jobs data GPUS:%d nodes:%d\n",
              job_map->job_pwr_manager->num_of_gpus,
              job_map->job_pwr_manager->num_of_nodes);
  current_device_utilized -= job_map->job_pwr_manager->num_of_gpus;
  // TODO: Should we redistribute power when we remove a job ?
  current_nodes_utilized -= job_map->job_pwr_manager->num_of_nodes;
  log_message("cluster_mgr:remove job");
  zhashx_delete(cluster_mgr->job_hash_table, &job_map->jobId);
  return 0;
}

void cluster_mgr_destroy(cluster_mgr_t **manager) {
  if (!manager || !*manager) {
    log_error("cluster manager does not exist");
    return;
  }
  cluster_mgr_t *mgr = *manager;
  zhashx_purge(mgr->job_hash_table);
  zhashx_destroy(&mgr->job_hash_table);

  for (int i = 0; i < nodes_in_cluster; i++) {
    free(cluster_node_hostname_list[i]);
  }
  free(cluster_node_hostname_list);
  cluster_self_ref = NULL;
  free(mgr);
  *manager = NULL; // Avoid dangling pointer
}

void job_map_destroy(void **job_map) {
  log_message("Destroyer for job map called");
  if (job_map && *job_map) {
    job_map_t *proper_job_map = *job_map;
    job_mgr_destroy(flux_handle, &proper_job_map->job_pwr_manager);
    free(proper_job_map);
    *job_map = NULL; // Avoid dangling pointer
  }
}
int cluster_mgr_set_global_pwr_budget(cluster_mgr_t *cluster_mgr, double pwr) {
  if (!cluster_mgr)
    return -1;
  cluster_mgr->global_power_budget = pwr;
  return 0;
}

void cluster_mgr_set_global_powerlimit_cb(flux_t *h, flux_msg_handler_t *mh,
                                          const flux_msg_t *msg, void *args) {
  double powerlimit;
  if (flux_request_unpack(msg, NULL, "{s:f}", "gl_pl", &powerlimit) < 0) {
    log_error("RPC_ERROR:Unable to decode set powerlimit RPC");
  }
  if (powerlimit <= 0) {
    log_message("RPC ERROR powerlimit");
    return;
  }
  cluster_self_ref->global_power_budget = powerlimit;
  log_message("Got new power %f", powerlimit);
  redistribute_power(cluster_self_ref, current_device_utilized);
}
void cluster_mgr_set_power_ratio_cb(flux_t *h, flux_msg_handler_t *mh,
                                    const flux_msg_t *msg, void *args) {

  int powerratio;
  if (flux_request_unpack(msg, NULL, "{s:i}", "pr", &powerratio) < 0) {
    log_error("RPC_ERROR:Unable to decode set poweratio RPC");
  }
  log_message("Setting Power Ratio of cluster to %d", powerratio);
  // global_power_ratio = powerratio;
}
