#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "dynamic_job_map.h"
#include "job_data.h"
#include "job_power_util.h"
#include "power_policies/current_power_strategy.h"
#include "power_policies/power_policy.h"
#include "power_policies/power_policy_manager.h"
#include "power_util.h"
#include "util.h"
#include "variorum.h"
#include <assert.h>
#include <czmq.h>
#include <flux/core.h>
#include <jansson.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#define JOB_MAP_SIZE 100
#define HOSTNAME_SIZE 256
static uint32_t rank, size;
#define MY_MOD_NAME "flux_pwr_manager"
const char default_service_name[] = MY_MOD_NAME;
static double global_power_budget = 3250 * 4;
static power_strategy *current_power_strategy;
static char node_hostname[HOSTNAME_SIZE];
// This stores capability of each node.
static bool node_powercap = false;
static bool mem_powercap = false;
static bool socket_powercap = false;
static bool cpu_powercap = false;
static bool gpu_powercap = false;
// This is only used by zero Rank
static char **hostname_list;
static flux_t *flux_handle;
static dynamic_job_map *job_map_data;

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

enum {
  UPSTREAM = 0,
  DOWNSTREAM1,
  DOWNSTREAM2,
  MAX_NODE_EDGES,
  MAX_CHILDREN = 2,
};

static int dag[MAX_NODE_EDGES];

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
                           job_map_data->entries[i].data->max_powercap);
    for (int j = 0; j < job_map_data->entries[i].data->num_of_nodes; j++) {
      set_node_power_strategy(
          job_map_data->entries[i].data->node_power_profile_data[j],
          current_power_strategy,
          job_map_data->entries[i]
              .data->node_power_profile_data[j]
              ->max_powercap);
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
      update_node_powercap(node_data);
    }
    update_job_powercap(job_data);
  }
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
  bool req_info;
  json_t *device_info;
  char *recv_hostname;
  uint64_t jobId;
  char *errmsg = "";
  int index;
  job_data *job;
  node_power_profile *node_data;
  json_t *device_value;
  if (flux_rpc_get_unpack(f, "{s:I,s:s,s:b,s:o}", "jobId", &jobId, "hostname",
                          &recv_hostname, "req_info", &req_info, "device_array",
                          &device_info) < 0) {
    errmsg = "Unable to unpack the response to flux RPC ";
  }
  if ((job = find_job(job_map_data, jobId)) == NULL) {
    errmsg = "The job is already finished";
    goto err;
  }
  if ((node_data = find_node(job, recv_hostname)) == NULL) {
    errmsg = "Unable to find the node with hostname ";
    goto err;
  }

  json_array_foreach(device_info, index, device_value) {
    int device_id;
    device_type type;
    double max_power, min_power, powercap_set;
    device_power_profile *device_data;
    if (req_info) {
      if (json_unpack(device_value, "{s:i,s:i,s:f,s:f,s:f}", "type", &type,
                      "id", &device_id, "max_p", &max_power, "min_p",
                      &min_power, "pcap_set", &powercap_set) < 0) {
        errmsg = "Unable to decode json of device info";
        goto err;
      }

      if ((device_data = find_device(node_data, type, device_id)) == NULL) {
        errmsg = "Wrong deviceId found in JSON";
        goto err;
      }
      device_data->min_power = min_power;
      device_data->max_power = max_power;
      device_data->powercap = powercap_set;
    } else {
      if (json_unpack(device_value, "{s:i,s:i,s:f}", "type", &type, "id",
                      &device_id, "pcap_set", &powercap_set) < 0) {
        errmsg = "Unable to decode json of device info";
        goto err;
      }

      if ((device_data = find_device(node_data, type, device_id)) == NULL) {
        errmsg = "Wrong deviceId found in JSON";
        goto err;
      }
      device_data->powercap = powercap_set;
    }
    if (type == NODE) {
      // redestributing the power of devices based on node powercap;
      current_power_strategy->set_device_power_distribution(node_data,
                                                            powercap_set);
      // Updating the node powercap
      node_data->powercap = powercap_set;
    }
  }

  flux_future_destroy(f);
err:
  flux_log_error(flux_handle, "Error:%s", errmsg);
  flux_future_destroy(f);
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
    if (job_map_data->entries[i].data == NULL)
      return -1;
    job_data *job_data = job_map_data->entries[i].data;
    for (int j = 0; j < job_data->num_of_nodes; j++) {
      if (job_data->node_power_profile_data[j] == NULL)
        return -1;
      node_power_profile *node_data = job_data->node_power_profile_data[j];
      if (node_data == NULL)
        continue;
      int local_rank = find_rank_hostname(node_data->hostname);
      flux_log(h, LOG_CRIT, "Sending flux_set_powercap to the rank %d",
               local_rank);
      if (rank < 0)
        continue;
      json_t *powercap_payload = json_array();
      bool request_info = false;
      for (int k = 0; k < node_data->total_num_of_devices; k++) {
        device_power_profile *d_data = node_data->device_list[k];
        if (d_data == NULL)
          continue;
        if (!d_data->powercap_allowed)
          continue;
        json_t *device_payload;
        if (d_data->max_power == 0)
          request_info = true;

        device_payload = json_pack(
            "{s:i,s:i,s:f,s:f}", "type", d_data->type, "id", d_data->device_id,
            "c_p", d_data->powercap, "m_p", d_data->max_powercap);
        if (device_payload == NULL) {
          continue;
        }

        json_array_append_new(powercap_payload, device_payload);
      }
      char *test;
      test = json_dumps(powercap_payload, 0);
      flux_log(h, LOG_CRIT, "we are sending data for device %s", test);
      flux_future_t *f;
      if (!(f = flux_rpc_pack(h, "flux_pwr_manager.set_powercap", local_rank,
                              FLUX_RPC_STREAMING, "{s:I,s:s,s:b,s:o}", "jobId",
                              job_data->jobId, "hostname", node_data->hostname,
                              "req_info", request_info, "device_array",
                              powercap_payload))) {
        flux_log_error(h, "ERROR In sending RPC for powercap for hostname %s",
                       node_data->hostname);
        json_decref(powercap_payload);
        return -1;
      }
      json_decref(powercap_payload);
      if (flux_future_then(f, -1., handle_powercap_response, NULL) < 0) {
        flux_log(h, LOG_CRIT,
                 "Error in setting flux_future_then for RPC get_node_power");
        return -1;
      }
    }
  }
  return 0;
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

  bool request_info;
  char *errmsg = "";
  int ret = 0;
  char *current_hostname;
  char myhostname[256];
  gethostname(myhostname, 256);
  json_t *device_array;
  json_t *device_data;
  uint64_t jobId;
  int index;
  char *s = malloc(1500);
  json_t *device_info = json_array();
  if (flux_request_unpack(msg, NULL, "{s:I,s:s,s:b,s:o}", "jobId", &jobId,
                          "hostname", &current_hostname, "req_info",
                          &request_info, "device_array", &device_array) < 0) {
    flux_log_error(h, "flux_pwr_mgr_set_powercap_cb: flux_request_unpack");
    errmsg = "flux_request_unpack failed";
    goto err;
  }
  if (request_info) {
    int ret_info = variorum_get_node_power_domain_info_json(&s);
    flux_log(h, LOG_CRIT, "Result of variorum node_power_domain_info_json %s",
             s);
  }
  // Here the thought process is we are collecting data for all the devices we
  // are getting result for but only chaning power caps to devices where it is
  // possible.
  json_array_foreach(device_array, index, device_data) {
    // Right now hardcoding node capabilities to just have only one thing
    device_type type;
    int device_id;
    double powercap;
    double power_limit;
    json_t *new_device_info;
    if (json_unpack(device_data, "{s:i,s:i,s:f,s:f}", "type", &type, "id",
                    &device_id, "c_p", &powercap, "m_p", &power_limit) < 0) {
      flux_log(h, LOG_CRIT, "Some error in unpacking the json");

      continue;
    }
    if (type == NODE) {
      if (powercap > 1000 && powercap < 3250) {
        ret = variorum_cap_best_effort_node_power_limit(powercap);
        if (ret != 0) {
          errmsg = "Variorum unable to set powercap for Node";
          flux_log(h, LOG_CRIT, "Variorum set node power limit failed!\n");
          goto err;
        }
      }
    }
    if (!request_info) {
      if ((new_device_info = json_pack("{s:i,s:i,s:f}", "type", type, "id",
                                       device_id, "pcap_set", powercap)) ==
          NULL)
        continue;
    }
    if (request_info) {
      int powercap_set;
      double max_power, min_power;
      if (type == GPU) {
        max_power = 300;
        min_power = 125;
        powercap_set = -1;
      }
      if (type == NODE) {
        max_power = 3250;
        min_power = 1000;
        powercap_set = powercap;
      }

      new_device_info = json_pack("{s:i,s:i,s:f,s:f,s:f}", "type", type, "id",
                                  device_id, "max_p", max_power, "min_p",
                                  min_power, "pcap_set", powercap_set);
      if (new_device_info == NULL)
        continue;
    }
    json_array_append_new(device_info, new_device_info);
  }
  if (flux_respond_pack(h, msg, "{s:I,s:s,s:b,s:o}", "jobId", jobId, "hostname",
                        current_hostname, "req_info", request_info,
                        "device_array", device_info) < 0) {
    errmsg = "Unable to send RPC response to flux_pwr_manager.set_powercap";
    goto err;
  }
  json_decref(device_info);
  free(s);
  s = NULL;
  return;
err:
  if (s != NULL)
    free(s);
  json_decref(device_info);
  if (flux_respond_error(h, msg, errno, errmsg) < 0)
    flux_log_error(h, "flux_respond_error");
}

void handle_get_node_power_rpc(flux_future_t *f, void *args) {
  uint64_t start_time, end_time;
  uint64_t jobId;
  json_t *array;
  char *print_test;
  if (flux_rpc_get_unpack(f, "{s:I,s:I,s:I,s:O}", "start_time", &start_time,
                          "end_time", &end_time, "flux_jobId", &jobId, "data",
                          &array) < 0) {
    flux_log_error(flux_handle, "Unable to parse RPC data");
    return;
  }
  print_test = json_dumps(array, 0);
  printf("Flux log data is %s", print_test);
  free(print_test);
  for (int i = 0; i < job_map_data->size; i++) {
    if (job_map_data->entries[i].jobId == jobId) {
      flux_log(flux_handle, LOG_CRIT, "In the parse handle_get_node_power_rpc");
      parse_power_payload(array, job_map_data->entries[i].data, end_time);
    }
  }
  flux_future_destroy(f);
}
// Use flux_pwr_monitor to get power_data for job nodes
void get_job_power(flux_t *h, job_data *job) {
  // Getting the current time
  struct timeval tv;
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

  flux_future_t *f;
  if (!(f = flux_rpc_pack(h, "flux_pwr_monitor.get_node_power", 0, 0,
                          "{s:I,s:I,s:I,s:o}", "start_time", start_time,
                          "end_time", current_time, "flux_jobId", job->jobId,
                          "nodelist", nodelist))) {
    flux_log(h, LOG_CRIT, "Error in sending job-list Request");
    return;
  }
  flux_log(h, LOG_CRIT, "DONE 1 with jobId %ld", job->jobId);
  if (flux_future_then(f, -1., handle_get_node_power_rpc, NULL) < 0) {
    flux_log(h, LOG_CRIT,
             "Error in setting flux_future_then for RPC get_node_power");
    return;
  }
  // if (flux_reactor_run(flux_get_reactor(h), 0) < 0) {
  //   flux_log(h, LOG_CRIT,
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
    flux_log(h, LOG_CRIT, "Error in sending job-list Request");
    return;
  }

  if (flux_rpc_get_unpack(f, "{s:o}", "jobs", &jobs) < 0) {
    flux_log(h, LOG_CRIT, "Error in unpacking job-list Request");
    return;
  }

  if (!job_data_string) {
    fprintf(stderr, "error: failed to serialize json\n");
    json_decref(jobs);
    return; // return here because job_data_string is NULL
  }

  parse_jobs(h, jobs, job_map_data);
  flux_log(h, LOG_CRIT, "Number of jobs after parsing is %ld",
           job_map_data->size);

  json_decref(
      jobs); // Decrement reference count of jobs after it's no longer needed

  flux_future_destroy(f);
}
static void timer_handler(flux_reactor_t *r, flux_watcher_t *w, int revents,
                          void *arg) {
  if (rank == 0) {
    static int initialized = 0;

    int ret;
    char *s = malloc(1500);

    double node_power;
    double gpu_power;
    double cpu_power;
    double mem_power;
    flux_t *h = (flux_t *)arg;
    get_flux_jobs(h);
    flux_log(h, LOG_CRIT, "number of job %ld", job_map_data->size);
    for (int i = 0; i < job_map_data->size; i++) {
      get_job_power(h, job_map_data->entries[i].data);
    }
    if (job_map_data->size > 0)
      initialized++;
    flux_log(h, LOG_CRIT, "Initalized is %d", initialized);
    if (initialized % 3 == 0) {
      update_power_policy();
      update_power_distribution_strategy();
      set_powercap();

      // Currently, the device capabilities are being retrieved during the first
      // powercap RPC, which disrupts the overall structure of power capping. At
      // present, a temporary workaround is being used to manage this issue
      // since the existing code structure doesn't allow RPC calls to be made
      // from other files. A refactor is planned for this section, which will
      // centralize all RPC calls into a single file. This will enable calls to
      // be made from any location, not just the main file, thereby improving
      // the flexibility and maintainability of the code.
      //

      if (send_powercap_rpc(h, rank, node_hostname) < 0) {
        flux_log_error(h, "ERROR Unable to set powercap");
      }
    }
  }
}
void flux_pwr_manager_get_hostname(flux_t *h, flux_msg_handler_t *mh,
                                   const flux_msg_t *msg, void *arg) {
  const char *hostname;
  uint32_t sender;
  if (flux_request_unpack(msg, NULL, "{s:I,s:s}", "rank", &sender, "hostname",
                          &hostname) < 0)
    goto error;
  if (rank > 0) {
    flux_future_t *f =
        flux_rpc_pack(h,                               // flux_t *h
                      "flux_pwr_manager.get_hostname", // char *topic
                      FLUX_NODEID_UPSTREAM, // uint32_t nodeid (FLUX_NODEID_ANY,
                      FLUX_RPC_NORESPONSE,  // int flags (FLUX_RPC_NORESPONSE,,
                      "{s:I,s:s}", "rank", sender, "hostname", hostname);
    if (f == NULL)
      goto error;
    flux_future_destroy(f);
  } else if (rank == 0) {
    hostname_list[sender] = strdup(hostname);
    if (hostname_list[sender] == NULL) {
      flux_log_error(h, "Error in copying string");
    }
  }

error:
  flux_log_error(h, "Unable to unpack hostname request");
  if (flux_respond_error(
          h, msg, errno,
          "Error:unable to unpack flux_power_monitor.collect_power ") < 0)
    flux_log_error(h, "%s: flux_respond_error", __FUNCTION__);
}
static const struct flux_msg_handler_spec htab[] = {
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_manager.set_powercap",
     flux_pwr_manager_set_powercap_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_manager.get_hostname",
     flux_pwr_manager_get_hostname, 0},
    FLUX_MSGHANDLER_TABLE_END,
};

int mod_main(flux_t *h, int argc, char **argv) {

  flux_get_rank(h, &rank);
  flux_get_size(h, &size);

  // flux_log(h, LOG_CRIT, "QQQ %s:%d Hello from rank %d of %d.\n", __FILE__,
  // __LINE__, rank, size);
  if (rank == 0) {
    current_power_strategy = malloc(sizeof(power_strategy));
    if (current_power_strategy == NULL) {
      printf("Error In allocating current power strategy");
      return -1;
    }
    current_power_policy_init(current_power_strategy);
  }
  gethostname(node_hostname, HOSTNAME_SIZE);
  if (rank == 0) {
    if (hostname_list == NULL) {
      hostname_list = malloc(sizeof(char *) * size);
      if (hostname_list == NULL) {
        flux_log_error(h, "Unable to allocate memory for hostname_list");
        return -1;
      }
    }
    for (int i = 1; i < size; i++)
      hostname_list[i] = NULL;

    hostname_list[0] = strdup(node_hostname);
  }

  // We don't have easy access to the topology of the underlying flux network,
  // so we'll set up an overlay instead.
  dag[UPSTREAM] = (rank == 0) ? -1 : rank / 2;
  dag[DOWNSTREAM1] = (rank >= size / 2) ? -1 : rank * 2;
  dag[DOWNSTREAM2] = (rank >= size / 2) ? -1 : rank * 2 + 1;
  if (rank == size / 2 && size % 2) {
    // If we have an odd size then rank size/2 only gets a single child.
    dag[DOWNSTREAM2] = -1;
  }
  if (rank == 0) {
    job_map_data = init_job_map(JOB_MAP_SIZE);
  }
  flux_msg_handler_t **handlers = NULL;
  flux_handle = h;
  // Let all ranks set this up.
  assert(flux_msg_handler_addvec(h, htab, NULL, &handlers) >= 0);
  flux_rpc_pack(h,                               // flux_t *h
                "flux_pwr_manager.get_hostname", // char *topic
                FLUX_NODEID_UPSTREAM, // uint32_t nodeid (FLUX_NODEID_ANY,
                FLUX_RPC_NORESPONSE,  // int flags (FLUX_RPC_NORESPONSE,,
                "{s:I,s:s}", "rank", rank, "hostname", node_hostname);
  // All ranks set a handler for the timer.
  flux_watcher_t *timer_watch_p = flux_timer_watcher_create(
      flux_get_reactor(h), 5.0, 5.0, timer_handler, h);
  assert(timer_watch_p);
  flux_watcher_start(timer_watch_p);

  // Run!
  assert(flux_reactor_run(flux_get_reactor(h), 0) >= 0);

  // On unload, shutdown the handlers.
  if (rank == 0) {
    flux_msg_handler_delvec(handlers);
    if (job_map_data != NULL)
      dynamic_job_map_destroy(job_map_data);
    for (int i = 0; i < size; i++) {
      if (hostname_list[i] != NULL)
        free(hostname_list[i]);
    }
    free(hostname_list);
    // free(current_power_strategy);
  }
  return 0;
}

MOD_NAME(MY_MOD_NAME);
