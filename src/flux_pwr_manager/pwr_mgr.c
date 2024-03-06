#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "cluster_pwr_mgr.h"
#include "constants.h"
#include "flux_pwr_logging.h"
#include "json_utility.h"
#include "node_capabilities.h"
#include "node_manager/node_manager.h"
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
#define MY_MOD_NAME "pwr_mgr"
const char default_service_name[] = MY_MOD_NAME;
double global_power_budget;
uint64_t max_global_powerlimit;
uint64_t min_global_powerlimit;
char node_hostname[HOSTNAME_SIZE];
char **hostname_list;
uint32_t rank, size;
cluster_mgr_t *current_cluster_mgr;
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

static void timer_handler(flux_reactor_t *r, flux_watcher_t *w, int revents,
                          void *arg) {
  // Every time, get data into each job. This require calling cluster_mgr to collect data from
  // each job and in turn each job collect data for each node.
  
  if (rank == 0) {
  
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
    cluster_mgr_add_hostname(sender, (char *)hostname);
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
void handle_jobtap_nodelist_rpc(flux_future_t *f, void *args) {
  char **job_hostname_list = NULL;
  int size = 0;
  char *topic = (char *)args;
  char *node_string;
  uint64_t id;
  char *job_cwd;
  char *job_name;

  if (flux_rpc_get_unpack(f, "{s:{s:I s:s s:s s:s}}", "job", "id", &id,
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
  if (strcmp(topic, "job.state.run") == 0) {
    cluster_mgr_add_new_job(current_cluster_mgr, id, job_hostname_list, size,
                            job_cwd, job_name);
  } else if (strcmp(topic, "job.inactive-add") == 0) {
    cluster_mgr_remove_job(current_cluster_mgr, id);
  }
  // for (int i = 0; i < size; i++)
  //   send_node_notify_rpc(topic, id, job_hostname_list[i], job_cwd, job_name);
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

// void flux_pwr_manager_set_power_ratio_cb(flux_t *h, flux_msg_handler_t *mh,
//                                          const flux_msg_t *msg, void *args) {
//   uint64_t jobid;
//   int power_ratio;
//   if(flux_request_unpack(msg,NULL,"{s:I
//   s:i}","jobId",&jobid,"p_r",&power_ratio)<0){
//     goto error;
//   }
//
//   // flux_rpc_pack(h,)
//   error:
//
// }
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
    {FLUX_MSGTYPE_REQUEST, "pwr_mgr.set_global_powerlimit",
     flux_pwr_manager_set_powerlimit_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "pwr_mgr.nm-new_job", node_manager_new_job_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "pwr_mgr.nm-set_pl", node_manager_set_pl_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "pwr_mgr.nm-end_job", node_manager_end_job_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "pwr_mgr.cm-set_powerratio",
     cluster_mgr_set_power_ratio_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "pwr_mgr.cm-set_global_pl",
     cluster_mgr_set_global_powerlimit_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "pwr_mgr.get_hostname",
     flux_pwr_manager_get_hostname_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "pwr_mgr.job_notify",
     flux_pwr_manager_job_notification_rpc_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "pwr_mgr.jobtap_destructor_notify",
     flux_pwr_manager_jobtap_destructor_cb, 0},
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
    current_cluster_mgr = cluster_mgr_new(h, global_power_budget, size);
    if (hostname_list == NULL) {
      hostname_list = malloc(sizeof(char *) * size);
      if (hostname_list == NULL) {
        log_error("Unable to allocate memory for hostname_list");
        rc = -1;
        goto done;
      }
    }
    cluster_mgr_add_hostname(0, node_hostname);
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
  }
  flux_msg_handler_t **handlers = NULL;
  // Let all ranks set this up.
  if (flux_msg_handler_addvec(h, htab, NULL, &handlers) < 0) {
    log_error("Flux msg_handler addvec error");
    rc = -1;
    goto done;
  }
  if (flux_rpc_pack(h,                      // flux_t *h
                    "pwr_mgr.get_hostname", // char *topic
                    0,                      // uint32_t nodeid (FLUX_NODEID_ANY,
                    FLUX_RPC_NORESPONSE,    // int flags (FLUX_RPC_NORESPONSE,,
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
    cluster_mgr_destroy(&current_cluster_mgr);
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
