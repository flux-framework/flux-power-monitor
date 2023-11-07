#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "constants.h"
#include "flux_pwr_logging.h"
#include "jansson.h"
#include "node_data.h"
#include "node_util.h"
#include "power_monitor.h"
#include "response_power_data.h"
#include "retro_queue_buffer.h"
#include "root_node_level_info.h"
#include "util.h"
#include "variorum.h"
#include <czmq.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#define MAX_FILENAME_SIZE 256

pthread_mutex_t file_mutex;
pthread_mutex_t data_mutex;
pthread_cond_t file_cond; // file is ready
// Control flags
bool terminate_thread = false; // Flag to signal thread termination
bool write_to_file = false;    // Flag to control when to write to file
bool job_running = false;      // Indicates whether a job is currently running
bool global_file_write = true; // Global flag to enable/disable file writing
node_power *current_element =
    NULL; // Tracks the element from which to start copying
int retro_queue_buffer_marker = -1; // Marker for circular buffer data
char output_filename[MAX_FILENAME_SIZE] = "default_output.csv";
FILE *current_file = NULL;
pthread_t data_thread; // New thread for data collection

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
  static int num_times = 0;
  node_power *np = (node_power *)data;
  char **temp_buffer = (char **)user_data;

  // Concatenate the fixed-size string
  strcat(*temp_buffer, np->csv_string);
  num_times++;
  log_message("num_times %d", num_times);
}

void update_temp_buffer(retro_queue_buffer_t *buffer, size_t start_index) {

  if (!global_temp_buffer) {
    fprintf(stderr, "Global buffer is not allocated\n");
    return;
  }
  log_message(
      "POWER_MONITOR:Updating temp_buffer current buffer size %ld",
      retro_queue_buffer_get_current_size(node_power_data->node_power_time));
  current_element = retro_queue_buffer_iterate_until_before_tail(
      buffer, &node_power_cmp, current_element, concatenate_csv_callback,
      &global_temp_buffer);
  // retro_queue_buffer_iterate_partial(buffer, concatenate_csv_callback,
  //                                 &global_temp_buffer, start_index,
  //                                 end_index);
}
void *add_data_to_buffer_thread(void *arg) {
  retro_queue_buffer_t *buffer = (retro_queue_buffer_t *)arg;
  int error = 0;
  while (!terminate_thread) {
    struct timespec start, end, req, rem;
    long time_spent, time_to_sleep;

    clock_gettime(CLOCK_MONOTONIC, &start);

    // Data fetching and processing
    int ret = 0;
    char *s = malloc(1500);
    if (s == NULL) {
      log_error("unable to allocate s variorum\n");
      error=-1;
      goto calculate_sleep; // Jump to sleep calculation
    }

    ret = variorum_get_node_power_json(&s);
    if (ret < 0) {
      log_error("unable to get variorum data\n");
      free(s);
      error=-1;
      goto calculate_sleep; // Jump to sleep calculation
    }

    node_power *p_data = parse_string(s);
    free(s);

    if (p_data == NULL) {
      log_error("Unable to parse the variorum_data\n");
      error = -1;
      goto calculate_sleep; // Jump to sleep calculation
    }

    // The last value is the marker.
    pthread_mutex_lock(&data_mutex);
    if (retro_queue_buffer_marker == 0 && global_file_write) {
      update_temp_buffer(
          node_power_data->node_power_time,
          retro_queue_buffer_get_max_size(node_power_data->node_power_time));
      retro_queue_buffer_marker =
          retro_queue_buffer_get_max_size(node_power_data->node_power_time) - 1;
      // Enable write to file.
      pthread_mutex_lock(&file_mutex);
      write_to_file = true;
      pthread_cond_signal(&file_cond);
      pthread_mutex_unlock(&file_mutex);
    }
    pthread_mutex_unlock(&data_mutex);
  calculate_sleep:
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_spent = (end.tv_sec - start.tv_sec) * 1000000000L +
                 (end.tv_nsec - start.tv_nsec);
    time_to_sleep = INTERVAL_SEC * 1000000000L + INTERVAL_NSEC - time_spent;
    if (time_to_sleep < 0) {
      log_message("Warning: Processing time exceeded sampling interval.");
      time_to_sleep = 0; // Ensure we don't have a negative sleep time
    }
    req.tv_sec = time_to_sleep / 1000000000L;
    req.tv_nsec = time_to_sleep % 1000000000L;
    nanosleep(&req, &rem);
    if (error == -1)
      continue;
    pthread_mutex_lock(&data_mutex);
    // Check if we should still insert data after potential long processing
    if (!terminate_thread && p_data) {
      retro_queue_buffer_push(node_power_data->node_power_time, p_data);
      if (retro_queue_buffer_marker > 0)
        retro_queue_buffer_marker--;
    }
    pthread_mutex_unlock(&data_mutex);
  }

  return NULL;
}
void write_header(FILE *file) {
  const char *header = "Timestamp (ms),Node Power (W),"
                       "Socket 0 CPU Power (W),Socket 0 GPU Power (W),"
                       "Socket 0 Mem Power (W),"
                       "Socket 1 CPU Power (W),Socket 1 GPU Power (W),"
                       "Socket 1 Mem Power (W)\n";
  fputs(header, file);
}

void write_data_to_file() {

  if (current_file != NULL && global_temp_buffer != NULL) {
    fputs(global_temp_buffer, current_file);
    reset_temp_buffer(global_temp_buffer, global_temp_buffer_size);
  }
}
void *file_write_thread(void *arg) {
  while (!terminate_thread) {
    pthread_mutex_lock(&file_mutex);
    while (!(write_to_file && global_file_write)) {
      if (terminate_thread) {
        break;
      }
      pthread_cond_wait(&file_cond, &file_mutex);
    }
    if (terminate_thread) {
      pthread_mutex_unlock(&file_mutex);
      break;
    }
    if (job_running) {
      if (current_file == NULL) {
        current_file = fopen(output_filename, "a");
        if (current_file == NULL) {
          log_message("POWER_MONITOR: ERROR IN opening file\n");
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
        if (current_file != NULL)
          write_header(current_file);
      }
      write_data_to_file();
      fclose(current_file);
      current_file = NULL;
      write_to_file = false;
    }
    pthread_mutex_unlock(&file_mutex);
  }
  if (current_file) {
    fclose(current_file);
    current_file = NULL;
  }
  return NULL;
}

void power_monitor_start_job(uint64_t jobId, char *job_cwd, char *job_name) {
  if (global_file_write) {
    pthread_mutex_lock(&data_mutex);

    retro_queue_buffer_marker =
        retro_queue_buffer_get_max_size(node_power_data->node_power_time) - 1;
    pthread_mutex_lock(&node_power_data->node_power_time->mutex);
    current_element = zlist_tail(node_power_data->node_power_time->list);
    pthread_mutex_unlock(&node_power_data->node_power_time->mutex);
    pthread_mutex_unlock(&data_mutex);

    pthread_mutex_lock(&file_mutex);
    // open file in the job current working directory
    snprintf(output_filename, MAX_FILENAME_SIZE, "%s/%s_%lu_%s.powmon.dat",
             job_cwd, job_name, jobId,
             node_power_data->hostname); // Use actual hostname
    job_running = true;
    pthread_mutex_unlock(&file_mutex);
  } else
    retro_queue_buffer_marker = -1;
  log_message("POWER_MONITOR:Job init");
}

void power_monitor_end_job() {

  if (global_file_write) {
    // Serialize access to buffer parameters
    pthread_mutex_lock(&data_mutex);
    update_temp_buffer(
        node_power_data->node_power_time,
        retro_queue_buffer_get_max_size(node_power_data->node_power_time));
    retro_queue_buffer_marker = -1;
    current_element = NULL;
    pthread_mutex_unlock(&data_mutex);
    // Serialize access to file
    pthread_mutex_lock(&file_mutex);
    write_to_file = true;
    job_running = false;
    pthread_cond_signal(&file_cond);
    pthread_mutex_unlock(&file_mutex);
  }
  log_message("POWER_MONITOR:Job done");
}

int power_monitor_set_node_powercap(double powercap) {
  variorum_cap_gpu_power_ratio(100);
  return variorum_cap_best_effort_node_power_limit(powercap);
}

void destroy_pthread_component() {

  terminate_thread = true;         // Signal the thread to terminate
  pthread_cond_signal(&file_cond); // Wake up the thread if it's waiting
  pthread_mutex_destroy(&file_mutex);
  pthread_mutex_destroy(&data_mutex);
  pthread_cond_destroy(&file_cond);
}

void power_monitor_init(size_t buffer_size) {
  int ret; // Variable to store return values for error checking
  global_file_write = true;
  global_temp_buffer_size = buffer_size;
  // Create data collection thread
  if ((ret = pthread_create(&data_thread, NULL, add_data_to_buffer_thread,
                            node_power_data->node_power_time)) != 0) {
    log_error("Data thread creation failed: %s", strerror(ret));
    // handle error
    return;
  }
  if (global_file_write) {
    // Assuming allocate_global_buffer returns an int for success (0) or failure
    // (-1)
    if ((ret = pthread_mutex_init(&data_mutex, NULL)) != 0) {
      log_error("data Mutex init failed: %s", strerror(ret));
      return;
    }
    ret = allocate_global_buffer(&global_temp_buffer, global_temp_buffer_size);
    if (ret != 0) {
      log_error("Failed to allocate global buffer");
      return;
    }

    pthread_t file_thread;
    if ((ret = pthread_mutex_init(&file_mutex, NULL)) != 0) {
      log_error("Mutex init failed: %s", strerror(ret));
      return;
    }

    if ((ret = pthread_cond_init(&file_cond, NULL)) != 0) {
      log_error("file_cond init failed: %s", strerror(ret));
      pthread_mutex_destroy(&file_mutex);
      return;
    }

    if ((ret = pthread_create(&file_thread, NULL, file_write_thread, NULL)) !=
        0) {
      log_error("Thread creation failed: %s", strerror(ret));
      pthread_mutex_destroy(&file_mutex);
      pthread_cond_destroy(&file_cond);
      return; // Exit if thread creation fails
    }

    if ((ret = pthread_detach(file_thread)) != 0) {
      log_error("Thread detach failed: %s", strerror(ret));
      // Cannot clean up thread here, as it's already running
    }
  }
  if ((ret = pthread_detach(data_thread)) != 0) {
    log_error("Data thread detach failed: %s", strerror(ret));
    // handle error
  }
}

void power_monitor_destructor() {
  if (global_file_write) {
    free_global_buffer();
    destroy_pthread_component();
  }
}
