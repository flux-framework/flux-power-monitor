#ifndef FLUX_POWER_LOGGING_H
#define FLUX_POWER_LOGGING_H
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <flux/core.h>
void init_flux_pwr_logging(flux_t *flux_handle);
void log_message(const char *format, ...);
void log_error(const char *format, ...);
void send_error(const flux_msg_t *msg, const char *errmsg);
#endif
