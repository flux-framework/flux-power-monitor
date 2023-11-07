#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "constants.h"
#include "flux_pwr_logging.h"
#include "node_data.h" // Assuming this includes retro_queue_buffer_t and node_power
#include "node_util.h"
#include <fftw3.h>
#include <pthread.h>
#include <string.h>

// Global variables
pthread_t fft_thread;
bool fft_thread_running = false;
bool job_active = false;
pthread_cond_t fft_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t fft_mutex = PTHREAD_MUTEX_INITIALIZER;
fftw_complex *fft_input;
node_power *current_element;
int num_copied = 0;
int global_copied = 0;
size_t fft_data_size = 0;

void copy_node_power_to_fftw_callback(void *item, void *user_data) {
  fftw_complex *dest_buffer = (fftw_complex *)user_data;

  node_power *np = (node_power *)item;
  if (np != NULL && global_copied < THREE_MINUTES) {
    dest_buffer[global_copied][0] = np->node_power; // Real part
    dest_buffer[global_copied][1] = 0.0;            // Imaginary part
    global_copied++;
    num_copied++;
  }
}

// FFT computation function
void perform_fft(fftw_complex *data, size_t data_size) {
  fftw_complex *out =
      (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * data_size);
  fftw_plan p =
      fftw_plan_dft_1d(data_size, data, out, FFTW_FORWARD, FFTW_ESTIMATE);

  fftw_execute(p);
  // Find the dominant frequency
  int idx = 0;
  double max_val = 0.0;
  double second_max_val = 0.0;
  int second_idx = 0.0;
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
  // Adjusted frequency calculation
  double dominant_frequency =
      ((double)idx) * ((SAMPLING_RATE * 1.0) / data_size);
  double Titer = 1 / dominant_frequency;
  double second_dominant_frequency =
      ((double)second_idx) * ((SAMPLING_RATE * 1.0) / data_size);
  double second_Titer = 1 / second_dominant_frequency;
  log_message("Dominant Frequency: %f Hz, Titer: %f seconds\n",
              dominant_frequency, Titer);

  log_message("Second Dominant Frequency: %f Hz, Titer: %f seconds\n",
              second_dominant_frequency, second_Titer);
  // Process FFT output here

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
    wait_time.tv_sec += 30;

    // Check if there's new data to process every 30 seconds
    while (job_active && fft_data_size < THREE_MINUTES) {
      wait_result = pthread_cond_timedwait(&fft_cond, &fft_mutex, &wait_time);
      if (wait_result == ETIMEDOUT) {
        if (!current_element)
          continue;
        current_element = retro_queue_buffer_iterate_from(
            node_power_data->node_power_time, &node_power_cmp,
            (void *)current_element, &copy_node_power_to_fftw_callback,
            (void *)fft_input, THIRTY_SECONDS);
        fft_data_size += num_copied;
        num_copied = 0;
        if (fft_data_size > THREE_MINUTES) {
          fft_data_size = THREE_MINUTES;
        }
        log_message("data_size when performing FFT %ld", fft_data_size);
        struct timespec start, end;
        double elapsed;

        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
        perform_fft(fft_input, fft_data_size);
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);

        elapsed = end.tv_sec - start.tv_sec;
        elapsed += (end.tv_nsec - start.tv_nsec) / 1000000000.0;

        log_message("Time taken by FFT: %f seconds\n", elapsed);
        // Set up the next time to check the buffer (in another 30 seconds)
        clock_gettime(CLOCK_REALTIME, &wait_time);
        wait_time.tv_sec += 30;
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

// Initialization
void fft_predictor_init() {
  pthread_mutex_lock(&fft_mutex);
  fft_thread_running = true;
  fft_input = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * THREE_MINUTES);
  memset(fft_input, 0,
         sizeof(fftw_complex) * THREE_MINUTES); // Initialize with zeros
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

  if (fft_input) {
    fftw_free(fft_input);
  }
}

// Job notifications
void fft_predictor_new_job() {
  pthread_mutex_lock(&fft_mutex);
  job_active = true;
  global_copied = 0;
  num_copied = 0;
  fft_data_size = 0;
  pthread_mutex_lock(&node_power_data->node_power_time->mutex);
  current_element =
      (node_power *)zlist_tail(node_power_data->node_power_time->list);
  pthread_mutex_unlock(&node_power_data->node_power_time->mutex);
  pthread_cond_signal(&fft_cond); // Wake up the FFT thread
  pthread_mutex_unlock(&fft_mutex);
  log_message("FFT_PREDICTOR:Job Init");
}

void fft_predictor_finish_job() {
  log_message("FFT_PREDICTOR: recevied job finsihed");

  pthread_mutex_lock(&fft_mutex);
  job_active = false;
  current_element = NULL;
  global_copied = 0;
  num_copied = 0;
  fft_data_size = 0;
  // Optionally perform any final FFT computation or cleanup here
  memset(fft_input, 0,
         sizeof(fftw_complex) * THREE_MINUTES); // Reset the buffer
  pthread_mutex_unlock(&fft_mutex);
  log_message("FFT_PREDICTOR: job finsihed");
}
