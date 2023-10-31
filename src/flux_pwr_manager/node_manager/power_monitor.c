#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "circular_buffer.h"
#include "flux_pwr_logging.h"
#include "jansson.h"
#include "node_data.h"
#include "node_util.h"
#include "power_monitor.h"
#include "response_power_data.h"
#include "root_node_level_info.h"
#include "util.h"
#include "variorum.h"
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#define MAX_FILENAME_SIZE 256
#define SAMPLING_RATE 0.1 //Seconds
pthread_mutex_t file_mutex;
pthread_cond_t file_cond;           // file is ready
pthread_cond_t write_complete_cond; // Writing is done
// Control flags
bool terminate_thread = false; // Flag to signal thread termination
bool write_to_file = false;    // Flag to control when to write to file
bool job_running = false;      // Indicates whether a job is currently running
bool global_file_write = true; // Global flag to enable/disable file writing

int circular_buffer_marker = -1; // Marker for circular buffer data
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

  circular_buffer_iterate_partial(buffer, concatenate_csv_callback,
                                  &global_temp_buffer, start_index, end_index);
}

void *add_data_to_buffer_thread(void *arg) {
  circular_buffer_t *buffer = (circular_buffer_t *)arg;
  struct timespec req, rem;
  req.tv_sec = 0;
  req.tv_nsec = SAMPLING_RATE * 1000000000ULL;
  while (!terminate_thread) {

    int ret = 0;
    char *s = malloc(1500);
    if (s == NULL) {
      log_message("unable to allocate s variorum");
      nanosleep(&req, &rem);
      continue;
    }
    ret = variorum_get_node_power_json(&s);
    if (ret < 0) {
      log_message("unable to get variorum data");
      nanosleep(&req, &rem);
      free(s);
      continue;
    }
    node_power *p_data = parse_string(s);
    free(s);
    if (p_data == NULL) {
      log_message("Unable to parse the variorum_data");
      nanosleep(&req, &rem);
      continue;
    }
    // The last value is the marker.
    if (circular_buffer_marker == 1 && global_file_write) {
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
    if (circular_buffer_marker != -1 &&
        global_file_write) // Indicating we dont need to update circular marker
      // BUG: Check for correct value of circular_buffer_marker
      circular_buffer_marker--;
  }
  return NULL;
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
  if (current_file) {
    fclose(current_file);
    current_file = NULL;
  }
  return NULL;
}

void power_monitor_start_job(uint64_t jobId) {
  if (global_file_write) {
    circular_buffer_marker =
        circular_buffer_get_max_size(node_power_data->node_power_time);
    pthread_mutex_lock(&file_mutex);
    snprintf(output_filename, MAX_FILENAME_SIZE, "%lu_%s.csv", jobId,
             node_power_data->hostname); // Use actual hostname
    job_running = true;
    pthread_mutex_unlock(&file_mutex);
  } else
    circular_buffer_marker = -1;
}

void power_monitor_end_job() {
  // Copy the buffer so that it can be written.
  if (global_file_write) {
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
}

void destroy_pthread_component() {

  terminate_thread = true; // Signal the thread to terminate

  pthread_cond_signal(&file_cond); // Wake up the thread if it's waiting

  pthread_mutex_destroy(&file_mutex);
  pthread_cond_destroy(&file_cond);
  pthread_cond_destroy(&write_complete_cond);
  destroy_pthread_component();
}

void power_monitor_init() {
  int ret; // Variable to store return values for error checking
  global_file_write = false;

  // Assuming allocate_global_buffer returns an int for success (0) or failure
  // (-1)
  // Create data collection thread
  if ((ret = pthread_create(&data_thread, NULL, add_data_to_buffer_thread,
                            node_power_data->node_power_time)) != 0) {
    log_error("Data thread creation failed: %s", strerror(ret));
    // handle error
    return;
  }
  if (global_temp_buffer) {
    ret = allocate_global_buffer(
        global_temp_buffer, &global_temp_buffer_size,
        circular_buffer_get_max_size(node_power_data->node_power_time) - 1);
    if (ret != 0) {
      log_error("Failed to allocate global buffer");
      return; // Exit if buffer allocation fails
    }

    pthread_t file_thread;
    if ((ret = pthread_mutex_init(&file_mutex, NULL)) != 0) {
      log_error("Mutex init failed: %s", strerror(ret));
      return; // Exit if mutex init fails
    }

    if ((ret = pthread_cond_init(&file_cond, NULL)) != 0) {
      log_error("file_cond init failed: %s", strerror(ret));
      pthread_mutex_destroy(&file_mutex); // Clean up mutex
      return; // Exit if condition variable init fails
    }

    if ((ret = pthread_cond_init(&write_complete_cond, NULL)) != 0) {
      log_error("write_complete_cond init failed: %s", strerror(ret));
      pthread_mutex_destroy(&file_mutex); // Clean up mutex
      pthread_cond_destroy(&file_cond);   // Clean up condition variable
      return; // Exit if condition variable init fails
    }

    if ((ret = pthread_create(&file_thread, NULL, file_write_thread, NULL)) !=
        0) {
      log_error("Thread creation failed: %s", strerror(ret));
      pthread_mutex_destroy(&file_mutex);         // Clean up mutex
      pthread_cond_destroy(&file_cond);           // Clean up condition variable
      pthread_cond_destroy(&write_complete_cond); // Clean up condition variable
      return; // Exit if thread creation fails
    }

    if ((ret = pthread_detach(file_thread)) != 0) {
      log_error("Thread detach failed: %s", strerror(ret));
      // Cannot clean up thread here, as it's already running
    }

    global_file_write =
        true; // Enable file write, assuming initialization is successful
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
