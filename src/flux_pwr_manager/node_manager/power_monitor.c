#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "constants.h"
#include "flux_pwr_logging.h"
#include "jansson.h"
#include "job_hash.h"
#include "node_data.h"
#include "node_util.h"
#include "power_monitor.h"
#include "response_power_data.h"
#include "retro_queue_buffer.h"
#include "tracker.h"
#include "util.h"
#include "variorum.h"
#include <czmq.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#define MAX_FILENAME_SIZE 256
#define POWER_RATIO 0
pthread_mutex_t file_mutex;
pthread_mutex_t data_mutex;
pthread_cond_t file_cond; // file is ready
// Control flags
bool terminate_thread = false; // Flag to signal thread termination
bool write_to_file = false;    // Flag to control when to write to file
bool global_file_write = true; // Global flag to enable/disable file writing
pthread_t data_thread;         // New thread for data collection
zhashx_t *power_job_data;      // To record current jobs running on this node.
bool job_running = false;
uint64_t shared_jobid = 0;

//*****These two variable are always modified before file write thread is
// called, so it should be thread safe.****
// Callback function for concatenating csv strings
typedef struct {
  char *write_buffer[NUM_OF_GPUS];
  int deviceId[NUM_OF_GPUS];
  bool file_write;
  size_t buffer_size;
  pthread_mutex_t share_lock;
  char jobname[MAX_FILENAME_SIZE];
} shared_data;

void *add_data_to_buffer_thread(void *arg) {
  retro_queue_buffer_t *buffer = (retro_queue_buffer_t *)arg;
  static size_t counter = 0;
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
      error = -1;
      goto calculate_sleep; // Jump to sleep calculation
    }

    ret = variorum_get_power_json(&s);
    if (ret < 0) {
      log_error("unable to get variorum data\n");
      free(s);
      error = -1;
      goto calculate_sleep; // Jump to sleep calculation
    }

    node_power *p_data = parse_string(s);
    free(s);

    if (p_data == NULL) {
      log_error("Unable to parse the variorum_data\n");
      error = -1;
      goto calculate_sleep; // Jump to sleep calculation
    }
    bool write_flag_set = false;
    // The last value is the marker.
    if (global_file_write) {
      power_tracker_t *data = zhashx_first(power_job_data);
      while (data != NULL) {
        if (data->value_written == BUFFER_SIZE) {

          pthread_mutex_lock(&file_mutex);
          data->write_flag = true;
          write_flag_set = true;
          memcpy(data->write_buffer, data->buffer,
                 MAX_CSV_SIZE * data->buffer_size);
          pthread_mutex_unlock(&file_mutex);
          reset_buffer(data->buffer, data->buffer_size);
        } else if (data->value_written < data->buffer_size) {
          if (write_device_specific_node_power_data(data->buffer, p_data,
                                                    data->job_info) < 0) {
            log_error("Unable to write data in buffer");
            continue;
          }
          data->value_written++;
        }
        data = zhashx_next(power_job_data);
      }

      // Enable write to file.
      pthread_mutex_lock(&file_mutex);
      if (write_flag_set) {
        write_to_file = true;
        pthread_cond_signal(&file_cond);
      }
      pthread_mutex_unlock(&file_mutex);
    }
    goto calculate_sleep;
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
    int status = nanosleep(&req, &rem);
    if (status != 0) {
      log_error("Sleep failed");
    }
    if (error == -1)
      continue;
    pthread_mutex_lock(&data_mutex);
    // Check if we should still insert data after potential long processing
    if (!terminate_thread && p_data) {
      // log_message("inserti");
      retro_queue_buffer_push(node_power_data->node_power_time, p_data);
      // printf("entry time stamp %ld \n", p_data->timestamp);
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

void write_data_to_file(char *write_buffer, FILE *f) {
  if (write_buffer != NULL) {
    fputs(write_buffer, f);
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
    power_tracker_t *p_data = zhashx_first(power_job_data);

    while (p_data != NULL) {
      log_message("writing");
      if (!p_data->write_flag) {
        p_data = zhashx_next(power_job_data);
        continue;
      }
      FILE *f = fopen(p_data->filename, "a");
      if (!f) {
        p_data = zhashx_next(power_job_data);
        continue;
      }
      if (!p_data->header_writen) {
        // write_header(f);
        char *header;
        get_header(&header, p_data->job_info);
        fputs(header, f);
        p_data->header_writen = true;
        free(header);
      }
      write_data_to_file(p_data->write_buffer, f);
      p_data->write_flag = false;
      p_data->value_written = 0;

      pthread_mutex_unlock(&file_mutex);
      fclose(f);
      reset_buffer(p_data->write_buffer, p_data->buffer_size);
      if (!p_data->job_status) {
        zhashx_delete(power_job_data, &p_data->job_info->jobId);
        if (zhashx_size(power_job_data) == 0)
          job_running = false;
      }
      p_data = zhashx_next(power_job_data);
    }
    write_to_file = false;

    pthread_mutex_unlock(&file_mutex);
  }
  // if (current_file) {
  //   fclose(current_file);
  //   current_file = NULL;
  // }
  return NULL;
}

void power_monitor_start_job(uint64_t jobId) {
  if (global_file_write) {
    node_job_info *job_data = zhashx_lookup(current_jobs, &jobId);
    if (!job_data) {
      log_error("Job not found in the current jobs hash");
      return;
    }
    power_tracker_t *data = power_tracker_new(job_data);
    // Dynamically allocated JobId.
    uint64_t *key = malloc(sizeof(uint64_t));
    *key = jobId;
    log_message("PM:Insert");
    if (zhashx_insert(power_job_data, key, (void *)data) < 0) {
      log_error("unable to insert data to power_job_data hash");
      return;
    }
  }
}

void power_monitor_end_job(uint64_t jobId) {

  if (global_file_write) {
    power_tracker_t *data = zhashx_lookup(power_job_data, &jobId);
    if (!data) {
      log_error("data not found for the jobId %ld", jobId);
      pthread_mutex_unlock(&file_mutex);
      return;
    }
    pthread_mutex_lock(&file_mutex);
    data->write_flag = true;
    data->job_status = false;
    write_to_file = true;
    memcpy(data->write_buffer, data->buffer, MAX_CSV_SIZE * data->buffer_size);
    pthread_cond_signal(&file_cond);
    // Wait for file write to be complete

    pthread_mutex_unlock(&file_mutex);
  }
  log_message("POWER_MONITOR:Job done");
}
int power_monitor_set_node_power_ratio(int power_ratio) {
  return variorum_cap_gpu_power_ratio(power_ratio);
}
int power_monitor_set_node_powercap(double powercap, int gpu_id) {

  char command[256];
  // snprintf(command, sizeof(command), "sudo nvidia-smi -pl %f -i %d",
  // powercap, gpu_id);
  int powercap_int = (int)round(powercap);
  log_message("powercapping GPU ID: %d with value d", gpu_id, powercap);
  snprintf(command, sizeof(command),
           "sudo /admin/scripts/nv_powercap -p  %d -i %d", powercap_int,
           gpu_id);

  return system(command);
}
// return variorum_cap_best_effort_node_power_limit(powercap);

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

  // Create data collection thread
  if ((ret = pthread_create(&data_thread, NULL, add_data_to_buffer_thread,
                            node_power_data->node_power_time)) != 0) {
    log_error("Data thread creation failed: %s", strerror(ret));
    // handle error
    return;
  }
  if (global_file_write) {

    power_job_data = malloc(NUM_OF_GPUS * sizeof(power_tracker_t *));
    if (power_job_data == NULL) {
      log_error("memory allocation failed for file write");
      // Disable file write;
      global_file_write = false;
      return;
    }
    power_job_data = job_hash_create();
    if (power_job_data == NULL) {
      global_file_write = false;
      return;
    }
    zhashx_set_destructor(power_job_data, power_tracker_destroy);
    // Assuming allocate_global_buffer returns an int for success (0) or failure
    // (-1)
    if ((ret = pthread_mutex_init(&data_mutex, NULL)) != 0) {
      log_error("data Mutex init failed: %s", strerror(ret));
      return;
    }
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
    if (power_job_data) {
      zhashx_destroy(&power_job_data);
    }
    free(power_job_data);
    destroy_pthread_component();
  }
}
