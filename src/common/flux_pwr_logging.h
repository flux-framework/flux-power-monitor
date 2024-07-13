#ifndef FLUX_PWR_MANAGER_POWER_LOGGING_H
#define FLUX_PWR_MANAGER_POWER_LOGGING_H
#include <flux/core.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#define log_message(format, ...)                                               \
  log_message_internal(__func__,__LINE__, format, ##__VA_ARGS__)

#define log_error(format, ...) \
    log_error_internal(__func__, __LINE__, format, ##__VA_ARGS__)
void init_flux_pwr_logging(flux_t *flux_handle);
void log_message_internal(const char *function_name, int line_number,
                          const char *format, ...);
void log_error_internal(const char *function_name, int line_number,
                        const char *format, ...);
void send_error(const flux_msg_t *msg, const char *errmsg);
#endif
