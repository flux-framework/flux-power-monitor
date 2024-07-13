#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "flux_pwr_logging.h"
#include <flux/core.h>
#include <flux/jobtap.h>
flux_t *h;
static int jobtap_cb(flux_plugin_t *p, const char *topic,
                     flux_plugin_arg_t *args, void *arg) {
  size_t userid;
  uint64_t id;
  double t_submit;

  if (flux_plugin_arg_unpack(args, FLUX_PLUGIN_ARG_IN, "{s:I s:f s:I}", "id",
                             &id, "t_submit", &t_submit, "userid", &userid) < 0)
    return -1;

  // log_message("JOBTAP:Topic: %s User Id : %d, t_submit: %f and jobId %ld", topic,
              // userid, t_submit, id);
  if (flux_rpc_pack(h, "pwr_mgr.job_notify", FLUX_NODEID_ANY, 0,
                    "{s:s s:I s:f s:I}", "topic", topic, "id", id, "t_submit",
                    t_submit, "userId", userid) < 0) {
    log_error("JOBTAP:Cannot send RPC at jobtap_cb module");
  }
  return 0;
}
static void destructor(void *args) {
  bool terminated = true;
  if (flux_rpc_pack(h, "pwr_mgr.jobtap_destructor_notify",
                    FLUX_NODEID_ANY, 0, "{s:b}", "terminated",
                    terminated) < 0) {
    log_error("JOBTAP:Unable to send RPC for jobtap destructor");
  }
}

int flux_plugin_init(flux_plugin_t *p) {
  flux_plugin_set_name(p, "JOB_NOTIFY");
  h = flux_jobtap_get_flux(p);
  init_flux_pwr_logging(h);
  if (flux_plugin_aux_set(p, NULL, p, destructor) < 0) {

    destructor(NULL);
    return -1;
  }

  if (
      flux_plugin_add_handler(p, "job.inactive-add", jobtap_cb, NULL) < 0 ||
      flux_plugin_add_handler(p, "job.state.run", jobtap_cb, NULL) < 0
  )
    return -1;

  return 0;
}
