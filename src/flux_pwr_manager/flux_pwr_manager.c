#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "constants.h"
#include "device_power_info.h"
#include "dynamic_job_map.h"
#include "flux_pwr_logging.h"
#include "job_data.h"
#include "job_power_util.h"
#include "json_utility.h"
#include "node_capabilities.h"
#include "node_manager/node_manager.h"
#include "power_policies/dynamic_power_strategy.h"
#include "power_policies/power_policy.h"
#include "power_policies/power_policy_manager.h"
#include "power_policies/uniform_power_strategy.h"
#include "power_util.h"
#include "util.h"
#include "variorum.h"
#include <assert.h>
#include <czmq.h>
#include <flux/core.h>
#include <flux/jobtap.h>
#include <jansson.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define JOB_MAP_SIZE 100
#define HOSTNAME_SIZE 256
#define MAX_NODE_POWER 3050
#define MIN_NODE_POWER 500
#define BUFFER_SIZE                                                            \
  360 // Number of samples in seconds, total 6 mins collect data
int num_of_job = 0;
flux_t *flux_handler = NULL;
uint64_t current_job_id[100];
#define MY_MOD_NAME "flux_pwr_manager"
const char default_service_name[] = MY_MOD_NAME;
double global_power_budget;
uint64_t max_global_powerlimit;
uint64_t min_global_powerlimit;
power_strategy *current_power_strategy;
char node_hostname[HOSTNAME_SIZE];
char **hostname_list;
dynamic_job_map *job_map_data;
uint32_t rank, size;
node_capabilities current_node_capabilities = {0};

// static const int NOFLAGS=0;
/*
 * This code sets power usage limits, or 'powercaps', at different levels: jobs,
 * nodes, and devices (which include computer nodes, GPUs, CPUs, sockets, and
 * memory).
 *
 * Initially, power is  allocated amongst jobs. Each job is then
 * subdivided into nodes, and each node is subdivided into devices. This
 * requires our system to support powercaps at the node and device levels.
 *
 * We use a simple strategy and policy system for this. The strategy determines
 * how power is distributed, while the policy governs how the powercaps are set.
 *
 * Some systems only permit node-level powercaps. If this is the case, we simply
 * cap the power for each node. However, if a system allows power distribution
 * to individual devices, we follow that approach. Otherwise, a basic powercap
 * is applied at the node level.
 *
 * When distributing power initially, we  assign it across each level
 * (job, node, device), setting a power limit for each component. However,
 * setting a powercap for a node could also affect the power limits of its
 * associated devices, which depends on the power distribution policy.
 */
// enum {
//   UPSTREAM = 0,
//   DOWNSTREAM1,
//   DOWNSTREAM2,
//   MAX_NODE_EDGES,
//   MAX_CHILDREN = 2,
// };

// static int dag[MAX_NODE_EDGES];

static uint32_t sample_id = 0; // sample counter

static double node_power_acc = 0.0; // Node power accumulator
static double gpu_power_acc = 0.0;  // GPU power accumulator
static double cpu_power_acc = 0.0;  // CPU power accumulator
static double mem_power_acc = 0.0;  // Memory power accumulator

/**
 *
 *
 */
int find_rank_hostname(char *hostname) {
  int rank = -1;
  for (int i = 0; i < size; i++) {
    if (hostname_list[i] == NULL)
      continue;
    if (strcmp(hostname, hostname_list[i]) == 0)
      return i;
  }
  return rank;
}

void update_power_policy() {
  current_power_strategy->set_job_power_policy(job_map_data);
  for (int i = 0; i < job_map_data->size; i++) {
    current_power_strategy->set_node_power_policy(
        job_map_data->entries[i].data);
    for (int j = 0; j < job_map_data->entries[i].data->num_of_nodes; j++) {
      current_power_strategy->set_device_power_policy(
          job_map_data->entries[i].data->node_power_profile_data[j]);
    }
  }
}

void update_power_distribution_strategy() {
  set_global_power_strategy(job_map_data, current_power_strategy,
                            global_power_budget);
  for (int i = 0; i < job_map_data->size; i++) {
    set_job_power_strategy(job_map_data->entries[i].data,
                           current_power_strategy,
                           job_map_data->entries[i].data->powerlimit);
    for (int j = 0; j < job_map_data->entries[i].data->num_of_nodes; j++) {
      set_node_power_strategy(
          job_map_data->entries[i].data->node_power_profile_data[j],
          current_power_strategy,
          job_map_data->entries[i]
              .data->node_power_profile_data[j]
              ->powerlimit);
    }
  }
}

void set_powercap() {

  for (int i = 0; i < job_map_data->size; i++) {
    job_data *job_data = job_map_data->entries[i].data;
    for (int j = 0; j < job_data->num_of_nodes; j++) {
      node_power_profile *node_data = job_data->node_power_profile_data[j];
      for (int k = 0; k < node_data->total_num_of_devices; k++) {
        device_power_profile *device_data = node_data->device_list[k];
        device_data->powercap =
            current_power_strategy->get_device_powercap(device_data);
      }
      // The function update_node_powercap set the powercap to the sum of its
      // devices powercap. Do this only when node powercap is not allowed.
      if (!node_data->powercap_allowed)
        update_node_powercap(node_data);
      else
        node_data->powercap =
            current_power_strategy->get_node_powercap(node_data);
    }
    update_job_powercap(job_data);
  }
}

int process_device_info(json_t *device_info, node_power_profile *node_data) {
  int index;
  json_t *device_value;

  json_array_foreach(device_info, index, device_value) {
    int device_id;
    device_type type;
    double powercap_set;
    device_power_profile *device_data;

    if (json_unpack(device_value, "{s:i s:i s:f}", "type", &type, "id",
                    &device_id, "pcap_set", &powercap_set) < 0) {
      log_message(NULL, "Unable to decode json of device info");
      return -1;
    }

    if ((device_data = find_device(node_data, type, device_id)) == NULL) {
      log_message(NULL, "Wrong deviceId found in JSON");
      return -1;
    }

    device_data->powercap = powercap_set;

    if (type == NODE) {
      // redistributing the power of devices based on node powercap;
      current_power_strategy->set_device_power_distribution(node_data,
                                                            powercap_set);
      // Updating the node powercap
      node_data->powercap = powercap_set;
    }
  }

  return 0; // Success
}

json_t *construct_payload(node_power_profile *node_data) {
  json_t *powercap_payload = json_array();

  for (int k = 0; k < node_data->total_num_of_devices; k++) {
    device_power_profile *d_data = node_data->device_list[k];
    if (!d_data)
      continue;

    // Only include devices where powercap is allowed
    if (!d_data->powercap_allowed)
      continue;

    json_t *device_payload;
    device_payload = json_pack("{s:i s:i s:f s:f}", "type", d_data->type, "id",
                               d_data->device_id, "c_p", d_data->powercap,
                               "m_p", d_data->powerlimit);

    if (!device_payload)
      continue;

    json_array_append_new(powercap_payload, device_payload);
  }

  return powercap_payload;
}

/**
 * This function takes the response from the powercap set RPC and parse the
 * response. The function is responsible for updating the device and node
 * powercaps as well as updating the device capabilities at first.
 *
 * The function also enables redistribution of power, if node powercap has benn
 * set.
 */

void handle_powercap_response(flux_future_t *f, void *args) {
  json_t *device_info;
  char *recv_hostname;
  uint64_t jobId;
  char *errmsg = "";
  job_data *job;
  node_power_profile *node_data;
  if (flux_rpc_get_unpack(f, "{s:I s:s s:O}", "jobId", &jobId, "hostname",
                          &recv_hostname, "device_array", &device_info) < 0) {
    errmsg = "Unable to unpack the response to flux RPC ";
    goto err;
  }

  if ((job = find_job(job_map_data, jobId)) == NULL) {
    errmsg = "The job is already finished";
    goto err;
  }

  if ((node_data = find_node(job, recv_hostname)) == NULL) {
    errmsg = "Unable to find the node with hostname ";
    goto err;
  }

  if (!process_device_info(device_info, node_data)) {
    errmsg = "Error processing device info";
    goto err;
  }

  flux_future_destroy(f);
  return;

err:
  log_error("ERROR:%s", errmsg);
  flux_future_destroy(f);
}
int set_powercap_all_level(double powercap, device_type type,
                           node_capabilities *capabilities) {
  if (!capabilities)
    return -1;
  double min_power, max_power;
  if (type == GPU) {
    min_power = capabilities->gpus.min_power;
    max_power = capabilities->gpus.max_power;
  } else if (type == NODE) {
    min_power = capabilities->node.min_power;
    max_power = capabilities->node.max_power;
  }

  if (powercap > min_power && powercap < max_power) {
    if (type == NODE) {
      log_message("POWERCAP:Setting powercap for node with powercap value %f",
                  powercap);
      return variorum_cap_best_effort_node_power_limit(powercap);
    }
    // else if (type == GPU)
    //   return variorum_cap_each_gpu_power_limit(powercap);
  }
  return 0; // Return 0 or an appropriate error code if no action is needed.
}
int set_node_level_powercap(node_power_profile *n_data) {
  if (n_data->powercap_allowed) {
    if (n_data->max_power > 0 && n_data->min_power > 0) {
      if (n_data->powercap > n_data->min_power &&
          n_data->powercap < n_data->max_power) {
        set_powercap_all_level(n_data->powercap, NODE,
                               &current_node_capabilities);
      }
    }
  }
  for (int i = 0; i < n_data->total_num_of_devices; i++) {
    if (n_data->device_list[i] == NULL)
      continue;
    device_power_profile *d_data = n_data->device_list[i];
    if (d_data->powercap_allowed) {
      if (set_powercap_all_level(d_data->powercap, d_data->type,
                                 &current_node_capabilities) < 0) {
        log_error("ERROR:Unable to set powercap for device for root node");
      }
    }
  }
  return 0;
}
/*
 * This function creates a Remote Procedure Call (RPC) for each node in every
 * job. The RPC contains information about all devices of a node.
 *
 * Initially, we instruct the system to set a power cap. If we are unaware of
 * a device's capabilities, we request that information as well.
 *
 * The function responsible for handling the RPC will then update and dispatch
 * the data appropriately.
 */
int send_powercap_rpc(flux_t *h, int rank, char *hostname) {
  for (int i = 0; i < job_map_data->size; i++) {
    job_data *job_data = job_map_data->entries[i].data;
    if (!job_data)
      return -1;

    for (int j = 0; j < job_data->num_of_nodes; j++) {
      node_power_profile *node_data = job_data->node_power_profile_data[j];
      if (!node_data)
        continue;

      int local_rank = find_rank_hostname(node_data->hostname);

      if (rank < 0)
        continue;
      if (local_rank == 0) {
        set_node_level_powercap(node_data);
        continue;
      }

      json_t *powercap_payload = construct_payload(node_data);
      if (!powercap_payload)
        continue;
      log_message("RPC_INFO: Sending powercap to %d, with node powercap %f",
                  rank, node_data->powercap);
      flux_future_t *f = flux_rpc_pack(
          h, "flux_pwr_manager.set_powercap", local_rank, FLUX_RPC_STREAMING,
          "{s:I s:s s:f s:f s:O}", "jobId", job_data->jobId, "hostname",
          node_data->hostname, "n_c_p", node_data->powercap, "n_m_p",
          node_data->powerlimit, "device_array", powercap_payload);
      if (!f) {
        log_message(
            "RPC_INFO:ERROR In sending RPC for powercap for hostname %s",
            node_data->hostname);
        json_decref(powercap_payload);
        return -1;
      }

      if (flux_future_then(f, -1., handle_powercap_response, NULL) < 0) {
        log_message("RPC_INFO:ERROR in setting flux_future_then for RPC "
                    "get_node_power");
        return -1;
      }
      json_decref(powercap_payload);
    }
  }
  return 0;
}

int process_device_array(json_t *device_array, json_t *device_info) {
  int index;
  json_t *device_data;

  json_array_foreach(device_array, index, device_data) {
    device_type type;
    int device_id;
    double powercap;
    double power_limit;
    json_t *new_device_info;

    if (json_unpack(device_data, "{s:i s:i s:f s:f}", "type", &type, "id",
                    &device_id, "c_p", &powercap, "m_p", &power_limit) < 0) {
      log_message("JSON:Error unpacking the device data from JSON");
      return -1; // Error
    }
    if (set_powercap_all_level(powercap, type, &current_node_capabilities) <
        0) {
      log_error("POWERCAP:Unable to set powercap for device %d", type);
      continue;
    }
    new_device_info = create_device_json(type, device_id, powercap);
    if (!new_device_info) {
      log_message("JSON:Error creating new device info JSON object");
      return 0; // Error
    }

    append_to_device_info(device_info, new_device_info);
  }

  return 1; // Success
}

/**
 *
 * This function sets the powercap using variorum API and then if necessary
 * calls the variorum domain_info API to get device_info. The info would
 * include whether powercapping is allowed on what devices on that node as
 * well as the max and min powercap. The response would also include whether
 * it was able to succesfully set powercap or not.
 */
void flux_pwr_manager_set_powercap_cb(flux_t *h, flux_msg_handler_t *mh,
                                      const flux_msg_t *msg, void *arg) {
  char *errmsg = "";
  uint64_t jobId;
  char *current_hostname;
  double node_current_powercap;
  json_t *device_array;
  json_t *device_info = json_array();
  double node_max_powerlimit = 0;

  if (flux_request_unpack(msg, NULL, "{s:I s:s s:f s:f s:O}", "jobId", &jobId,
                          "hostname", &current_hostname, "n_c_p",
                          &node_current_powercap, "n_m_p", &node_max_powerlimit,
                          "device_array", &device_array) < 0) {
    send_error(msg, "flux_request_unpack failed");
    return;
  }

  if (set_powercap_all_level(node_current_powercap, NODE,
                             &current_node_capabilities) != 0) {
    log_message("POWERCAP:Variorum set node power limit failed!\n");
    send_error(msg, "Variorum unable to set powercap for Node");
    return;
  }

  json_t *node_info = create_device_json(NODE, 0, node_current_powercap);
  if (!node_info) {
    send_error(msg, "Unable to encode node_info actual powercap");
    return;
  }
  append_to_device_info(device_info, node_info);

  // Assuming there's a process_device_array function that processes the
  // device_array
  if (!process_device_array(device_array, device_info)) {
    send_error(msg, "Error processing device array");
    return;
  }

  if (flux_respond_pack(h, msg, "{s:I s:s s:O}", "jobId", jobId, "hostname",
                        current_hostname, "device_array", device_info) < 0) {
    send_error(msg,
               "Unable to send RPC response to flux_pwr_manager.set_powercap");
  }

  json_decref(device_info);
}

void handle_get_node_power_rpc(flux_future_t *f, void *args) {
  uint64_t start_time, end_time;
  uint64_t jobId;
  json_t *array;
  if (flux_rpc_get_unpack(f, "{s:I s:I s:I s:O}", "start_time", &start_time,
                          "end_time", &end_time, "flux_jobId", &jobId, "data",
                          &array) < 0) {
    log_error("RPC_INFO:Unable to parse RPC data");
    return;
  }
  for (int i = 0; i < job_map_data->size; i++) {
    if (job_map_data->entries[i].jobId == jobId) {
      parse_power_payload(array, job_map_data->entries[i].data, end_time);
    }
  }
  flux_future_destroy(f);
}
// Use flux_pwr_monitor to get power_data for job nodes
void get_job_power(flux_t *h, job_data *job) {
  // Getting the current time
  struct timeval tv;
  flux_future_t *f;
  uint64_t current_time;
  gettimeofday(&tv, NULL);
  current_time = tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
  // Use the value which is larger, i.e, use t_depend of job or the start_time.
  //  Calculating start_time (current_time - 30sec)
  uint64_t start_time = current_time - 30 * 1e+9;
  if (job->t_depend > start_time)
    start_time = job->t_depend;
  // Creating the nodelist array
  json_t *nodelist = json_array();
  for (int i = 0; i < job->num_of_nodes; i++) {
    json_array_append_new(nodelist, json_string(job->node_hostname_list[i]));
  }
  if (!(f = flux_rpc_pack(h, "flux_pwr_monitor.get_node_power", 0,
                          FLUX_RPC_STREAMING, "{s:I s:I s:I s:o}", "start_time",
                          start_time, "end_time", current_time, "flux_jobId",
                          job->jobId, "nodelist", nodelist))) {
    json_decref(nodelist);
    log_message("RPC_INFO:Error in sending get job power Request");
    return;
  }
  // json_decref(nodelist);
  if (flux_future_then(f, -1., handle_get_node_power_rpc, NULL) < 0) {
    log_message(
        "RPC_INFO:Error in setting flux_future_then for RPC get_node_power");
    return;
  }
  // if (flux_reactor_run(flux_get_reactor(h), 0) < 0) {
  //   log_message(
  //            "Error in reactor for flux_get_reactor for RPC get_node_power");
  //   return;
  // }
}
void get_flux_jobs(flux_t *h) {
  flux_future_t *f;
  json_t *jobs;
  uint32_t userid;
  int states = 0;
  char *job_data_string;

  if (!(f = flux_rpc_pack(h, "job-list.list", FLUX_NODEID_ANY, 0,
                          "{s:i s:[s] s:i s:i s:i}", "max_entries", 100,
                          "attrs", "all", "userid", FLUX_USERID_UNKNOWN,
                          "states", FLUX_JOB_STATE_ACTIVE, "results", 0))) {
    log_message("RPC_INFO:Error in sending job-list Request");
    return;
  }

  if (flux_rpc_get_unpack(f, "{s:o}", "jobs", &jobs) < 0) {
    log_message("RPC_INFO:Error in unpacking job-list Request");
    return;
  }
  parse_jobs(h, jobs, job_map_data);

  // json_decref(
  //     jobs); // Decrement reference count of jobs after it's no longer needed

  flux_future_destroy(f);
}

void handle_get_node_power_capabilities_rpc(flux_future_t *f, void *args) {
  uint64_t jobId;
  char *hostname;
  json_t *devices_array;
  if (flux_rpc_get_unpack(f, "{s:I s:s s:O}", "jobId", &jobId, "hostname",
                          &hostname, "devices", &devices_array) < 0) {
    log_error("RPC_INFO:Unable to parse RPC data");
    return;
  }

  node_capabilities remote_capabilities = {0};
  size_t index;
  json_t *device_json;

  json_array_foreach(devices_array, index, device_json) {
    char *s1;
    int type_int = json_integer_value(json_object_get(device_json, "type"));
    device_type type = (device_type)type_int;
    if (type == GPU) {
      fill_device_capability_from_json(device_json, &remote_capabilities.gpus,
                                       type);
    } else if (type == CPU) {
      fill_device_capability_from_json(device_json, &remote_capabilities.cpus,
                                       type);
    } else if (type == MEM) {
      fill_device_capability_from_json(device_json, &remote_capabilities.mem,
                                       type);
    } else if (type == NODE) {
      fill_device_capability_from_json(device_json, &remote_capabilities.node,
                                       type);
    } else if (type == SOCKETS) {
      fill_device_capability_from_json(device_json,
                                       &remote_capabilities.sockets, type);
    }
  }
  job_data *j_data = find_job(job_map_data, jobId);
  // Update job data max power allowed.
  j_data->max_power += remote_capabilities.node.max_power;
  // Update job data min power.
  j_data->min_power += remote_capabilities.node.min_power;
  if (j_data == NULL)
    log_error(
        "Unable to find job when handling flux rpc for power capabilities");
  node_power_profile *n_data = find_node(j_data, hostname);
  if (n_data == NULL)
    log_error(
        "Unable to find node when handling flux rpc for power capabilities");
  // We dont have device info
  if (n_data->total_num_of_devices == 0) {
    int ret = node_device_list_init(n_data, &remote_capabilities, 100);
  }
  flux_future_destroy(f);
}

void node_cap_rpc(flux_t *h, uint64_t jobId, char *hostname) {

  flux_future_t *f;
  int rank = find_rank_hostname(hostname);
  if (!(f = flux_rpc_pack(h, "flux_pwr_manager.get_node_power_capabilities",
                          rank, 0, "{s:I s:s}", "jobId", jobId, "hostname",
                          hostname))) {
    log_message("RPC_INFO:Error in sending node_cap  Request");
    return;
  }

  if (flux_future_then(f, -1., handle_get_node_power_capabilities_rpc, NULL) <
      0) {
    log_message(
        "RPC_INFO:Error in setting flux_future_then for RPC get_node_power");
    return;
  }
}

// Helper function to handle variorum and parse capabilities
static int handle_variorum_and_parse(char *s, node_capabilities *node) {
  int ret = variorum_get_node_power_domain_info_json(&s);
  if (ret != 0) {
    return -1; // Variorum get domain power failed
  }

  if ((parse_node_capabilities(s, node)) < 0) {
    return -2; // Unable to parse variorum domain power info
  }
  return 0;
}
int add_node_capabiliity_to_job(flux_t *h, job_data *data) {
  if (data == NULL)
    return -1;

  int counter = 0; // Ensure it starts at 0 every time this function is called

  for (int i = 0; i < data->num_of_nodes; i++) {
    node_power_profile *n_data = data->node_power_profile_data[i];
    if (n_data == NULL)
      continue;
    // At start the number of devices is zero
    if (n_data->total_num_of_devices != 0) {
      counter++;
    } else {
      // Manually handle parsing.
      if (find_rank_hostname(n_data->hostname) == 0) {
        char *s = malloc(1500);
        if (!s) {
          return -1;
        }
        handle_variorum_and_parse(s, &current_node_capabilities);
        node_device_list_init(n_data, &current_node_capabilities, 100);
        data->max_power += current_node_capabilities.node.max_power;
        data->min_power += current_node_capabilities.node.min_power;
        free(s);
        counter++;
      } else {
        node_cap_rpc(h, data->jobId, n_data->hostname);
      }
    }
  }
  if (counter == data->num_of_nodes) {
    return 0;
  }
  return -1;
}
static void timer_handler(flux_reactor_t *r, flux_watcher_t *w, int revents,
                          void *arg) {
  if (rank == 0) {
    static int initialized = 0;

    int ret;
    char *s = malloc(1500);

    //   double node_power;
    //   double gpu_power;
    //   double cpu_power;
    //   double mem_power;
    //   flux_t *h = (flux_t *)arg;
    //   get_flux_jobs(h);
    //   for (int i = 0; i < job_map_data->size; i++) {
    //     int ret = add_node_capabiliity_to_job(h,
    //     job_map_data->entries[i].data); if (ret != 0) {
    //       log_message("Still Waiting for node Info");
    //       return;
    //     }
    //   }
    //   for (int i = 0; i < job_map_data->size; i++) {
    //     get_job_power(h, job_map_data->entries[i].data);
    //   }
    //   if (job_map_data->size > 0)
    //     initialized++;
    //   if (initialized % 3 == 0) {
    //     update_power_policy();
    //     update_power_distribution_strategy();
    //     set_powercap();
    //
    //     // Currently, the device capabilities are being retrieved during the
    //     first
    //     // powercap RPC, which disrupts the overall structure of power
    //     capping. At
    //     // present, a temporary workaround is being used to manage this issue
    //     // since the existing code structure doesn't allow RPC calls to be
    //     made
    //     // from other files. A refactor is planned for this section, which
    //     will
    //     // centralize all RPC calls into a single file. This will enable
    //     calls to
    //     // be made from any location, not just the main file, thereby
    //     improving
    //     // the flexibility and maintainability of the code.
    //     //
    //
    //     if (send_powercap_rpc(h, rank, node_hostname) < 0) {
    //       log_error("ERROR Unable to set powercap");
    //     }
    //   }
  }
}
void flux_pwr_manager_get_hostname_cb(flux_t *h, flux_msg_handler_t *mh,
                                      const flux_msg_t *msg, void *arg) {
  const char *hostname;
  uint32_t sender;
  char *errmsg = "";
  if (flux_request_unpack(msg, NULL, "{s:I s:s}", "rank", &sender, "hostname",
                          &hostname) < 0) {
    errmsg = "Unable to unpack hostname rpc";
    goto error;
  }
  // if (rank > 0) {
  //   flux_future_t *f;
  //
  //   if (!(f = (flux_rpc_pack(
  //             h,                               // flux_t *h
  //             "flux_pwr_manager.get_hostname", // char *topic
  //             FLUX_NODEID_UPSTREAM, // uint32_t nodeid (FLUX_NODEID_ANY,
  //             FLUX_RPC_NORESPONSE,  // int flags (FLUX_RPC_NORESPONSE,,
  //             "{s:I,s:s}", "rank", sender, "hostname", hostname)))) {
  //     errmsg = "Unable to forward hostname request";
  //     goto error;
  //   }
  //   flux_future_destroy(f);
  if (rank == 0) {
    hostname_list[sender] = strdup(hostname);
    if (hostname_list[sender] == NULL) {
      log_error("Error in copying string");
    }
  }

error:
  log_error(errmsg);
  if (flux_respond_error(
          h, msg, errno,
          "Error:unable to unpack flux_power_monitor.collect_power ") < 0)
    log_error("%s: flux_respond_error", __FUNCTION__);
}

/**
 * {
    "jobId": 12345,
    "hostname": "my-host",
    "devices": [
        {
            "type": "GPU",
            "count": 2,
            "power_capping": {"min": 10, "max": 50}:warn("%s");
        },
        {
            "type": "CPU",
            "count": 16,
            "power_capping": {"min": 5, "max": 30}
        },
        ...
    ]
}
 if power_capping contains both min and max as 0, we are assuming it does not
support power capping.
*/
void flux_pwr_manager_get_node_power_capabilities_cb(flux_t *h,
                                                     flux_msg_handler_t *mh,
                                                     const flux_msg_t *msg,
                                                     void *arg) {
  uint64_t jobId;
  char *recv_hostname;
  char *errmsg = "";
  char *s = malloc(1500);
  if (!s) {
    errmsg = "Memory allocation failed";
    goto err;
  }

  json_t *devices = json_array();

  if (flux_request_unpack(msg, NULL, "{s:I s:s}", "jobId", &jobId, "hostname",
                          &recv_hostname) < 0) {
    errmsg = "Unable to unpack RPC";
    goto err;
  }
  int variorum_result =
      handle_variorum_and_parse(s, &current_node_capabilities);
  if (variorum_result == -1) {
    errmsg = ("ERROR:Variorum get domain power failed");
    goto err;
  } else if (variorum_result == -2) {
    errmsg = ("ERROR:Unable to parse variorum domain power info");
    goto err;
  }
  // Append each device's info to the JSON
  append_single_device_to_json(&current_node_capabilities.gpus, devices);
  append_single_device_to_json(&current_node_capabilities.mem, devices);
  append_single_device_to_json(&current_node_capabilities.sockets, devices);
  append_single_device_to_json(&current_node_capabilities.cpus, devices);
  append_single_device_to_json(&current_node_capabilities.node, devices);

  if (flux_respond_pack(h, msg, "{s:I s:s s:O}", "jobId", jobId, "hostname",
                        recv_hostname, "devices", devices) < 0) {
    errmsg = ("Unable to reply for the get node power capabilities RPC");
    goto err;
  }

err:
  if (s != NULL) {
    free(s);
    s = NULL; // Ensure we don't double free
  }

  if (devices != NULL)
    json_decref(devices); // Free the devices JSON array

  if (flux_respond_error(h, msg, errno, errmsg) < 0)
    log_error("%s: flux_respond_error", __FUNCTION__);
}

void flux_pwr_manager_set_powerlimit_cb(flux_t *h, flux_msg_handler_t *mh,
                                        const flux_msg_t *msg, void *arg) {

  char *errmsg = "";
  uint64_t global_powerlimit;
  if (flux_request_unpack(msg, NULL, "{s:I}", "gl_pl", &global_powerlimit) <
      0) {
    errmsg = "Unable to unpack RPC";
    goto err;
  }
  if (rank == 0) {
    if (min_global_powerlimit < global_powerlimit < max_global_powerlimit) {
      global_power_budget = global_powerlimit;
    } else
      goto err;
  }
err:
  if (flux_respond_error(h, msg, errno, errmsg) < 0)
    log_error("%s: flux_respond_error", __FUNCTION__);
}

void flux_pwr_manager_set_power_strategy_cb(flux_t *h, flux_msg_handler_t *mh,
                                            const flux_msg_t *msg, void *arg) {

  char *errmsg = "";
  int power_strat;
  POWER_POLICY_TYPE type;

  if (flux_request_unpack(msg, NULL, "{s:i}", "power_strat", &power_strat) <
      0) {
    errmsg = "Unable to unpack RPC";
    goto err;
  }
  if (rank == 0) {
    type = (POWER_POLICY_TYPE)power_strat;
  }
  if (type == DYNAMIC_POWER)
    dynamic_power_policy_init(current_power_strategy);
  if (type == UNIFORM)
    uniform_power_policy_init(current_power_strategy);

err:
  if (flux_respond_error(h, msg, errno, errmsg) < 0)
    log_error("%s: flux_respond_error", __FUNCTION__);
}
int handle_node_job_notification(char *topic, uint64_t jobId, char *job_cwd,
                                 double power_budget, char *job_name) {
  static int i = 0;
  i++;
  if (topic == NULL)
    return -1;
  if (strcmp(topic, "job.state.run") == 0) {

    if (node_manager_new_job(jobId, job_cwd, job_name) < 0)
      return -1;
    log_message("powepcap setting status %d",
                node_manager_set_powerlimit(power_budget));
  } else if (strcmp(topic, "job.inactive-add") == 0) {
    if (node_manager_finish_job(jobId) < 0)
      return -1;
  }

  return 0;
}
int send_node_notify_rpc(char *topic, uint64_t jobId, char *hostname,
                         char *job_cwd, char *job_name) {
  int local_rank = 0;
  static int i = 0;
  if (hostname == NULL)
    return -1;
  if (topic == NULL)
    return -1;
  local_rank = find_rank_hostname(hostname);
  if (flux_rpc_pack(flux_handler, "flux_pwr_manager.jobtap_node_notify",
                    local_rank, 0, "{s:s s:I s:s s:f,s:s}", "topic", topic,
                    "id", jobId, "cwd", job_cwd, "pl",
                    global_power_budget / size, "name", job_name) < 0) {
    log_error("RPC_ERROR:Error in sending notification to node");
    return -1;
  }
  return 0;
}
void handle_jobtap_nodelist_rpc(flux_future_t *f, void *args) {
  char **job_hostname_list = NULL;
  int size = 0;
  char *topic = (char *)args;
  char *node_string;
  uint64_t id;
  char *job_cwd;
  char *job_name;

  if (flux_rpc_get_unpack(f, "{s:{s:I s:s s:s,s:s}}", "job", "id", &id,
                          "nodelist", &node_string, "cwd", &job_cwd, "name",
                          &job_name) < 0) {
    log_error("RPC_INFO:Unable to parse RPC data %s",
              flux_future_error_string(f));
    flux_future_destroy(f);
    return;
  }
  int ret = getNodeList((char *)node_string, &job_hostname_list, &size);
  if (ret < 0) {
    goto error;
  }
  for (int i = 0; i < size; i++)
    send_node_notify_rpc(topic, id, job_hostname_list[i], job_cwd, job_name);
error:
  for (int i = 0; i < size; i++) {
    free(job_hostname_list[i]);
  }
  free(job_hostname_list);
  flux_future_destroy(f);
  free(topic);
}

void flux_pwr_manager_job_notification_rpc_cb(flux_t *h, flux_msg_handler_t *mh,
                                              const flux_msg_t *msg,
                                              void *args) {
  flux_future_t *f;
  char *topic_ref;
  char *topic;
  uint64_t id;
  int userId;
  double t_submit;
  uint32_t state = FLUX_JOB_STATE_ACTIVE | FLUX_JOB_STATE_INACTIVE;

  if (flux_request_unpack(msg, NULL, "{s:s s:I s:f s:i}", "topic", &topic_ref,
                          "id", &id, "t_submit", &t_submit, "userId",
                          &userId) < 0) {
    log_error("Job Tap Giving error");
    return;
  }
  topic = strdup(topic_ref);

  json_t *attrs_array = json_array();
  if (!attrs_array) {
    free(topic);
    log_error("JSON_ERROR:Unable to create attrs array for nodelist");
    goto error;
  }

  if (json_array_append_new(attrs_array, json_string("nodelist")) == -1) {
    log_error("JSON_ERROR:Unable to append to attrs array");
    free(topic);
    goto error;
  }
  struct timespec req, rem;

  // Set the sleep time: 500 milliseconds
  // 1 millisecond = 1,000,000 nanoseconds
  req.tv_sec = 0;               // 0 seconds
  req.tv_nsec = 500 * 1000000L; // 500 million nanoseconds (500 milliseconds)

  // Sleep is necessary as run state does not have any data regarding job
  // nodelist
  if (nanosleep(&req, &rem) < 0) {
    log_error("Error in Sleep during flux_pwr_manager job_notify");
  }
  f = flux_rpc_pack(h, "job-list.list-id", FLUX_NODEID_ANY, 0,
                    "{s:I s:[s,s,s]}", "id", id, "attrs", "nodelist", "cwd",
                    "name");
  if (!f) {
    log_error("RPC_ERROR:Unable to send RPC to get job info in jobtap %d");
    free(topic);
    goto error;
  }
  if (flux_future_then(f, -1., handle_jobtap_nodelist_rpc, topic) < 0) {
    log_message(
        "RPC_INFO:Error in setting flux_future_then for RPC get_node_power");
    flux_future_destroy(f); // Clean up the future object
    goto error;
  }
error:
  if (attrs_array != NULL)
    json_decref(attrs_array);
}

void flux_pwr_manager_notify_node(flux_t *h, flux_msg_handler_t *mh,
                                  const flux_msg_t *msg, void *args) {
  uint64_t jobId;
  char *topic_ref;
  char *topic;
  char *job_cwd_ref;
  char *job_cwd;
  char *job_name_ref;
  char *job_name;
  double power_budget;
  if (flux_request_unpack(msg, NULL, "{s:s s:I s:s s:f,s:s}", "topic",
                          &topic_ref, "id", &jobId, "cwd", &job_cwd_ref, "pl",
                          &power_budget, "name", &job_name_ref) < 0)
    log_message("RPC_ERROR: unapck error notify node");
  topic = strdup(topic_ref);
  job_cwd = strdup(job_cwd_ref);
  job_name = strdup(job_name_ref);
  if (handle_node_job_notification(topic, jobId, job_cwd, power_budget,
                                   job_name) < 0)
    log_message("Error in setting node_manager job notification");
  free(topic);
  free(job_cwd);
  free(job_name);
}

void flux_pwr_manager_jobtap_destructor_cb(flux_t *h, flux_msg_handler_t *mh,
                                           const flux_msg_t *msg, void *args) {
  char **hostname_list;
  int size;
  bool terminated;
  if (flux_request_unpack(msg, NULL, "{s:b}", "terminated", terminated) < 0) {
    log_error("RPC_ERROR:Unpacking jobtap destructor");
  }

  // DO whatever we have to do when jobtap is down
  //
}
static const struct flux_msg_handler_spec htab[] = {
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_manager.set_powercap",
     flux_pwr_manager_set_powercap_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_manager.set_global_powerlimit",
     flux_pwr_manager_set_powerlimit_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_manager.set_power_strategy",
     flux_pwr_manager_set_power_strategy_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_manager.get_node_power_capabilities",
     flux_pwr_manager_get_node_power_capabilities_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_manager.get_hostname",
     flux_pwr_manager_get_hostname_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_manager.job_notify",
     flux_pwr_manager_job_notification_rpc_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_manager.jobtap_destructor_notify",
     flux_pwr_manager_jobtap_destructor_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_manager.jobtap_node_notify",
     flux_pwr_manager_notify_node, 0},
    FLUX_MSGHANDLER_TABLE_END,
};

int mod_main(flux_t *h, int argc, char **argv) {
  int rc = 0;
  flux_get_rank(h, &rank);
  flux_get_size(h, &size);

  init_flux_pwr_logging(h);
  flux_handler = h;
  if (size == 0)
    return -1;
  // log_message( "QQQ %s:%d Hello from rank %d of %d.\n", __FILE__,
  // __LINE__, rank, size);
  if (rank == 0) {
    current_power_strategy = malloc(sizeof(power_strategy));
    if (current_power_strategy == NULL) {

      printf("Error In allocating current power strategy");
      rc = -1;
      goto done;
    }

    dynamic_power_policy_init(current_power_strategy);
  }
  max_global_powerlimit = size * MAX_NODE_POWER;
  min_global_powerlimit = size * MIN_NODE_POWER;
  if (max_global_powerlimit == 0 || min_global_powerlimit == 0) {
    rc = -1;
    goto done;
  }
  global_power_budget = max_global_powerlimit;
  gethostname(node_hostname, HOSTNAME_SIZE);
  if (rank == 0) {
    if (hostname_list == NULL) {
      hostname_list = malloc(sizeof(char *) * size);
      if (hostname_list == NULL) {
        log_error("Unable to allocate memory for hostname_list");
        rc = -1;
        goto done;
      }
    }
    for (int i = 1; i < size; i++)
      hostname_list[i] = NULL;

    hostname_list[0] = strdup(node_hostname);
    log_message("node hostname %s", hostname_list[0]);
  }
  // We don't have easy access to the topology of the underlying flux network,
  // so we'll set up an overlay instead.
  // dag[UPSTREAM] = (rank == 0) ? -1 : rank / 2;
  // dag[DOWNSTREAM1] = (rank >= size / 2) ? -1 : rank * 2;
  // dag[DOWNSTREAM2] = (rank >= size / 2) ? -1 : rank * 2 + 1;
  // if (rank == size / 2 && size % 2) {
  //   // If we have an odd size then rank size/2 only gets a single child.
  //   dag[DOWNSTREAM2] = -1;
  // }
  if (rank == 0) {
    job_map_data = init_job_map(JOB_MAP_SIZE);
  }
  flux_msg_handler_t **handlers = NULL;
  // Let all ranks set this up.
  if (flux_msg_handler_addvec(h, htab, NULL, &handlers) < 0) {
    log_error("Flux msg_handler addvec error");
    rc = -1;
    goto done;
  }
  if (flux_rpc_pack(h,                               // flux_t *h
                    "flux_pwr_manager.get_hostname", // char *topic
                    0,                   // uint32_t nodeid (FLUX_NODEID_ANY,
                    FLUX_RPC_NORESPONSE, // int flags (FLUX_RPC_NORESPONSE,,
                    "{s:I s:s}", "rank", rank, "hostname", node_hostname) < 0) {
    log_error("RPC_ERROR:Unable to get hostname");
    rc = -1;
    goto done;
  }
  // All ranks set a handler for the timer.
  flux_watcher_t *timer_watch_p = flux_timer_watcher_create(
      flux_get_reactor(h), 10.0, 5.0, timer_handler, h);
  if (timer_watch_p == NULL) {
    rc = -1;
    goto done;
  }
  flux_watcher_start(timer_watch_p);
  log_message("POST execution 0");
  node_manager_init(h, rank, size, node_hostname, SAMPLING_RATE * BUFFER_SIZE,
                    SAMPLING_RATE);
  // node_manager_init(h, rank, size, node_hostname, 10,
  //                   SAMPLING_RATE);
  log_message("POST execution 1");

  // Run!
  if (flux_reactor_run(flux_get_reactor(h), 0) < 0) {
    log_error("FLUX_REACTOR_RUN");
    rc = -1;
    log_message("We are done");
    goto done;
  }
  log_message("POST execution 2");
done:
  // On unload, shutdown the handlers.
  if (rank == 0) {
    flux_msg_handler_delvec(handlers);
    if (job_map_data != NULL)
      dynamic_job_map_destroy(job_map_data);
    for (int i = 0; i < size; i++) {
      if (hostname_list[i] != NULL)
        free(hostname_list[i]);
    }
    printf("Freeing the hostname_list");
    free(hostname_list);
    // free(current_power_strategy);
  }
  node_manager_destructor();
  return rc;
}
MOD_NAME(MY_MOD_NAME);
