#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "flux_pwr_logging.h"
static flux_t *h;
void log_message_internal(const char *function_name, int line_number,
                          const char *format, ...) {
  if (h == NULL)
    return;
  va_list args;
  va_start(args, format);
  char buffer[512];
  vsnprintf(buffer, sizeof(buffer), format, args);
  flux_log(h, LOG_CRIT, "Func:%s Line:%d %s", function_name, line_number,
           buffer);
  va_end(args);
}

void log_error_internal(const char *function_name, int line_number,
                        const char *format, ...) {
  if (h == NULL)
    return;
  va_list args;
  va_start(args, format);
  char buffer[512];
  vsnprintf(buffer, sizeof(buffer), format, args);
  flux_log_error(h, "Func : %s Line %d %s", function_name, line_number, buffer);
  va_end(args);
}
void init_flux_pwr_logging(flux_t *flux_handle) { h = flux_handle; }

void send_error(const flux_msg_t *msg, const char *errmsg) {
  if (flux_respond_error(h, msg, errno, errmsg) < 0)
    flux_log_error(h, "flux_respond_error");
}
