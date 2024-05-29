#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "flux_pwr_logging.h"
#include "node_power_info.h"
#include "tracker.h"

power_tracker_t *power_tracker_new(node_job_info *device_data) {
  power_tracker_t *data = calloc(1, sizeof(power_tracker_t));
  if (!data) {
    log_error("Memory allocation failed for power_tracker_t");
    return NULL;
  }
  if (device_data == NULL) {
    log_error("Device information error");
    return NULL;
  }
  data->job_info = node_job_info_copy(device_data);
  if (data->job_info == NULL) {
    log_error("Copying of device_data failed");
  }
  data->value_written = 0;
  data->write_flag = false;
  data->job_status = true;
  data->current_file = NULL;
  data->header_writen = false;
  data->buffer_size = BUFFER_SIZE;
  data->buffer = malloc(data->buffer_size * MAX_CSV_SIZE * sizeof(char));
  if (!data->buffer) {
    log_error("Memory allocation error");
    return NULL;
  }
  data->buffer[0] = '\0';
  data->write_buffer = malloc(data->buffer_size * MAX_CSV_SIZE * sizeof(char));
  if (!data->write_buffer) {
    log_error("Memory allocation error");
    return NULL;
  }
  snprintf(data->filename, MAX_FILENAME_SIZE, "%s/%s_%lu_%s.powmon.dat",
           device_data->job_cwd, device_data->name, device_data->jobId,
           node_power_data->hostname); // Use actual hostname
  return data;
}
void power_tracker_destroy(void **power_tracker_data) {
  // Don't destroy job info as that will be handled by node_manager.
  if (*power_tracker_data == NULL)
    return;
  power_tracker_t *data = (power_tracker_t *)*power_tracker_data;
  if (data->buffer) {
    free(data->buffer);
  }
  if (data->write_buffer)
    free(data->write_buffer);
  node_job_info_destroy((void **)&data->job_info);
  free(data);
  *power_tracker_data = NULL;
}
