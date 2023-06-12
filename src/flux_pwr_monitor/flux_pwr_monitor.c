/* power-mgr.c */
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "circular_buffer.h"
#include "node_power_info.h"
#include "variorum.h"
#include <assert.h>
#include <flux/core.h>
#include <jansson.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>
static uint32_t rank, size;
static uint32_t sampling_rate, buffer_size;
#define MY_MOD_NAME "flux_pwr_monitor"
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

// enum {
//   UPSTREAM = 0,
//   DOWNSTREAM1,
//   DOWNSTREAM2,
//   MAX_NODE_EDGES,
//   MAX_CHILDREN = 2,
// };
static circular_buffer_t *circular_buffer_data;

static uint32_t sample_id = 0; // sample counter

static double node_power_acc = 0.0; // Node power accumulator
static double gpu_power_acc = 0.0;  // GPU power accumulator
static double cpu_power_acc = 0.0;  // CPU power accumulator
static double mem_power_acc = 0.0;  // Memory power accumulator

void flux_pwr_mtr_collect_power_cb(flux_t *h, flux_msg_handler_t *mh,
                                   const flux_msg_t *msg, void *arg) {
  static uint32_t _sample = 0;
  static double _node_value = 0;
  static double _gpu_value = 0;
  static double _cpu_value = 0;
  static double _mem_value = 0;

  uint32_t sender, in_sample;
  double temp_1, temp_2, temp_3, temp_4;

  const char *recv_from_hostname;
  char my_hostname[256];
  gethostname(my_hostname, 256);
  const char *s;

  // Crack open the message and store off what we need.
  // assert(flux_request_unpack(msg, NULL,
  // "{s:i s:i s:s s:f s:f s:f s:f}",
  // "sender", &sender, "sample", &in_sample,
  // "hostname", &recv_from_hostname, "node_power",
  // &temp_1, "gpu_power", &temp_2, "cpu_power",
  // &temp_3, "mem_power", &temp_4
  // S
  // ) >= 0);
  assert((flux_request_decode(msg, NULL, &s)) > 0);

  // _sample[sender % 2] = in_sample;
  // _node_value = temp_1;
  // _gpu_value = temp_2;
  // _cpu_value = temp_3;
  // _mem_value = temp_4;
  //
  // flux_log(h, LOG_CRIT, "QQQ %s:%d Rank %d received
  // flux_pwr_mgr.collect_power message from %d sample=%d node value=%lf and
  // host %s.\n",
  //		__FILE__, __LINE__, rank, sender, in_sample, temp_1,
  // recv_from_hostname);

  // We have both samples, combine with ours and push upstream.
  if (rank > 0) {
    flux_future_t *f =
        flux_rpc(h,                            // flux_t *h
                 "flux_pwr_mgr.collect_power", // char *topic
                 s,
                 FLUX_NODEID_UPSTREAM, // uint32_t nodeid (FLUX_NODEID_ANY,
                                       // FLUX_NODEID_UPSTREAM, or a flux
                                       // instance rank)
                 FLUX_RPC_NORESPONSE   // int flags (FLUX_RPC_NORESPONSE,
                                       // FLUX_RPC_STREAMING, or NOFLAGS)
                 // "{s:i s:i s:s s:f s:f s:f s:f}", "sender", rank, "sample",
                 // sample_id, "hostname", my_hostname, "node_power",
                 // node_power_acc, "gpu_power", gpu_power_acc, "cpu_power",
                 // cpu_power_acc, "mem_power", mem_power_acc

        );
    assert(f);
    flux_future_destroy(f);
  } else { // Rank 0
    flux_log(h, LOG_CRIT, "ZERO %s:%d Rank %d has sample=%d \
      node_value=%lf and host %s .\n",
             __FILE__, __LINE__, rank, sample_id, _node_value, my_hostname);
    struct timeval tv;
    uint64_t ts;
    gettimeofday(&tv, NULL);
    ts = tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
    node_power_info *power_data = node_power_info_new(my_hostname, s, ts);
    circular_buffer_push(circular_buffer_data, (void *)power_data);
  }
}

// If Rank is 0, create the KVS transaction, now that we have the value.
// if (rank == 0) {
// }

static void timer_handler(flux_reactor_t *r, flux_watcher_t *w, int revents,
                          void *arg) {

  static int initialized = 0;

  int ret;
  char *s = malloc(1500);
  json_t *power_obj = json_object();

  double node_power;
  double gpu_power;
  double cpu_power;
  double mem_power;

  char my_hostname[256];
  gethostname(my_hostname, 256);

  if (initialized <= 1000) {
    initialized++;
    flux_t *h = (flux_t *)arg;

    // Go off and take your measurement.
    ret = variorum_get_node_power_json(&s);
    if (ret != 0)
      //   flux_log(h, LOG_CRIT, "JSON: get node power failed!\n");
      //
      // power_obj = json_loads(s, JSON_DECODE_ANY, NULL);
      // node_power =
      //     json_real_value(json_object_get(power_obj, "power_node_watts"));
      // gpu_power =
      //     json_real_value(json_object_get(power_obj,
      //     "power_gpu_watts_socket_0"));
      // + json_real_value(json_object_get(power_obj,
      // "power_gpu_watts_socket_1"))
      // ;
      // cpu_power =
      //     json_real_value(json_object_get(power_obj,
      //     "power_cpu_watts_socket_0"));
      // + json_real_value(json_object_get(power_obj,
      // "power_cpu_watts_socket_1"))
      // ;
      // mem_power =
      //     json_real_value(json_object_get(power_obj,
      //     "power_mem_watts_socket_0"));
      // + json_real_value(json_object_get(power_obj,
      // "power_mem_watts_socket_1"))
      // ;

      // Instantaneous power values at this sample, no accumulation yet.
      // node_power_acc = node_power;
      // gpu_power_acc = gpu_power;
      // cpu_power_acc = cpu_power;
      // mem_power_acc = mem_power;

      // All ranks increment their sample id?
      sample_id++;

    flux_log(
        h, LOG_CRIT,
        "INFO: I am rank %d with node power %lf on sample %d and host %s\n",
        rank, node_power_acc, sample_id, my_hostname);

    // Then....
    // Just send the message.  These ranks don't do any combining.
    // flux_log(h, LOG_CRIT, "!!! %s:%d LEAF rank %d (size=%d) on host %s.\n",
    // __FILE__, __LINE__, rank, size, hostname);
    flux_future_t *f =
        flux_rpc(h,                            // flux_t *h
                 "flux_pwr_mgr.collect_power", // char *topic
                 s,
                 FLUX_NODEID_UPSTREAM, // uint32_t nodeid (FLUX_NODEID_ANY,
                                       // FLUX_NODEID_UPSTREAM, or a flux
                                       // instance rank)
                 FLUX_RPC_NORESPONSE   // int flags (FLUX_RPC_NORESPONSE,
                                       // FLUX_RPC_STREAMING, or NOFLAGS)
                 // "{s:i s:i s:s s:f s:f s:f s:f}", "sender", rank, "sample",
                 // sample_id, "hostname", my_hostname, "node_power",
                 // node_power_acc, "gpu_power", gpu_power_acc, "cpu_power",
                 // cpu_power_acc, "mem_power", mem_power_acc

        );
    assert(f);
    flux_future_destroy(f);
  }

  // Clean up JSON obj
  free(s);
}
void flux_pwr_mtr_get_node_power(flux_t *h, flux_msg_handler_t *mh,
                                 const flux_msg_t *msg, void *arg) {
  if (rank == 0) {
  }
}

static const struct flux_msg_handler_spec htab[] = {
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_mtr.get_node_power",
     flux_pwr_mtr_get_node_power, 0},
    {FLUX_MSGTYPE_REQUEST, "flux_pwr_mtr.collect_power",
     flux_pwr_mtr_collect_power_cb, 0},
    FLUX_MSGHANDLER_TABLE_END,
};

void initalize_circular_buffer(size_t buffer_size) {
  circular_buffer_data =
      circular_buffer_new(buffer_size, &node_power_info_destroy);
}
int mod_main(flux_t *h, int argc, char **argv) {

  int opt;

  flux_get_rank(h, &rank);
  flux_get_size(h, &size);
  if (argc != 2) {
    return -1;
  }

  // flux_log(h, LOG_CRIT, "QQQ %s:%d Hello from rank %d of %d.\n", __FILE__,
  // __LINE__, rank, size);

  // We don't have easy access to the topology of the underlying flux network,
  // so we'll set up an overlay instead.
  // dag[UPSTREAM] = (rank == 0) ? -1 : rank / 2;
  // dag[DOWNSTREAM1] = (rank >= size / 2) ? -1 : rank * 2;
  // dag[DOWNSTREAM2] = (rank >= size / 2) ? -1 : rank * 2 + 1;
  // if (rank == size / 2 && size % 2) {
  // If we have an odd size then rank size/2 only gets a single child.
  //   dag[DOWNSTREAM2] = -1;
  // }

  flux_msg_handler_t **handlers = NULL;

  // Let all ranks set this up.
  assert(flux_msg_handler_addvec(h, htab, NULL, &handlers) >= 0);
  // All ranks set a handler for the timer.
  //
  while ((opt = getopt(argc, argv, "s:r:")) !=
         -1) { // the colon after a letter means it requires an argument
    switch (opt) {
    case 's':
      buffer_size = atoi(optarg);
      break;
    case 'r':
      sampling_rate = atof(optarg);
      break;
    default: /* '?' */
      fprintf(stderr, "Usage: %s [-s size] [-r sampling_rate]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }
  flux_watcher_t *timer_watch_p = flux_timer_watcher_create(
      flux_get_reactor(h), 1.0, sampling_rate, timer_handler, h);
  assert(timer_watch_p);
  flux_watcher_start(timer_watch_p);

  // Run!
  assert(flux_reactor_run(flux_get_reactor(h), 0) >= 0);

  // On unload, shutdown the handlers.
  if (rank == 0) {
    flux_msg_handler_delvec(handlers);
    circular_buffer_destroy(circular_buffer_data);
  }

  return 0;
}

MOD_NAME(MY_MOD_NAME);
