#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "circular_buffer.h"
#include "jansson.h"
#include "node_data.h"
#include "node_util.h"
#include "power_monitor.h"
#include "response_power_data.h"
#include "root_node_level_info.h"
#include "flux_pwr_logging.h"
#include "util.h"
#include "variorum.h"
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#define MAX_FILENAME_SIZE 256
#define SAMPLING_RATE 100000000L
pthread_mutex_t file_mutex;
pthread_cond_t file_cond;           // file is ready
pthread_cond_t write_complete_cond; // Writing is done
bool write_to_file =
    false; // Flag to check whether we should write data when buffer is full.
bool job_running = false; // Indicate whether job is running or not. Useful as
                          // file needs to be closed with job.
bool global_file_write = true; // Global flag to control file writing.
int circular_buffer_marker =
    0; // Marker denotes how much data is needed to fill the buffer for a
       // parituclar job. -1 means no job running.
char output_filename[MAX_FILENAME_SIZE] = "default_output.csv";
FILE *current_file = NULL;
//*****These two variable are always modified before file write thread is
// called, so it should be thread safe.****
char *global_temp_buffer = NULL;
size_t global_temp_buffer_size = 0;

void reset_temp_buffer(char *global_temp_buffer,
                       size_t global_temp_buffer_size) {

  memset(global_temp_buffer, 0, global_temp_buffer_size); // Reset the}
}

void free_global_buffer() {
  free(global_temp_buffer);
  global_temp_buffer = NULL;
  global_temp_buffer_size = 0;
}

// Callback function for concatenating csv strings
void concatenate_csv_callback(void *data, void *user_data) {
  node_power *np = (node_power *)data;
  char **temp_buffer = (char **)user_data;

  // Concatenate the fixed-size string
  strcat(*temp_buffer, np->csv_string);
}

void update_temp_buffer(circular_buffer_t *buffer, size_t start_index,
                        size_t end_index) {
  if (!global_temp_buffer) {
    fprintf(stderr, "Global buffer is not allocated\n");
    return;
  }

  circular_buffer_iterate_partial(buffer, concatenate_csv_callback, &global_temp_buffer,
                                  start_index, end_index);
}

void add_data_to_buffer(circular_buffer_t *buffer) {
  struct timespec req, rem;
  req.tv_sec = 0;
  req.tv_nsec = SAMPLING_RATE; // 5 milliseconds
  while (true) {

    int ret = 0;
    char *s = malloc(1500);
    if (s == NULL) {
      nanosleep(&req, &rem);
      continue;
    }
    ret = variorum_get_node_power_json(&s);
    if (!ret) {
      nanosleep(&req, &rem);
      free(s);
      continue;
    }
    node_power *p_data = parse_string(s);
    free(s);
    if (p_data == NULL) {
      nanosleep(&req, &rem);
      continue;
    }
    // The last value is the marker.
    if (circular_buffer_marker == 1) {
      update_temp_buffer(node_power_data->node_power_time, 0, 0);
      circular_buffer_marker =
          circular_buffer_get_max_size(node_power_data->node_power_time);
      // Enable write to file.
      pthread_mutex_lock(&file_mutex);
      write_to_file = true;
      pthread_cond_signal(&file_cond);
      pthread_mutex_unlock(&file_mutex);
    }
    pthread_mutex_lock(&node_power_data->node_power_time->mutex);
    circular_buffer_push(node_power_data->node_power_time, p_data);
    pthread_mutex_unlock(&node_power_data->node_power_time->mutex);
    if (circular_buffer_marker !=
        -1) // Indicating we dont need to update circular marker
      // BUG: Check for correct value of circular_buffer_marker
      circular_buffer_marker--;
  }
}
void write_header(FILE *file) {
  const char *header =
      "\"timestamp\", \"power_node_watts\","
      "\"power_cpu_watts_socket_0\", \"power_mem_watts_socket_0\", "
      " \"power_gpu_watts_socket_0\", "
      "\"power_cpu_watts_socket_1\", \"power_gpu_watts_socket_1\","
      " \"power_mem_watts_socket_1\"\n";
  fputs(header, file);
}

void write_data_to_file() {

  if (current_file != NULL) {
    fputs(global_temp_buffer, current_file);
    reset_temp_buffer(global_temp_buffer, global_temp_buffer_size);
  }
}
void *file_write_thread(void *arg) {
  while (true) {
    pthread_mutex_lock(&file_mutex);
    while (!write_to_file && !global_file_write) {
      pthread_cond_wait(&file_cond, &file_mutex);
    }
    if (job_running) {
      if (current_file == NULL) {
        current_file = fopen(output_filename, "a");
        if (current_file == NULL) {
          pthread_mutex_unlock(&file_mutex);
          continue;
        }
        write_header(current_file);
      }
      write_data_to_file();
      write_to_file = false; // Make sure the next time, the cv is used.
    } else if (!job_running) {
      if (current_file == NULL) {
        current_file = fopen(output_filename, "a");
        write_header(current_file);
      }
      write_data_to_file();
      fclose(current_file);
      current_file = NULL;
      write_to_file = false;
      pthread_cond_signal(
          &write_complete_cond); // Signal that file writing is complete
    }
    pthread_mutex_unlock(&file_mutex);
  }
  return NULL;
}

void start_job(uint64_t jobId) {

  circular_buffer_marker =
      circular_buffer_get_max_size(node_power_data->node_power_time);
  pthread_mutex_lock(&file_mutex);
  snprintf(output_filename, MAX_FILENAME_SIZE, "%lu_%s.csv", jobId,
           node_power_data->hostname); // Use actual hostname
  job_running = true;
  pthread_mutex_unlock(&file_mutex);
}

void end_job() {
  // Copy the buffer so that it can be written.
  update_temp_buffer(
      node_power_data->node_power_time, circular_buffer_marker,
      circular_buffer_get_max_size(node_power_data->node_power_time));
  circular_buffer_marker = -1; // Reset The circular buffer marker
  pthread_mutex_lock(&file_mutex);
  write_to_file = true;
  job_running = false;
  pthread_cond_signal(&file_cond);
  pthread_cond_wait(&write_complete_cond, &file_mutex);
  pthread_mutex_unlock(&file_mutex);
}

void destroy_pthread_component(){
  pthread_mutex_destroy(&file_mutex);
  pthread_cond_destroy(&file_cond);
  pthread_cond_destroy(&write_complete_cond);
  destroy_pthread_component();
}
void power_monitor_init() {
  global_file_write = true;
  allocate_global_buffer(
      global_temp_buffer, &global_temp_buffer_size,
      circular_buffer_get_max_size(node_power_data->node_power_time)-1);
  pthread_t file_thread;
  if(
  (pthread_mutex_init(&file_mutex, NULL)<0) ||
  (pthread_cond_init(&file_cond, NULL)<0) || 
  (pthread_cond_init(&write_complete_cond, NULL)< 0) ||
  (pthread_create(&file_thread, NULL, file_write_thread, NULL)<0) ||
  (pthread_detach(file_thread)<0)
  ){
   log_error("Unable to create pthread components. Disabling file write"); 
    global_file_write=false;
    destroy_pthread_component();
  }
  add_data_to_buffer(node_power_data->node_power_time);
}
void power_monitor_destructor() {
  free_global_buffer();
  destroy_pthread_component();
}

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
  response_power_data *power_data =
      get_agg_power_data(node_power_data->node_power_time,
                         node_power_data->hostname, start_time, end_time);
  if (power_data == NULL) {
    flux_log_error(h, "Unable to get agg data from node for "
                      "flux_pwr_montior.request_power_data_from_node request");
    if (flux_respond_error(h, msg, errno,
                           "error responding to "
                           "flux_pwr_montior.request_power_data_from_node "
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
