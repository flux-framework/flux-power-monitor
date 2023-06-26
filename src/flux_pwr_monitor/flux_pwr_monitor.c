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

static circular_buffer_t *per_node_circular_buffer = NULL;
static uint32_t sample_id = 0;
static uint32_t rank, size;
static uint32_t sampling_rate, buffer_size;
static char node_hostname[256];
static char **hostname_list;

static void timer_handler(flux_reactor_t *r, flux_watcher_t *w, int revents,
                          void *arg) {

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
  if (ret == 0) {
    sample_id++;
    struct timeval tv;
    uint64_t ts;
    gettimeofday(&tv, NULL);
    ts = tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
    node_power_info *power_data = node_power_info_new(my_hostname, s, ts);
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

  circular_buffer_t *buffer_data = per_node_circular_buffer;
  if (buffer_data != NULL) {

    response_power_data *power_data;
    power_data =
        get_agg_power_data(buffer_data, hostname, start_time, end_time);
    if (power_data == NULL) {
      flux_log_error(h, "Unable to get aggregrate data for hostname: %s",
                     hostname);
      return NULL;
    }
    return power_data;
  } else {
    flux_log(h, LOG_CRIT, "ZERO:No Circular Buffer found for hostname:%s",
             hostname);
  }
  flux_log_error(h, "Data not present for hostname: %s", hostname);
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
  printf("Test\n");

  flux_log(h, LOG_CRIT, "Z:E:R:O:My rank: %d\n", rank);
  if (rank == 0) {
    uint64_t start_time, end_time;
    json_t *node_list;
    json_t *node_hostname;
    size_t index;
    size_t num_nodes_data_present = 0;

    response_power_data **power_data_nodes;
    if (flux_request_unpack(msg, NULL, "{s:I,s:I,s:o}", "start_time",
                            &start_time, "end_time", &end_time, "nodelist",
                            &node_list) < 0) {
      flux_log_error(
          h, "error responding to flux_pwr_montior.get_node_power request");
      if (flux_respond_error(
              h, msg, errno,
              "error responding to flux_pwr_montior.get_node_power request") <
          0)
        flux_log_error(
            h, "error responding to flux_pwr_montior.get_node_power request");
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
        flux_log(
            h, LOG_CRIT,
            "Z:E:R:O flux_pwr_monitor.get RPC with start_time %ld, end_time "
            "%ld and host_name containing %s \n",

            start_time, end_time, hostname);
        for (int i = ; i < node_list_size; i++) {

        }
        response_power_data *power_data =
            get_response_power_data(h, hostname, start_time, end_time);
        if (power_data != NULL) {

          power_data_nodes[num_nodes_data_present] = power_data;
          num_nodes_data_present++;
        }
      }
    }
    flux_log(h, LOG_CRIT, "Z:E:R:O:Power data nodes : %ld\n",
             num_nodes_data_present);

    json_t *power_payload = json_array();
    for (int i = 0; i < num_nodes_data_present; i++) {
      flux_log(h, LOG_CRIT,
               "Z:E:R:O:Power data nodes start_time: %ld end_time %ld\n",
               power_data_nodes[i]->start_time, power_data_nodes[i]->end_time);
      json_t *data_obj =
          json_pack("{s:s,s:b,s:{s:f, s:f,s:f,s:f,s:I,s:I}}", "hostname",
                    power_data_nodes[i]->hostname, "full_data_present",
                    power_data_nodes[i]->full_data_present, "node_power_data",
                    "node_power", power_data_nodes[i]->agg_node_power,
                    "cpu_power", power_data_nodes[i]->agg_cpu_power,
                    "gpu_power", power_data_nodes[i]->agg_gpu_power,
                    "mem_power", power_data_nodes[i]->agg_mem_power,
                    "result_start_time", power_data_nodes[i]->start_time,
                    "result_end_time", power_data_nodes[i]->end_time);
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
    if (flux_respond_pack(h, msg, "{s:I,s:I,s:O}", "start_time", start_time,
                          "end_time", end_time, "data", power_payload) < 0) {
      flux_log_error(
          h, "error responding to flux_pwr_montior.get_node_power request");
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
 *would be used by end client to get power data. flux_pwr_monitor.collect_power:
 *is the internal API used by nodes to communicate with root.
 **/

void flux_pwr_monitor_request_power_data_from_node(flux_t *h,
                                                   flux_msg_handler_t *mh,
                                                   const flux_msg_t *msg,
                                                   void *arg) {

  uint64_t start_time, end_time;
  json_t *node_list;
  size_t index;
  size_t num_nodes_data_present = 0;

  if (flux_request_unpack(msg, NULL, "{s:I,s:I,s:s}", "start_time", &start_time,
                          "end_time", &end_time, "nodelist", &node_list) < 0) {
    flux_log_error(
        h, "error responding to flux_pwr_montior.get_node_power request");
    if (flux_respond_error(
            h, msg, errno,
            "error responding to flux_pwr_montior.get_node_power request") < 0)
      flux_log_error(
          h, "error responding to flux_pwr_montior.get_node_power request");
    return;
  }

  response_power_data *power_data =
      get_response_power_data(h, node_hostname, start_time, end_time);
  if (power_data == NULL) {
    if (flux_respond_error(
            h, msg, errno,
            "error responding to flux_pwr_montior.get_node_power request") < 0)
      flux_log_error(
          h, "error responding to flux_pwr_montior.get_node_power request");
    return;
  }
  flux_log(h, LOG_CRIT, "Z:E:R:O:Power data nodes : %ld\n",
           num_nodes_data_present);

  if (flux_respond_pack(
          h, msg, "{s:f, s:f,s:f,s:f,s:I,s:I}", "n_p",
          power_data->agg_node_power, "c_p", power_data->agg_cpu_power, "g_p",
          power_data->agg_gpu_power, "m_p", power_data->agg_mem_power,
          "r_stime", power_data->start_time, "r_etime",
          power_data->end_time) < 0) {

    flux_log_error(
        h, "error responding to flux_pwr_montior.get_node_power request");
    return;
  }
}
static const struct flux_msg_handler_spec htab[] = {
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_monitor.get_node_power",
     flux_pwr_monitor_get_node_power, 0},
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_monitor.get_host_name",
     flux_pwr_monitor_request_power_data_from_node, 0},
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
      buffer_size = strtoull(optarg, NULL, 10);
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

  if (buffer_size == 0)
    buffer_size = 1000000;
  if (sampling_rate == 0)
    sampling_rate = 5;

  if (rank == 0) {
    if (hostname_list == NULL) {
      hostname_list = malloc(sizeof(char *) * size);
      if (hostname_list == NULL) {
        flux_log_error(h, "Unable to allocate memory for hostname_list");
        return -1;
      }
    }
  }

  flux_msg_handler_t **handlers = NULL;

  // Let all ranks set this up.
  assert(flux_msg_handler_addvec(h, htab, NULL, &handlers) >= 0);
  flux_watcher_t *timer_watch_p = flux_timer_watcher_create(
      flux_get_reactor(h), 1.0, sampling_rate, timer_handler, h);
  assert(timer_watch_p);
  flux_watcher_start(timer_watch_p);

  // Run!
  assert(flux_reactor_run(flux_get_reactor(h), 0) >= 0);

  // On unload, shutdown the handlers.
  flux_msg_handler_delvec(handlers);

  return 0;
}
