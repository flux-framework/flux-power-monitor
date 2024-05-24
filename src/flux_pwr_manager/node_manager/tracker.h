#ifndef FLUX_PWR_MANAGER_NODE_JOB_TRACKER_H
#define FLUX_PWR_MANAGER_NODE_JOB_TRACKER_H
#include "constants.h"
#include "node_data.h"
#include "node_job_info.h"
#include <fftw3.h>
#define BUFFER_SIZE FFT_BUFFER_SIZE
typedef struct fft_tracker {

  int deviceId;
  uint64_t jobId;
  size_t data_copied_count;
  size_t fft_input_index;
  size_t cumulative_sum;
  node_power *buffer_marker_list;
  fftw_complex device_data[FFT_BUFFER_SIZE];

} fft_tracker_t;
typedef struct power_tracker {
  char *buffer;
  bool job_status;
  bool write_flag;
  bool header_writen;
  FILE *current_file;
  size_t buffer_size;
  char *write_buffer;
  size_t value_written;
  node_job_info *job_info;
  char filename[MAX_FILENAME_SIZE];

} power_tracker_t;
/**
 * @brief  destructor for power_tracker_t object.
 *
 * @param power_tracker_data pointer to the object that is to be destroyed.
 */
void power_tracker_destroy(void **power_tracker_data);
/**
 * @brief Constructor for power_tacker_t
 *
 * @param device_data job info.
 * @return
 */
power_tracker_t *power_tracker_new(node_job_info *device_data);
#endif
