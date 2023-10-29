#include "flux_pwr_logging.h"
static flux_t *h;
void log_message(const char *format, ...) {
  if (h == NULL)
    return;
  va_list args;
  va_start(args, format);
  char buffer[512];
  vsnprintf(buffer, sizeof(buffer), format, args);
  flux_log(h, LOG_CRIT, "%s", buffer);
  va_end(args);
}

void log_error(const char *format, ...) {
  if (h == NULL)
    return;
  va_list args;
  va_start(args, format);
  char buffer[512];
  vsnprintf(buffer, sizeof(buffer), format, args);
  flux_log_error(h, "%s", buffer);
  va_end(args);
}
void init_flux_pwr_logging(flux_t *flux_handle) { h = flux_handle; }

void send_error(const flux_msg_t *msg, const char *errmsg) {
  if (flux_respond_error(h, msg, errno, errmsg) < 0)
    flux_log_error(h, "flux_respond_error");
}
