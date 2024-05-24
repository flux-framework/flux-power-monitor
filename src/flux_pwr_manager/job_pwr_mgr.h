#ifndef FLUX_PWR_MANAGER_JOB_PWR_MANAGER_H
#define FLUX_PWR_MANAGER_JOB_PWR_MANAGER_H
#include "constants.h"
#include "power_policies/policy_mgr.h"
#include "power_policies/power_policy.h"
#include "pwr_info.h"
#include "pwr_stats.h"
#include "retro_queue_buffer.h"
#include "parse_util.h"
#include <flux/core.h>
#include <unistd.h>

typedef struct {
  uint64_t jobId;
  int num_of_nodes; // Num of nodes in job
  char *cwd;
  char *job_name;
  char **node_hostname_list;    // node hostname list for each node in the job.
  double powerlimit;            // powerlimit for the job.
  pwr_policy_t *job_pwr_policy; // power policy for the job.
  int *hostname_rank_mapping;   // ranking between node and rank.
  retro_queue_buffer_t *power_history; // Updated by callback
  pwr_info pwr_data;                   // Updated by callback
  pwr_stats_t *device_pwr_stats;        // Updated by callback
  node_device_info_t **device_list; //list of devices associated with this job
  int num_of_gpus;
  int num_of_cpus;

} job_mgr_t;
/**
 * @brief A constructor method that creates a new job_mgr.
 * @para jobId a jobid gotten from the flux system.
 * @para nodelist a char list of all the hostname in the job.
 * @para num_of_nodes num of nodes in the job.
 * @para cwd the working directory of the job.
 * @para pwr_policy enum value that denotes the current job
 * power policy, set at first by cluster_mgr.
 * @para powerlimit the powerlimit of the job, set by cluster_mgr.
 * @para device_data holds the device information about the job.
 * @para node_index mapping between hostname and the flux broker rank.
 * @para h: flux handle required to make RPC.
 * @returns A pointer to the new job_mgr_t.
 **/
//TODO: Remove power_ratio, setting power ratio for a whole job is wrong.
// Currently just setting a single value.
job_mgr_t *job_mgr_new(uint64_t jobId, char **nodelist, int num_of_nodes,
                       char *cwd, char *job_name, POWER_POLICY_TYPE pwr_policy,
                       double powerlimit,node_device_info_t **device_data,  int *node_index, flux_t *h);
/**
 * @brief Destructor for job_mgr.
 * @para h flux handle.
 * @para job_mgr_t **job: the job object that is getting destoyed.
 **/
void job_mgr_destroy(flux_t *h, job_mgr_t **job);

/**
 * @brief This method update the @para job_mgr with the given @para
 *new_powerlimit.
 * @param job_mgr job_mgr_handle, denotes the job whose values need
 * to change.
 * @param flux handle needed to send RPC to each node about its new
 * powerlimit.
 * @para new_powerlimit the job overeall powerlimit, generally it will
 * be a fixed power per node * num_of_nodes
 * @returns the status.
 **/
int job_mgr_update_powerlimit(job_mgr_t *job_mgr, flux_t *h,
                              double new_powerlimit);
// should be between 0-100
void broadcast_node_power_ratio(int power_ratio);

void manage_power_capping();

#endif
