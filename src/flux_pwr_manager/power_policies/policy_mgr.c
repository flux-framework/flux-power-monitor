#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "fft_based_power_policy.h"
#include "policy_mgr.h"
#include "uniform_pwr_policy.h"
#define POWER_HISTORY_BUFFER 100
pwr_policy_t *pwr_policy_new(POWER_POLICY_TYPE policy_type){
  pwr_policy_t *mgr = malloc(sizeof(pwr_policy_t));
  if (mgr == NULL)
    return NULL;
  if (policy_type == FFT) {
    fft_pwr_policy_init(mgr);
  } else if (policy_type == UNIFORM) {
    uniform_pwr_policy_init(mgr);
  }
  mgr->powercap_history=retro_queue_buffer_new(POWER_HISTORY_BUFFER,free);
  mgr->powerlimit_history=retro_queue_buffer_new(POWER_HISTORY_BUFFER,free);
  mgr->time_history=retro_queue_buffer_new(POWER_HISTORY_BUFFER,free);
  mgr->converged=false;
  if (mgr->powerlimit_history==NULL || mgr->powercap_history==NULL || mgr->time_history==NULL)
    return NULL;
  return mgr;
}
void  pwr_policy_destroy(pwr_policy_t **pwr_mgr){
  if(pwr_mgr==NULL)
    return;
  pwr_policy_t* mgr=*pwr_mgr;
  if(mgr==NULL)
    return;
  if(mgr->powercap_history==NULL){
    free(pwr_mgr);
    return;}
  if(mgr->powerlimit_history==NULL){
    retro_queue_buffer_destroy(mgr->powercap_history);
    free(pwr_mgr);
  return ;
  }
    retro_queue_buffer_destroy(mgr->powercap_history);
    retro_queue_buffer_destroy(mgr->powerlimit_history);
    retro_queue_buffer_destroy(mgr->time_history);
  free(pwr_mgr);
  return;
}
