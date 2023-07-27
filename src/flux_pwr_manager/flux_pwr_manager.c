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
static dynamic_job_map *job_map_data;
const char default_service_name[] = MY_MOD_NAME;
static double global_power_budget;
static power_strategy *current_power_strategy;
static char node_hostname[HOSTNAME_SIZE];
static char **hostname_list;
static flux_t *flux_handle;
// static const int NOFLAGS=0;

/* Here's the algorithm:
 *
 * Ranks from size/2 .. size-1 send a message to dag[UPSTREAM].
 * Ranks from 1 .. (size/2)-1 listen for messages from ranks rank*2 and (if
 * needed) (rank*2)+1. They then combine the samples with their own sample and
 * forward the new message to dag[UPSTREAM]. Rank 0 listens for messages from
 * ranks 1 and 2, combines them as usually, then writes the summary out to the
 * kvs.
 *
 * To do this, all ranks take their measurements when the timer goes off.  Only
 * ranks size/2 .. size-1 then send a message. All of the other combining and
 * message sending happens in the message handler.
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
//
void update_powercap_for_device(job_data *data) {}
// void get_max_power(job_data* data){
//   for(int i=0;i<data->num_of_nodes;i++){
//     data->node_power_profile_data[i]->node_power_history
//   }
//
// }
// void get_min_power(job_data* data){
//
// return 50;
// }
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
int send_powercap_rpc(flux_t *h, int rank, char *hostname) {
  for (int i = 0; i < job_map_data->size; i++) {
    if (job_map_data->entries[i].data == NULL)
      return -1;
    job_data *job_data = job_map_data->entries[i].data;
    for (int j = 0; j < job_data->num_of_nodes; j++) {
      if (job_data->node_power_profile_data[j] == NULL)
        return -1;
      node_power_profile *node_data = job_data->node_power_profile_data[j];
      int rank = find_rank_hostname(node_data->hostname);
      if (rank < 0)
        continue;
      if (flux_rpc_pack(h, "flux_pwr_manager.set_powercap", rank, 0, "s:s,s:f",
                        "hostname", node_data->hostname, "node_powercap",
                        node_data->powercap) < 0) {
        flux_log_error(h, "ERROR In sending RPC for powercap for hostname %s",
                       node_data->hostname);
        return -1;
      }
    }
  }
  return 0;
}
void flux_pwr_manager_get_node_capability_cb(flux_t *h, flux_msg_handler_t *mh,
                                             const flux_msg_t *msg,
                                             void *args) {
  json_t *data;
  char *s = malloc(1500);
  double min_power, max_power;
  bool cpu, socket, node, mem, gpu;
  char my_hostname[256];
  char *rec_hostname;
  if (flux_request_unpack(msg, NULL, "{s:s}", "hostname", &rec_hostname) < 0) {
    goto error;
  }
  if (strcmp(my_hostname, rec_hostname) != 0) {
    goto error;
  }
  int ret = variorum_get_node_power_domain_info_json(&s);
  json_error_t error;

  data = json_loads(s, 0, &error);

  if (!data) {
    fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
    goto error;
  }

  if (!json_is_object(data)) {
    json_decref(data);
    goto error;
  }
  if (flux_respond_pack(h, msg, "{s:o}", "data", data) < 0) {
    goto error;
  }
  json_decref(data);
error:
  if (!data)
    json_decref(data);
  flux_log_error(h, "ERROR:");
}
void flux_pwr_manager_set_powercap_cb(flux_t *h, flux_msg_handler_t *mh,
                                      const flux_msg_t *msg, void *arg) {

  double power_cap;
  char *errmsg = "";
  int ret = 0;
  char *current_hostname;
  char myhostname[256];
  /*  Use flux_msg_get_payload(3) to get the raw data and size
   *   of the request payload.
   *  For JSON payloads, also see flux_request_unpack().
   */
  gethostname(myhostname, 256);
  if (flux_request_unpack(msg, NULL, "{s:s,s:f}", "hostname", &current_hostname,
                          "node_powercap", &power_cap) < 0) {
    flux_log_error(h, "flux_pwr_mgr_set_powercap_cb: flux_request_unpack");
    errmsg = "flux_request_unpack failed";
    goto err;
  }

  flux_log(h, LOG_CRIT,
           "I received flux_pwr_mgr.set_powercap message of %f W. \n",
           power_cap);
  if (strcmp(current_hostname, myhostname) == 0) {
    ret = variorum_cap_best_effort_node_power_limit(power_cap);
    if (ret != 0) {
      flux_log(h, LOG_CRIT, "Variorum set node power limit failed!\n");
      return;
    }
  }
  /*  Use flux_respond_raw(3) to include copy of payload in the response.
   *  For JSON payloads, see flux_respond_pack(3).
   */

  // We only get here if power capping succeeds.
  if (flux_respond_pack(h, msg, "{s:i}", "node", power_cap) < 0) {
    flux_log_error(h, "flux_pwr_mgr_set_powercap_cb: flux_respond_pack");
    errmsg = "flux_respond_pack failed";
    goto err;
  }
  return;
err:
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
    return;
  }
  print_test = json_dumps(array, 0);
  printf("Flux log data is %s", print_test);
  free(print_test);
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
  uint64_t current_time;
  gettimeofday(&tv, NULL);
  current_time = tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;

  // Calculating start_time (current_time - 30sec)
  uint64_t start_time = current_time - 30 * 1e+9;

  // Creating the nodelist array
  json_t *nodelist = json_array();
  for (int i = 0; i < job->num_of_nodes; i++) {
    json_array_append_new(nodelist, json_string(job->node_hostname_list[i]));
  }

  flux_future_t *f;
  if (!(f = flux_rpc_pack(h, "flux_pwr_monitor.get_node_power", 0, 0,
                          "{s:I,s:I,s:I, s:o}", "start_time", start_time,
                          "flux_jobId", job->jobId, "end_time", current_time,
                          "nodelist", nodelist))) {
    flux_log(h, LOG_CRIT, "Error in sending job-list Request");
    return;
  }
  if (flux_future_then(f, -1., handle_get_node_power_rpc, NULL) < 0) {
    flux_log(h, LOG_CRIT,
             "Error in setting flux_future_then for RPC get_node_power");
    return;
  }
  if (flux_reactor_run(flux_get_reactor(h), 0) < 0) {
    flux_log(h, LOG_CRIT,
             "Error in reactor for flux_get_reactor for RPC get_node_power");
    return;
  }
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

  job_data_string = json_dumps(jobs, JSON_INDENT(4));
  if (!job_data_string) {
    fprintf(stderr, "error: failed to serialize json\n");
    json_decref(jobs);
    return; // return here because job_data_string is NULL
  }

  flux_log(h, LOG_CRIT, "%s\n", job_data_string);
  free(job_data_string); // use free() instead of json_decref()

  parse_jobs(h, jobs, job_map_data);
  flux_log(h, LOG_CRIT, "Number of jobs after parsing is %ld",
           job_map_data->size);

  json_decref(
      jobs); // Decrement reference count of jobs after it's no longer needed

  flux_future_destroy(f);
}
int update_node_info_for_jobs(dynamic_job_map *job_map) {
  for (int i = 0; i < job_map->size; i++) {
    job_data *job_d = job_map->entries[i].data;
    if (job_d == NULL)
      continue;
    for (int j = 0; j < job_d->num_of_nodes; j++) {
      int rank_host =
          find_rank_hostname(job_d->node_power_profile_data[j]->hostname);
      if (rank_host < 0)
        continue;
      flux_future_t *f;
      if (!(f = flux_rpc_pack(flux_handle,
                              "flux_pwr_manager.get_node_capability", rank_host,
                              FLUX_RPC_STREAMING, "{s:s}", "hostname",
                              hostname_list[rank_host]))) {
        goto error;
      }
      json_t *data;
      if (flux_rpc_get_unpack(f, "{s:o}", "data", &data) < 0) {
        goto error;
      }
    }
  }
  return 0;
error:
  return -1;
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

    char my_hostname[256];
    gethostname(my_hostname, 256);

    flux_t *h = (flux_t *)arg;

    get_flux_jobs(h);
    update_node_info_for_jobs(job_map_data);
    flux_log(h, LOG_CRIT, "number of job %ld", job_map_data->size);
    for (int i = 0; i < job_map_data->size; i++) {
      get_job_power(h, job_map_data->entries[i].data);
    }
    update_power_distribution_strategy();
    update_power_policy();
    set_powercap();
    send_powercap_rpc(h, rank, my_hostname);
  }
}
// Currently for decentralized version of the monitor. The request for data
// would contain a nodelist. Currently root has no inforamtion about the
// mapping between rank and hostnames. To map that relationship each node in
// the overlay tree sends an RPC containing its hostname to the root.
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
                      "flux_pwr_monitor.get_hostname", // char *topic
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
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_manager.get_node_capability",
     flux_pwr_manager_get_node_capability_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_manager.get_host_name",
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
      printf("Error");
      return -1;
    }
    current_power_policy_init(current_power_strategy);
  }
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
                "flux_pwr_monitor.get_hostname", // char *topic
                FLUX_NODEID_UPSTREAM, // uint32_t nodeid (FLUX_NODEID_ANY,
                FLUX_RPC_NORESPONSE,  // int flags (FLUX_RPC_NORESPONSE,,
                "{s:I,s:s}", "rank", rank, "hostname", node_hostname);
  // All ranks set a handler for the timer.
  flux_watcher_t *timer_watch_p = flux_timer_watcher_create(
      flux_get_reactor(h), 1.0, 10.0, timer_handler, h);
  assert(timer_watch_p);
  flux_watcher_start(timer_watch_p);

  // Run!
  assert(flux_reactor_run(flux_get_reactor(h), 0) >= 0);

  // On unload, shutdown the handlers.
  if (rank == 0) {
    flux_msg_handler_delvec(handlers);
  }
  if (rank == 0) {
    if (job_map_data != NULL)
      dynamic_job_map_destroy(job_map_data);
  }
  return 0;
}

MOD_NAME(MY_MOD_NAME);
