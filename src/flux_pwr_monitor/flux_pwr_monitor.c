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

static root_node_level_info **root_all_node_data;
static uint32_t sample_id = 0;
static uint32_t rank, size;
static uint32_t sampling_rate, buffer_size;
static size_t node_level_circular_buffer_size;

int write_data_to_root_node_info(flux_t *h, root_node_level_info *info,
                                 node_power_info *power_data) {
  if (info == NULL || power_data == NULL)
    return -1;

  // flux_log(h, LOG_CRIT,
  //          "ZERO: Writing data for sender:%d whose hostname is %s\n",
  //          info->rank, info->hostname);
  circular_buffer_push(info->power_data, (void *)power_data);
  return 0;
}
void flux_pwr_monitor_collect_power_cb(flux_t *h, flux_msg_handler_t *mh,
                                       const flux_msg_t *msg, void *arg) {
  static uint32_t _sample = 0;

  uint32_t sender, in_sample;

  const char *recv_from_hostname;
  char my_hostname[256];
  gethostname(my_hostname, 256);
  const char *s;
  // unpack the RPC request
  if (flux_request_unpack(msg, NULL, "{s:s,s:i,s:s}", "power_data", &s, "rank",
                          &sender, "hostname", &recv_from_hostname) < 0)
    goto error;
  // flux_log(h, LOG_CRIT,
  //          "INFO:I received flux_pwr_monitor.collect_power %s and my rank is
  //          %d and " "received data from %d. \n", s, rank, sender);
  if (s == NULL || recv_from_hostname == NULL) {
    flux_log_error(h, "Flux Unpack resulted in null items, exiting");
    return;
  }
  if (rank > 0) {
    // Non zero ranks forward the rpc cal to their parents until, it reaches
    // root.
    flux_future_t *f =
        flux_rpc_pack(h,                            // flux_t *h
                      "flux_pwr_mgr.collect_power", // char *topic

                      FLUX_NODEID_UPSTREAM, // uint32_t nodeid (FLUX_NODEID_ANY,
                      FLUX_RPC_NORESPONSE,  // int flags (FLUX_RPC_NORESPONSE,,
                      "{s:s,s:I,s:s}", "power_data", s, "rank", sender,
                      "hostname", recv_from_hostname);
    if (f == NULL)
      goto error;
    flux_future_destroy(f);
  } else if (rank == 0) { // Rank 0
    // flux_log(h, LOG_CRIT, "ZERO %s:%d Rank %d has sample=%d \
    //   node_value=%s and host %s .\n",
    //          __FILE__, __LINE__, rank, sample_id, s, recv_from_hostname);
    // Get timestamp
    if (root_all_node_data == NULL) {
      flux_log_error(h, "%s: Error:Root All node Data is not initalized ",
                     __FUNCTION__);
      return;
    }
    struct timeval tv;
    uint64_t ts;
    gettimeofday(&tv, NULL);
    ts = tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
    // Create Node_power_info object,  it store variorum result for a particular
    // node
    node_power_info *power_data =
        node_power_info_new(recv_from_hostname, s, ts);
    if (power_data == NULL) {
      flux_log_error(h, "%s: Error in Creating Node Power Info Object",
                     __FUNCTION__);
      return;
    }

    // Storing power Info data based on the original node's nodeId
    if (root_all_node_data[sender] == NULL) {
      root_all_node_data[sender] = root_node_data_new(
          sender, recv_from_hostname, node_level_circular_buffer_size,
          &node_power_info_destroy);

      if (root_all_node_data[sender] == NULL) {
        flux_log_error(h, "%s: Error in Creating Root Node Power Data",
                       __FUNCTION__);
        return;
      }
    }

    if (write_data_to_root_node_info(h, root_all_node_data[sender],
                                     power_data) < 0)

      flux_log_error(h, "Error in writing data for Node with rank:%d \n",
                     sender);
  }
error:
  if (flux_respond_error(
          h, msg, errno,
          "Error:unable to unpack flux_power_monitor.collect_power ") < 0)
    flux_log_error(h, "%s: flux_respond_error", __FUNCTION__);
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

  if (initialized <= 10000) {
    initialized++;
    flux_t *h = (flux_t *)arg;

    // Go off and take your measurement.
    ret = variorum_get_node_power_json(&s);

    if (ret == 0) {
      sample_id++;

      // flux_log(h, LOG_CRIT,
      //          "INFO: I am rank %d with power %s on sample %d and host %s "
      //          "parent_nodeId is :%d \n",
      //          rank, s, sample_id, my_hostname, FLUX_NODEID_UPSTREAM);
      //
      // Then....
      // Just send the message.  These ranks don't do any combining.
      if (rank > 0) {
        flux_future_t *f = flux_rpc_pack(
            h,                                // flux_t *h
            "flux_pwr_monitor.collect_power", // char *topic
            FLUX_NODEID_UPSTREAM, FLUX_RPC_NORESPONSE, "{s:s,s:I,s:s}",
            "power_data", s, "rank", rank, "hostname",
            my_hostname // int flags (FLUX_RPC_NORESPONSE,
        );

        if (f == NULL) {
          flux_log_error(h, "%s: flux_respond_error", __FUNCTION__);
          free(s);
          return;
        }

        flux_future_destroy(f);
      } else if (rank == 0) {
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

        if (root_all_node_data[rank] == NULL) {
          root_all_node_data[rank] = root_node_data_new(
              rank, my_hostname, node_level_circular_buffer_size,
              &node_power_info_destroy);

          if (root_all_node_data[rank] == NULL) {
            flux_log_error(h, "%s: Error in Creating Root Node Power Data",
                           __FUNCTION__);
            return;
          }
        }

        // flux_log(h, LOG_CRIT,
        //          "ZERO: Writing data for sender:%d whose hostname is %s\n",
        //          sender, recv_from_hostname);
        if (write_data_to_root_node_info(h, root_all_node_data[rank],
                                         power_data) < 0)

          flux_log_error(h, "Error in writing data for Node with rank:%d \n",
                         rank);
      }
    }
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

  for (uint32_t i = 0; i < size; i++) {

    root_node_level_info *node_info = root_all_node_data[i];
    if (node_info != NULL) {

      flux_log(h, LOG_CRIT,
               "Gettinf response power data for hostname %s and comapring it "
               "with stored data with hostname %s \n",
               hostname, node_info->hostname);
      if (strcmp(hostname, node_info->hostname) == 0) {
        response_power_data *power_data;
        power_data = get_agg_power_data(node_info->power_data, hostname,
                                        start_time, end_time, sampling_rate);
        if (power_data == NULL) {
          flux_log_error(h, "Unable to get aggregrate data for hostname: %s",
                         hostname);
          return NULL;
        }
        return power_data;
      }
    } else {

      flux_log(h, LOG_CRIT, "ZERO:No Node Info found for index: %d\n", i);
    }
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
    uint32_t start_time, end_time;
    json_t *node_list;
    json_t *node_hostname;
    size_t index;
    size_t num_nodes_data_present = 0;
    response_power_data **power_data_nodes;
    if (flux_request_unpack(msg, NULL, "{s:i,s:i,s:o}", "start_time",
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
    flux_log(h, LOG_CRIT,
             "Z:E:R:O Got request for get node power with start_time %d and "
             "endtime: %d\n",
             start_time, end_time);
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
            "Z:E:R:O flux_pwr_monitor.get RPC with start_time %d, end_time "
            "%d and host_name containing %s \n",

            start_time, end_time, hostname);
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
      json_t *data_obj =
          json_pack("{s:s,s:b,s:{s:f, s:f,s:f,s:f}}", "hostname",
                    power_data_nodes[i]->hostname, "full_data_present",
                    power_data_nodes[i]->full_data_present, "node_power_data",
                    "node_power", power_data_nodes[i]->agg_node_power,
                    "cpu_power", power_data_nodes[i]->agg_cpu_power,
                    "gpu_power", power_data_nodes[i]->agg_gpu_power,
                    "mem_power", power_data_nodes[i]->agg_mem_power);
      json_array_append_new(power_payload, data_obj);
    }

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

static const struct flux_msg_handler_spec htab[] = {
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_monitor.get_node_power",
     flux_pwr_monitor_get_node_power, 0},
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_monitor.collect_power",
     flux_pwr_monitor_collect_power_cb, 0},
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
    if (size != 0) {
      node_level_circular_buffer_size = (size_t)ceil(buffer_size / size);
    }
    root_all_node_data = malloc(sizeof(root_node_level_info *) * size);
    if (root_all_node_data == NULL)
      return 1;
    for (uint32_t i = 0; i < size; i++)
      root_all_node_data[i] = NULL;
  }
  flux_msg_handler_t **handlers = NULL;

  // Let all ranks set this up.
  assert(flux_msg_handler_addvec(h, htab, NULL, &handlers) >= 0);
  printf("buffer size is %d and sampling rate is %d, node: %ld and size is %d "
         "and my rank is %d \n",
         buffer_size, sampling_rate, node_level_circular_buffer_size, size,
         rank);
  flux_watcher_t *timer_watch_p = flux_timer_watcher_create(
      flux_get_reactor(h), 1.0, sampling_rate, timer_handler, h);
  assert(timer_watch_p);
  flux_watcher_start(timer_watch_p);

  // Run!
  assert(flux_reactor_run(flux_get_reactor(h), 0) >= 0);

  // On unload, shutdown the handlers.
  if (rank == 0) {
    for (uint32_t i = 0; i < size; i++) {
      printf("%d i is \n", i);
      if (root_all_node_data != NULL) {
        if (root_all_node_data[i] != NULL)
          root_node_level_info_destroy(root_all_node_data[i]);
        root_all_node_data[i] = NULL;
      }
    }
    free(root_all_node_data);
    flux_msg_handler_delvec(handlers);
  }

  return 0;
}
