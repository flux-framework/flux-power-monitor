/* power-mgr.c */
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "circular_buffer.h"
#include "node_power_info.h"
#include "response_power_data.h"
#include "root_node_level_info.h"
#include "util.h"
#include "variorum.h"
#include <assert.h>
#include <flux/core.h>
#include <jansson.h>
#include <math.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>
#define HOSTNAME_SIZE 256
static circular_buffer_t *per_node_circular_buffer = NULL;
static uint32_t sample_id = 0;
static uint32_t rank, size;
static uint32_t sampling_rate, buffer_size;
static char node_hostname[HOSTNAME_SIZE];
static char **hostname_list;
#define MY_MOD_NAME "flux_pwr_monitor"
const char default_service_name[] = MY_MOD_NAME;

static void timer_handler(flux_reactor_t *r, flux_watcher_t *w, int revents,
                          void *arg) {

  int ret;
  char *s = malloc(1500);

  double node_power;
  double gpu_power;
  double cpu_power;
  double mem_power;

  flux_t *h = (flux_t *)arg;

  // Go off and take your measurement.
  ret = variorum_get_node_power_json(&s);
  // sprintf(
  //     s,
  //     "{\"host\": \"corona285\", \"timestamp\": 1687898983502846, "
  //     "\"power_node_watts\": 384.0, \"power_cpu_watts_socket_0\": 85.0, "
  //     "\"power_mem_watts_socket_0\": 23.0,
  //     \"power_gpu_watts_socket_0\": 66.0, "
  //     "\"power_cpu_watts_socket_1\": 77.0,\"power_mem_watts_socket_1\": 23.0,
  //     "
  //     "\"power_gpu_watts_socket_1\": 66.0}");
  if (ret == 0) {
    sample_id++;
    struct timeval tv;
    uint64_t ts;
    gettimeofday(&tv, NULL);
    ts = tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
    node_power_info *power_data = node_power_info_new(node_hostname, s, ts);
    if (power_data == NULL) {
      flux_log_error(h, "%s: Error in Creating Node Power Info Object",
                     __FUNCTION__);
      return;
    }

    if (per_node_circular_buffer == NULL) {
      per_node_circular_buffer =
          circular_buffer_new(buffer_size, &node_power_info_destroy);

      if (per_node_circular_buffer == NULL) {
        flux_log_error(h, "%s: Error in Creating Root Node Power Data",
                       __FUNCTION__);
        return;
      }
    }

    circular_buffer_push(per_node_circular_buffer, (void *)power_data);
  }
  free(s);
}
/**
 * This function tries to find the appropriate node from the list of
 *root_node_level_info. Iterate over all the nodes and see if any matches the
 *hostname provided.
 **/

response_power_data *get_response_power_data(flux_t *h, const char *hostname,
                                             uint64_t start_time,
                                             uint64_t end_time) {
  response_power_data *power_data = NULL;
  double agg_node_power, agg_cpu_power, agg_gpu_power, agg_mem_power;
  uint64_t start_t, end_t;
  int status;
  for (int i = 0; i < size; i++) {
    if (hostname_list[i] == NULL)
      continue;
    if (strcmp(hostname_list[i], hostname) == 0) {
      if (i > 0) {
        flux_future_t *f = flux_rpc_pack(
            h, "flux_pwr_monitor.request_power_data_from_node", i,
            FLUX_RPC_STREAMING, "{s:I,s:I,s:s}", "start_time", start_time,
            "end_time", end_time, "node_hostname", hostname);
        if (f == NULL)
          goto error;
        if (flux_rpc_get_unpack(f, "{s:f,s:f,s:f,s:f,s:I,s:I,s:i}", "n_p",
                                &agg_node_power, "c_p", &agg_cpu_power, "g_p",
                                &agg_gpu_power, "m_p", &agg_mem_power,
                                "r_stime", &start_t, "r_etime", &end_t, "d_p",
                                &status) < 0) {
          flux_log_error(h, "error in Unpacking request from children nodes");
          return NULL;
        }
        flux_future_destroy(f);
        power_data = response_power_data_new(hostname, start_time, end_time);
        power_data->data_presence = status;
        power_data->start_time = start_t;
        power_data->end_time = end_t;
        power_data->agg_cpu_power = agg_cpu_power;
        power_data->agg_node_power = agg_node_power;
        power_data->agg_gpu_power = agg_gpu_power;
        power_data->agg_mem_power = agg_mem_power;
        return power_data;
      } else if (i == 0) {
        power_data = get_agg_power_data(per_node_circular_buffer, node_hostname,
                                        start_time, end_time);
        if (power_data == NULL) {
          flux_log_error(h, "error responding to "
                            "flux_pwr_montior.get_node_power request");
          return NULL;
        }
        return power_data;
      }
    }
  }
error:
  if (flux_respond_error(
          h, NULL, errno,
          "Error:unable to unpack flux_power_monitor.collect_power ") < 0)
    flux_log_error(h, "%s: flux_respond_error", __FUNCTION__);
  return NULL;
}

void node_power_info_array_destroy(response_power_data **power_data_nodes,
                                   size_t num_nodes_data_present) {

  for (size_t i = 0; i < num_nodes_data_present; i++) {
    if (power_data_nodes[i] != NULL) {
      response_power_data_destroy(power_data_nodes[i]);
      power_data_nodes[i] = NULL;
    }
  }
  free(power_data_nodes);
}

void flux_pwr_monitor_get_node_power(flux_t *h, flux_msg_handler_t *mh,
                                     const flux_msg_t *msg, void *arg) {
  if (rank == 0) {
    uint64_t start_time, end_time, flux_job_id;
    json_t *node_list;
    json_t *node_hostname;
    size_t index;
    size_t num_nodes_data_present = 0;

    response_power_data **power_data_nodes;
    if (flux_request_unpack(msg, NULL, "{s:I,s:I,s:I,s:o}", "start_time",
                            &start_time, "end_time", &end_time, "flux_jobId",
                            &flux_job_id, "nodelist", &node_list) < 0) {
      flux_log_error(h, "error Unpacking get_node_power request from client");
      if (flux_respond_error(
              h, msg, errno,
              "error Unpacking get_node_power request from client") < 0)
        flux_log_error(h, "error Unpacking get_node_power request from client");
      return;
    }
    size_t node_list_size = json_array_size(node_list);
    power_data_nodes = malloc(sizeof(response_power_data *) * node_list_size);

    if (power_data_nodes == NULL) {
      flux_respond_error(h, msg, 0,
                         "FATAL:error creating response_power_data array");
      flux_log_error(h, "error creating response_power_data array");
    }
    for (size_t i = 0; i < node_list_size; i++) {
      power_data_nodes[i] = NULL;
    }
    json_array_foreach(node_list, index, node_hostname) {
      const char *hostname = json_string_value(node_hostname);
      if (hostname == NULL) {
        flux_log_error(h, "JSON parse error for the hostname at index %ld",
                       index);
      } else {
        response_power_data *power_data =
            get_response_power_data(h, hostname, start_time, end_time);

        if (power_data == NULL) {
          flux_log_error(h, "Error:  Unable to get agg power_data hostname:%s",
                         hostname);
        } else {
          power_data_nodes[num_nodes_data_present] = power_data;
          num_nodes_data_present++;
        }
      }
    }

    json_t *power_payload = json_array();
    for (int i = 0; i < num_nodes_data_present; i++) {
      json_t *data_obj = json_pack(
          "{s:s,s:s,s:{s:f, s:f,s:f,s:f,s:I,s:I}}", "hostname",
          power_data_nodes[i]->hostname, "data_presence",
          get_data_presence_string(power_data_nodes[i]->data_presence),
          "node_power_data", "node_power", power_data_nodes[i]->agg_node_power,
          "cpu_power", power_data_nodes[i]->agg_cpu_power, "gpu_power",
          power_data_nodes[i]->agg_gpu_power, "mem_power",
          power_data_nodes[i]->agg_mem_power, "result_start_time",
          power_data_nodes[i]->start_time, "result_end_time",
          power_data_nodes[i]->end_time);
      // "data_start_time", power_data_nodes[i]->start_time,
      // "data_end_time", power_data_nodes[i]->end_time);
      json_array_append_new(power_payload, data_obj);
    }
    /** The JSON response would be something like this:
    {start_time:requested_start_time,end_time:"{requested end time},data:
    [{hostname:"hostname","full_data_present":{whether data for time range is
    availabe or not}, node_power_data:{node_power:{Node power reported by
    variorum},cpu_power:{CPU power reported by variorum}}}}]
    **/
    if (flux_respond_pack(h, msg, "{s:I,s:I,s:I,s:O}", "start_time", start_time,
                          "end_time", end_time, "flux_jobId", flux_job_id,
                          "data", power_payload) < 0) {
      flux_log_error(h, "error sending output RPC to client");
      node_power_info_array_destroy(power_data_nodes, num_nodes_data_present);
      power_data_nodes = NULL;
      return;
    }
    json_decref(power_payload);
    node_power_info_array_destroy(power_data_nodes, num_nodes_data_present);
    power_data_nodes = NULL;
  }
}

/**
 * flux_pwr_monitor.get_node_power: can be said is the user facing API that
 *would be used by end client to get power data.
 *flux_pwr_monitor.collect_power: is the internal API used by nodes to
 *communicate with root.
 **/
void flux_pwr_monitor_request_power_data_from_node(flux_t *h,
                                                   flux_msg_handler_t *mh,
                                                   const flux_msg_t *msg,
                                                   void *arg) {

  uint64_t start_time, end_time;
  char *node_name_from_remote;
  size_t index;
  size_t num_nodes_data_present = 0;

  if (flux_request_unpack(msg, NULL, "{s:I,s:I,s:s}", "start_time", &start_time,
                          "end_time", &end_time, "node_hostname",
                          &node_name_from_remote) < 0) {
    flux_log_error(
        h, "error responding to flux_pwr_montior.get_node_power request");
    if (flux_respond_error(
            h, msg, errno,
            "error responding to flux_pwr_montior.get_node_power request") < 0)
      flux_log_error(
          h, "error responding to flux_pwr_montior.get_node_power request");
    return;
  }
  response_power_data *power_data = get_agg_power_data(
      per_node_circular_buffer, node_hostname, start_time, end_time);
  if (power_data == NULL) {
    flux_log_error(h, "Unable to get agg data from node for "
                      "flux_pwr_montior.request_power_data_from_node request");
    if (flux_respond_error(
            h, msg, errno,
            "error responding to flux_pwr_montior.request_power_data_from_node "
            "request") < 0)
      flux_log_error(h,
                     "error responding to "
                     "flux_pwr_montior.request_power_data_from_node request");
    return;
  }
  if (flux_respond_pack(
          h, msg, "{s:f,s:f,s:f,s:f,s:I,s:I,s:i}", "n_p",
          power_data->agg_node_power, "c_p", power_data->agg_cpu_power, "g_p",
          power_data->agg_gpu_power, "m_p", power_data->agg_mem_power,
          "r_stime", power_data->start_time, "r_etime", power_data->end_time,
          "d_p", (int)power_data->data_presence) < 0) {
    response_power_data_destroy(power_data);
    flux_log_error(
        h, "Error in RPC:flux_pwr_monitor_request_power_data_from_node");
    return;
  }
  response_power_data_destroy(power_data);
}
// Currently for decentralized version of the monitor. The request for data
// would contain a nodelist. Currently root has no inforamtion about the
// mapping between rank and hostnames. To map that relationship each node in
// the overlay tree sends an RPC containing its hostname to the root.
void flux_pwr_monitor_get_hostname(flux_t *h, flux_msg_handler_t *mh,
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

    {FLUX_MSGTYPE_REQUEST, "flux_pwr_monitor.get_hostname",
     flux_pwr_monitor_get_hostname, 0},
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_monitor.get_node_power",
     flux_pwr_monitor_get_node_power, 0},
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_monitor.request_power_data_from_node",
     flux_pwr_monitor_request_power_data_from_node, 0},
    FLUX_MSGHANDLER_TABLE_END,
};

int mod_main(flux_t *h, int argc, char **argv) {

  int opt;

  flux_get_rank(h, &rank);
  flux_get_size(h, &size);
  // All ranks set a handler for the timer.
  //
  if (size == 0)
    return 0;
  while ((opt = getopt(argc, argv, "s:r:")) !=
         -1) { // the colon after a letter means it requires an argument
    switch (opt) {
    case 's':
      buffer_size = atoi(optarg);
      printf("buffer_size %d \n", buffer_size);
      break;
    case 'r':
      sampling_rate = atof(optarg);
      break;
    default: /* '?' */
      fprintf(stderr, "Usage: %s [-s size] [-r sampling_rate]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }
  gethostname(node_hostname, HOSTNAME_SIZE);
  if (buffer_size == 0)
    buffer_size = 1000000;
  if (sampling_rate == 0)
    sampling_rate = 2;

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
  // Allocate the mapping array.
  flux_msg_handler_t **handlers = NULL;

  // Let all ranks set this up.
  assert(flux_msg_handler_addvec(h, htab, NULL, &handlers) >= 0);
  flux_rpc_pack(h,                               // flux_t *h
                "flux_pwr_monitor.get_hostname", // char *topic
                FLUX_NODEID_UPSTREAM, // uint32_t nodeid (FLUX_NODEID_ANY,
                FLUX_RPC_NORESPONSE,  // int flags (FLUX_RPC_NORESPONSE,,
                "{s:I,s:s}", "rank", rank, "hostname", node_hostname);
  flux_watcher_t *timer_watch_p = flux_timer_watcher_create(
      flux_get_reactor(h), 1.0, sampling_rate, timer_handler, h);
  assert(timer_watch_p);
  flux_watcher_start(timer_watch_p);

  // Run!
  assert(flux_reactor_run(flux_get_reactor(h), 0) >= 0);

  // On unload, shutdown the handlers.
  flux_msg_handler_delvec(handlers);
  if (rank == 0) {
    for (int i = 0; i < size; i++) {
      if (hostname_list[i] != NULL)
        free(hostname_list[i]);
    }
    free(hostname_list);
  }
  return 0;
}

MOD_NAME(MY_MOD_NAME);
