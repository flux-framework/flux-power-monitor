#ifndef FLUX_PWR_MANAGER_FFT_PREDICTOR_H
#define FLUX_PWR_MANAGER_FFT_PREDICTOR_H
#include "node_data.h"
#include "node_job_info.h"
#include "retro_queue_buffer.h"
#include "system_config.h"
#include <fftw3.h>
typedef struct fft_result {
  retro_queue_buffer_t *gpus_period[NUM_OF_GPUS];
  retro_queue_buffer_t *gpus_period_secondary[NUM_OF_GPUS];
  retro_queue_buffer_t *fft_size[NUM_OF_GPUS];
  retro_queue_buffer_t *average_period_tracker[NUM_OF_GPUS];
  double cpus_period[NUM_OF_CPUS];
  double time_taken;
} fft_result;
typedef struct fft_input_devices {
  fftw_complex gpus[NUM_OF_GPUS];
  fftw_complex cpus[NUM_OF_CPUS];
} fft_input_devices;
// Each device gets its own, copy of tracking data.
typedef struct fft_device_tracker {
  uint64_t jobId;
  int num_of_copied;
  int total_copied;
  node_power *data;
} fft_device_tracker_t;
void fft_predictor_init();
void fft_predictor_destructor();
size_t fft_predictor_get_size(int gpuId);
double fft_predictor_get_result(int gpuId);
void fft_predictor_new_job(uint64_t jobId);
void fft_predictor_finish_job(uint64_t jobId);
/**
 * @brief This function returns a reference to FFT retro queue buffer. That is
 * used by node manager to make power decisions.
 *
 * @param gpuId The relevant GPUid
 * @return refernece to retro queue buffer, that will store the last result's of
 * the fft.
 */
retro_queue_buffer_t *fft_predictor_get_full_results(int gpuId);
double fft_predictor_secondary_get_result(int gpuId);
/**
 * @brief Method that resets the FFT tracker, specifically the buffer for a
 * particular gpuid. Preserves the job Info.
 *
 * @param gpuid identifes the devices needed to be reseted.
 */
void fft_predictor_reset(int gpuid);
#endif
