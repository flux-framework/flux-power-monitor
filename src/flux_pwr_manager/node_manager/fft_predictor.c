#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "flux_pwr_logging.h"
#include "node_data.h" // Assuming this includes circular_buffer_t and node_power
#include <fftw3.h>
#include <pthread.h>
#include <string.h>

#define SAMPLING_SIZE 10 // SAMPLING RATE IN A SECOND
#define THIRTY_SECONDS (30 * SAMPLING_SIZE)
#define THREE_MINUTES (180 * SAMPLING_SIZE)

// Global variables
pthread_t fft_thread;
bool fft_thread_running = false;
bool job_active = false;
pthread_cond_t fft_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t fft_mutex = PTHREAD_MUTEX_INITIALIZER;
fftw_complex *fft_input;
// Global variable to keep track of the last copied index
size_t last_copied_index = -1;

void copy_node_power_to_fftw_callback(void *item, void *user_data) {
  static size_t copied = 0; // Keep track of how many items have been copied
  fftw_complex *dest_buffer = (fftw_complex *)user_data;

  node_power *np = (node_power *)item;
  if (np != NULL && copied < THREE_MINUTES) {
    dest_buffer[copied][0] = np->node_power; // Real part
    dest_buffer[copied][1] = 0.0;            // Imaginary part
    copied++;
  }
}
void copy_node_power_to_fftw_buffer(circular_buffer_t *src_buffer,
                                    fftw_complex *dest_buffer,
                                    size_t start_index, size_t end_index) {
  if (src_buffer == NULL || dest_buffer == NULL)
    return;

  pthread_mutex_lock(&src_buffer->mutex);

  circular_buffer_iterate_partial(src_buffer, copy_node_power_to_fftw_callback,
                                  (void *)dest_buffer, start_index, end_index);

  pthread_mutex_unlock(&src_buffer->mutex);
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
  for (int i = 1; i < data_size / 2; ++i) { // Skip DC component at i = 0
    double magnitude = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);

    if (magnitude > max_val) {
      max_val = magnitude;
      idx = i;
    }
  }
  // Adjusted frequency calculation
  double dominant_frequency = ((double)idx / data_size) * SAMPLING_SIZE;
  double Titer = 1 / dominant_frequency;
  printf("Dominant Frequency: %f Hz, Titer: %f seconds\n", dominant_frequency,
         Titer);

  // Process FFT output here

  fftw_destroy_plan(p);
  fftw_free(out);
}
size_t calculateStartIndex(circular_buffer_t *buffer,
                           size_t thirty_seconds_count) {
  if (buffer->current_size < thirty_seconds_count) {
    return 0; // Not enough data for thirty seconds
  } else if (buffer->current_size < buffer->max_size) {
    // Buffer not full, simple calculation
    return buffer->current_size - thirty_seconds_count;
  } else {
    // Buffer is full, calculate considering wrap-around
    return (buffer->max_size - 1) - thirty_seconds_count;
  }
}
void *fft_thread_func(void *args) {
  size_t fft_data_size = 0;
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
        // Time to process new data
        size_t new_data_end_index = (last_copied_index + THIRTY_SECONDS) %
                                    THREE_MINUTES; // Always ensure we are less
                                                   // than exact three minutes.
        copy_node_power_to_fftw_buffer(node_power_data->node_power_time,
                                       fft_input, last_copied_index,
                                       new_data_end_index);
        last_copied_index = new_data_end_index;
        fft_data_size += THIRTY_SECONDS;
        if (fft_data_size > THREE_MINUTES) {
          fft_data_size = THREE_MINUTES;
        }
        printf("data_size when performing FFT %ld", fft_data_size);
        struct timespec start, end;
        double elapsed;

        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
        perform_fft(fft_input, fft_data_size);
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);

        elapsed = end.tv_sec - start.tv_sec;
        elapsed += (end.tv_nsec - start.tv_nsec) / 1000000000.0;

        printf("Time taken by FFT: %f seconds\n", elapsed);
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
void fft_predictor_destroy() {
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
void notify_job_start() {
  pthread_mutex_lock(&fft_mutex);
  job_active = true;
  pthread_mutex_lock(&node_power_data->node_power_time->mutex);
  last_copied_index =
      calculateStartIndex(node_power_data->node_power_time, THIRTY_SECONDS);
  pthread_mutex_unlock(&node_power_data->node_power_time->mutex);
  pthread_cond_signal(&fft_cond); // Wake up the FFT thread
  pthread_mutex_unlock(&fft_mutex);
}

void notify_job_end() {
  pthread_mutex_lock(&fft_mutex);
  job_active = false;
  last_copied_index = -1;
  // Optionally perform any final FFT computation or cleanup here
  memset(fft_input, 0,
         sizeof(fftw_complex) * THREE_MINUTES); // Reset the buffer
  pthread_mutex_unlock(&fft_mutex);
}