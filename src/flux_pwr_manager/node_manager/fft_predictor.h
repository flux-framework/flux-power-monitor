#ifndef FLUX_PWR_MANAGER_FFT_PREDICTOR_H
#define FLUX_PWR_MANAGER_FFT_PREDICTOR_H
#include "retro_queue_buffer.h"
#include <fftw3.h>
#include "system_config.h"
typedef struct fft_result {
  retro_queue_buffer_t gpus_period[NUM_OF_GPUS];
  double cpus_period[NUM_OF_CPUS];
  double time_taken;
} fft_result;
typedef struct fft_input_devices {
  fftw_complex gpus[NUM_OF_GPUS];
  fftw_complex cpus[NUM_OF_CPUS];
} fft_input_devices;
void fft_predictor_init();
void fft_predictor_destructor();
void fft_predictor_new_job();
void fft_predictor_finish_job();
void fft_predictor_get_result(int gpuId);
#endif
