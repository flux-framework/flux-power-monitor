#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "dynamic_job_map.h"
#include "job_data.h"
#include "util.h"
#include "variorum.h"
#include <assert.h>
#include <czmq.h>
#include <flux/core.h>
#include <jansson.h>
#include <stdint.h>
#include <unistd.h>

static job_data **job_data_list;
static uint32_t rank, size;
#define MY_MOD_NAME "flux_pwr_manager"
static dynamic_job_map* job_map_data;
const char default_service_name[] = MY_MOD_NAME;
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

void free_resources(char **node_hostname_list, int size,
                    job_data **job_data_list, int jobs_list_size) {
  for (int i = 0; i < size; i++) {
    free(node_hostname_list[i]);
  }
  free(node_hostname_list);
  for (int j = 0; j < jobs_list_size; j++) {
    job_data_destroy(job_data_list[j]);
  }
  free(job_data_list);
}

job_map_entry *find_job(job_map_entry *job_map, const char *jobId,
                        size_t job_map_size) {
  for (size_t i = 0; i < job_map_size; i++) {
    if (strcmp(job_map[i].jobId, jobId) == 0) {
      return &job_map[i];
    }
  }
  return NULL;
}

void handle_new_job(json_t *value, dynamic_job_map *job_map, flux_t *h) {
  char **node_hostname_list = NULL;
  int size = 0;

  json_t *nodelist = json_object_get(value, "nodelist");
  json_t *jobId_json = json_object_get(value, "jobId");

  if (!json_is_string(nodelist) || !json_is_string(jobId_json)) {
    flux_log(h, LOG_CRIT, "Unable get nodeList or jobId from job");
    return;
  }

  const char *str = json_string_value(nodelist);
  const char *jobId = json_string_value(jobId_json);
  if (!str || !jobId) {
    flux_log(h, LOG_CRIT, "Error in sending job-list or jobId Request");
    return;
  }

  getNodeList((char *)str, &node_hostname_list, &size);

  job_map_entry job_entry = {
      .jobId = strdup(jobId),
      .data = job_data_new((char *)jobId, node_hostname_list, size)};
  add_to_job_map(job_map, job_entry);

  for (int i = 0; i < size; i++) {
    free(node_hostname_list[i]);
  }
  free(node_hostname_list);
}

void parse_jobs(json_t *jobs, flux_t *h, dynamic_job_map *job_map) {
  size_t index;
  json_t *value;

  // Create a new job map
  dynamic_job_map *new_job_map = init_job_map(100);

  // Now handle each new job and add them into new_job_map
  json_array_foreach(jobs, index, value) {
    handle_new_job(value, new_job_map, h);
  }

  // Free the memory used by the old job_map
  for (size_t i = 0; i < job_map->size; i++) {
    free(job_map->entries[i].jobId);
    job_data_destroy(job_map->entries[i].data);
  }
  free(job_map->entries);
  free(job_map);

  // Assign the new_job_map to job_map
  job_map = new_job_map;
}

void flux_pwr_mgr_set_powercap_cb(flux_t *h, flux_msg_handler_t *mh,
                                  const flux_msg_t *msg, void *arg) {

  int power_cap;
  char *errmsg = "";
  int ret = 0;

  /*  Use flux_msg_get_payload(3) to get the raw data and size
   *   of the request payload.
   *  For JSON payloads, also see flux_request_unpack().
   */
  if (flux_request_unpack(msg, NULL, "{s:i}", "node", &power_cap) < 0) {
    flux_log_error(h, "flux_pwr_mgr_set_powercap_cb: flux_request_unpack");
    errmsg = "flux_request_unpack failed";
    goto err;
  }

  flux_log(h, LOG_CRIT,
           "I received flux_pwr_mgr.set_powercap message of %d W. \n",
           power_cap);

  ret = variorum_cap_best_effort_node_power_limit(power_cap);
  if (ret != 0) {
    flux_log(h, LOG_CRIT, "Variorum set node power limit failed!\n");
    return;
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

void get_flux_jobs(flux_t *h) {
  flux_future_t *f;
  json_t *jobs;
  uint32_t userid;
  int states = 0;
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
  job_map_data=init_job_map(100);
  parse_jobs(jobs, h,job_map_data);
  json_decref(jobs);
  flux_future_destroy(f);
}
static void timer_handler(flux_reactor_t *r, flux_watcher_t *w, int revents,
                          void *arg) {

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

  // Go off and take your measurement.
  ret = variorum_get_node_power_json(&s);
  if (ret != 0)
    flux_log(h, LOG_CRIT, "JSON: get node power failed!\n");
  printf("%s\n", s);
  get_flux_jobs(h);
}
static const struct flux_msg_handler_spec htab[] = {
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_mgr.set_powercap",
     flux_pwr_mgr_set_powercap_cb, 0},
    FLUX_MSGHANDLER_TABLE_END,
};

int mod_main(flux_t *h, int argc, char **argv) {

  flux_get_rank(h, &rank);
  flux_get_size(h, &size);

  // flux_log(h, LOG_CRIT, "QQQ %s:%d Hello from rank %d of %d.\n", __FILE__,
  // __LINE__, rank, size);

  // We don't have easy access to the topology of the underlying flux network,
  // so we'll set up an overlay instead.
  dag[UPSTREAM] = (rank == 0) ? -1 : rank / 2;
  dag[DOWNSTREAM1] = (rank >= size / 2) ? -1 : rank * 2;
  dag[DOWNSTREAM2] = (rank >= size / 2) ? -1 : rank * 2 + 1;
  if (rank == size / 2 && size % 2) {
    // If we have an odd size then rank size/2 only gets a single child.
    dag[DOWNSTREAM2] = -1;
  }

  flux_msg_handler_t **handlers = NULL;

  // Let all ranks set this up.
  assert(flux_msg_handler_addvec(h, htab, NULL, &handlers) >= 0);

  // All ranks set a handler for the timer.
  flux_watcher_t *timer_watch_p = flux_timer_watcher_create(
      flux_get_reactor(h), 2.0, 2.0, timer_handler, h);
  assert(timer_watch_p);
  flux_watcher_start(timer_watch_p);

  // Run!
  assert(flux_reactor_run(flux_get_reactor(h), 0) >= 0);

  // On unload, shutdown the handlers.
  if (rank == 0) {
    flux_msg_handler_delvec(handlers);
  }

  return 0;
}

MOD_NAME(MY_MOD_NAME);
