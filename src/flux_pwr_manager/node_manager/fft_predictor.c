#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "constants.h"
#include "fft_predictor.h"
#include "flux_pwr_logging.h"
#include "node_data.h" // Assuming this includes retro_queue_buffer_t and node_power
#include "node_job_info.h"
#include "node_util.h"
#include "retro_queue_buffer.h"
#include "system_config.h"
#include "tracker.h"
#include "util.h"
#include <czmq.h>
#include <pthread.h>
#include <string.h>

// Global variables
pthread_t fft_thread;
bool fft_thread_running = false;
bool job_active = false;
pthread_cond_t fft_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t fft_mutex = PTHREAD_MUTEX_INITIALIZER;
fft_result *fft_result_array;
node_power **job_tracker;
fft_tracker_t **fft_job_data;
void copy_node_power_to_fftw_callback(void *item, void *user_data) {

  node_power *np = (node_power *)item;
  fft_tracker_t *data = (fft_tracker_t *)user_data;

  // fft_input_devices *fft_devices_data = (fft_input_devices *)user_data;
  //
  if (np != NULL && data != NULL && data->deviceId != -1) {
    data->device_data[data->fft_input_index][0] = np->gpu_power[data->deviceId];
    data->device_data[data->fft_input_index][1] = 0;
    data->fft_input_index++;
    data->data_copied_count++;
  } else {
    log_error("CRITICAL:data conversion not possible");
  }
}

// FFT computation function
void perform_fft(fftw_complex *data, size_t data_size, double *result_first,
                 double *result_second) {
  fftw_complex *out =
      (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * data_size);
  fftw_plan p =
      fftw_plan_dft_1d(data_size, data, out, FFTW_FORWARD, FFTW_ESTIMATE);
  fftw_execute(p);
  // Find the dominant frequency
  // for (int i = 0; i < data_size; i++) {
  //   log_message("input %f", data[i][0]);
  // }
  int idx = 0;
  double max_val = 0.0;
  double second_max_val = 0.0;
  int second_idx = 0;
  for (int i = 1; i < data_size / 2; ++i) { // Skip DC component at i = 0
    double magnitude = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);
    if (magnitude > max_val) {
      second_max_val = max_val;
      max_val = magnitude;
      idx = i;
    }
    if (magnitude > second_max_val && magnitude < max_val) {
      second_max_val = magnitude;
      second_idx = i;
    }
  }

  static bool flag = true;

  // Adjusted frequency calculation
  double dominant_frequency =
      ((double)idx) * ((SAMPLING_RATE * 1.0) / data_size);
  double Titer = 1 / dominant_frequency;
  double second_dominant_frequency =
      ((double)second_idx) * ((SAMPLING_RATE * 1.0) / data_size);
  double second_Titer = 1 / second_dominant_frequency;
  *result_first = Titer;

  log_message("Dominant Frequency: %f Hz, Titer: %f seconds\n",
              dominant_frequency, Titer);
  *result_second = second_Titer;
  log_message("Second Dominant Frequency: %f Hz, Titer: %f seconds\n",
              second_dominant_frequency, second_Titer);
  log_message("fft size %ld", data_size);
  //
  fftw_destroy_plan(p);
  fftw_free(out);
}
void *fft_thread_func(void *args) {
  struct timespec wait_time;
  int wait_result;
  while (fft_thread_running) {

    pthread_mutex_lock(&fft_mutex);
    while (!job_active) {
      // Wait indefinitely until a job starts
      pthread_cond_wait(&fft_cond, &fft_mutex);
    }

    // Set up the next time to check the buffer (in 30 seconds)
    clock_gettime(CLOCK_REALTIME, &wait_time);
    wait_time.tv_sec += (int)FFT_SAMPLING_PERIOD / SAMPLING_RATE;

    // Check if there's new data to process every 30 seconds. And try to find
    // period just for three minutes, if not we fail in finding the period.
    while (job_active) {

      wait_result = pthread_cond_timedwait(&fft_cond, &fft_mutex, &wait_time);
      if (wait_result == ETIMEDOUT) {
        for (int i = 0; i < NUM_OF_GPUS; i++) {
          if (fft_job_data[i]->jobId != 0) {
            fft_tracker_t *data = fft_job_data[i];
            data->buffer_marker_list = retro_queue_buffer_iterate_from(
                node_power_data->node_power_time, &node_power_cmp,
                (void *)data->buffer_marker_list,
                &copy_node_power_to_fftw_callback, (void *)data,
                FFT_SAMPLING_PERIOD);
            struct timespec start, end;
            double elapsed;

            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
            double *result = malloc(sizeof(double));
            double *result_second = malloc(sizeof(double));
            size_t *fft_size = malloc(sizeof(size_t));
            *result = 0.0f;
            *result_second = 0.0f;
            data->cumulative_sum += data->data_copied_count;
            perform_fft(data->device_data, data->cumulative_sum, result,
                        result_second);
            *fft_size = data->cumulative_sum;
            data->data_copied_count = 0;
            // perform_fft(data->device_data,
            //             data->fft_data_size, result, result_second);
            retro_queue_buffer_push(fft_result_array->gpus_period[i], result);
            retro_queue_buffer_push(fft_result_array->gpus_period_secondary[i],
                                    result_second);
            retro_queue_buffer_push(fft_result_array->fft_size[i], fft_size);
            double average =
                do_average(fft_result_array->average_period_tracker[i]);

            retro_queue_buffer_pop(fft_result_array->average_period_tracker[i]);
            double *avg = malloc(sizeof(double));
            *avg = average;
            retro_queue_buffer_push(fft_result_array->average_period_tracker[i],
                                    avg);

            if (data->cumulative_sum >= FFT_BUFFER_SIZE) {
              data->data_copied_count = 0;
              data->fft_input_index = 0;
              data->cumulative_sum = 0;
              memset(data->device_data, 0,
                     sizeof(fftw_complex) * FFT_BUFFER_SIZE);
            }
            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
            elapsed = end.tv_sec - start.tv_sec;
            elapsed += (end.tv_nsec - start.tv_nsec) / 1000000000.0;
            fft_result_array->time_taken = elapsed;
            // Set up the next time to check the buffer (in another 30 seconds)
            clock_gettime(CLOCK_REALTIME, &wait_time);
            wait_time.tv_sec += (int)FFT_SAMPLING_PERIOD / SAMPLING_RATE;
          }
        }
      } else if (wait_result == 0) {
        // Woken up by a job ending or other signal; check job_active
        if (!job_active) {
          break; // Exit the loop if the job has ended
        }
      }
    }

    pthread_mutex_unlock(&fft_mutex);
  }

  pthread_exit(NULL);
}
void fft_tracker_reset(fft_tracker_t *fft_object) {
  if (!fft_object)
    return;
  fft_object->buffer_marker_list = NULL;
  fft_object->data_copied_count = 0;
  fft_object->fft_input_index = 0;
  fft_object->buffer_marker_list = 0;
  fft_object->jobId = 0;
  fft_object->deviceId = -1;
  fft_object->cumulative_sum = 0;

  memset(fft_object->device_data, 0, sizeof(fftw_complex) * FFT_BUFFER_SIZE);
}
void new_fft_result(int i) {

  fft_result_array->gpus_period[i] = NULL;
  fft_result_array->gpus_period[i] = retro_queue_buffer_new(100, free);
  fft_result_array->gpus_period_secondary[i] =
      retro_queue_buffer_new(100, free);
  fft_result_array->fft_size[i] = retro_queue_buffer_new(100, free);
  fft_result_array->average_period_tracker[i] =
      retro_queue_buffer_new(100, free);
}
void destroy_fft_result(int i) {

  retro_queue_buffer_destroy(fft_result_array->gpus_period[i]);
  retro_queue_buffer_destroy(fft_result_array->gpus_period_secondary[i]);
  retro_queue_buffer_destroy(fft_result_array->fft_size[i]);
  retro_queue_buffer_destroy(fft_result_array->average_period_tracker[i]);
}
// Initialization
void fft_predictor_init() {
  pthread_mutex_lock(&fft_mutex);
  fft_thread_running = true;
  fft_result_array = calloc(1, sizeof(fft_result));
  fft_job_data = malloc(NUM_OF_GPUS * sizeof(fft_tracker_t *));

  if (!fft_job_data) {
    log_error("Memory Allocation Failure for fft_job_data");
  }

  for (int i = 0; i < NUM_OF_GPUS; i++) {
    fft_job_data[i] = NULL;
    fft_job_data[i] = calloc(1, sizeof(fft_tracker_t));
    fft_tracker_reset(fft_job_data[i]);
  }

  // memset(fft_result_array, 0, sizeof(fft_result));
  // fft_input = malloc(NUM_OF_DEVICES * sizeof(fftw_complex *));
  pthread_create(&fft_thread, NULL, fft_thread_func, NULL);
  pthread_mutex_unlock(&fft_mutex);
}

// Destructor
void fft_predictor_destructor() {
  pthread_mutex_lock(&fft_mutex);
  fft_thread_running = false;
  pthread_cond_signal(&fft_cond);
  pthread_mutex_unlock(&fft_mutex);
  pthread_join(fft_thread, NULL); // Wait for the thread to finish
  for (int i = 0; i < NUM_OF_GPUS; i++) {
    retro_queue_buffer_destroy(fft_result_array->gpus_period[i]);
  }
  for (int i = 0; i < NUM_OF_GPUS; i++) {
    retro_queue_buffer_destroy(fft_result_array->gpus_period_secondary[i]);
  }
  for (int i = 0; i < NUM_OF_GPUS; i++) {
    retro_queue_buffer_destroy(fft_result_array->fft_size[i]);
  }
  for (int i = 0; i < NUM_OF_GPUS; i++) {
    retro_queue_buffer_destroy(fft_result_array->average_period_tracker[i]);
  }
  free(fft_result_array);
}

// Job notifications
void fft_predictor_new_job(uint64_t jobId) {
  if (jobId == 0) {
    log_error("wrong JobId");
  }
  node_job_info *data = zhashx_lookup(current_jobs, &jobId);
  if (!data) {
    log_error("Job Data not found");
    return;
  }
  log_message("FFT_PREDICTOR:Job Init");

  pthread_mutex_lock(&fft_mutex);
  pthread_mutex_lock(&node_power_data->node_power_time->mutex);
  node_power *marker =
      (node_power *)zlist_tail(node_power_data->node_power_time->list);
  pthread_mutex_unlock(&node_power_data->node_power_time->mutex);

  job_active = true;
  for (int i = 0; i < data->num_of_devices; i++) {
    int gpuid = data->deviceId[i];
    fft_tracker_reset(fft_job_data[gpuid]);
    fft_job_data[gpuid]->jobId = jobId;
    fft_job_data[gpuid]->deviceId = gpuid;
    fft_job_data[gpuid]->buffer_marker_list = marker;
    new_fft_result(gpuid);
  }
  pthread_cond_signal(&fft_cond); // Wake up the FFT thread
  pthread_mutex_unlock(&fft_mutex);
}

void fft_predictor_finish_job(uint64_t jobId) {
  if (jobId == 0) {
    log_error("wrong JobId");
  }
  // Do these things when there are no jobs remaining.
  node_job_info *data = zhashx_lookup(current_jobs, &jobId);
  if (!data)
    return;

  for (int i = 0; i < data->num_of_devices; i++) {
    fft_tracker_reset(fft_job_data[data->deviceId[i]]);
    destroy_fft_result(data->deviceId[i]);
  }
  if ((zhashx_size(current_jobs) - 1) == 0) {
    pthread_mutex_lock(&fft_mutex);
    job_active = false;
    pthread_mutex_unlock(&fft_mutex);
  }
  log_message("FFT_PREDICTOR: job finsihed");
}

void fft_predictor_reset(int gpuid) {
  pthread_mutex_lock(&fft_mutex);
  fft_tracker_t *data = fft_job_data[gpuid];
  log_message("RESET GPU for GPUID=%d", gpuid);

  // Reset the FFT buffer when power changes.
  memset(data->device_data, 0, sizeof(FFT_BUFFER_SIZE));
  data->cumulative_sum = 0;
  data->fft_input_index = 0;
  data->data_copied_count = 0;

  pthread_mutex_lock(&node_power_data->node_power_time->mutex);
  data->buffer_marker_list =
      (node_power *)zlist_tail(node_power_data->node_power_time->list);
  pthread_mutex_unlock(&node_power_data->node_power_time->mutex);
  log_message("RESET GPU for GPUID=%d", gpuid);

  pthread_mutex_unlock(&fft_mutex);
}
size_t fft_predictor_get_size(int gpuId) {
  for (int i = 0; i < NUM_OF_GPUS; i++) {
    if (i == gpuId) {
      if (fft_result_array->fft_size[i] != NULL &&
          zlist_size(fft_result_array->fft_size[i]->list) > 0) {

        size_t *period =
            (size_t *)zlist_tail(fft_result_array->fft_size[i]->list);

        return *period;
      } else {
        log_error("Error: in finding the last element");
      }
    }
  }
  return -1;
}
double fft_predictor_secondary_get_result(int gpuId) {
  for (int i = 0; i < NUM_OF_GPUS; i++) {
    if (i == gpuId) {
      if (fft_result_array->gpus_period_secondary[i] != NULL &&
          zlist_size(fft_result_array->gpus_period_secondary[i]->list) > 0) {

        double *period = (double *)zlist_tail(
            fft_result_array->gpus_period_secondary[i]->list);

        return *period;
      } else {
        log_error("Error: in finding the last element");
      }
    }
  }
  return -1;
}
retro_queue_buffer_t *fft_predictor_get_full_results(int gpuId) {
  return fft_result_array->gpus_period[gpuId];
}
double fft_predictor_get_result(int gpuId) {
  for (int i = 0; i < NUM_OF_GPUS; i++) {
    if (i == gpuId) {
      if (fft_result_array->gpus_period[i] != NULL &&
          zlist_size(fft_result_array->gpus_period[i]->list) > 0) {

        double *period =
            (double *)zlist_tail(fft_result_array->gpus_period[i]->list);

        return *period;
      } else {
        log_error("Error: in finding the last element");
      }
    }
  }
  return -1;
}
