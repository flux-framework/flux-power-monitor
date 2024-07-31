#include <jansson.h>
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "flux_pwr_logging.h"
#include "job_pwr_mgr.h"
#include "node_manager/node_manager.h"
#include "power_policies/uniform_pwr_policy.h"
#include "pwr_info.h"
#include "stdbool.h"
#include "util.h"
bool device_info_fetched;
job_mgr_t *job_self_ref = NULL;
size_t total_number_gpus = 0;
size_t total_number_cpus = 0;
int update_node_powerlimit_rpc(job_mgr_t *job_mgr, flux_t *h,
                               double *power_data) {
  int local_rank = 0;
  for (int i = 0; i < job_mgr->num_of_nodes; i++) {
    local_rank = job_mgr->hostname_rank_mapping[i];
    int power_offset = 0;

    node_device_info_t *current_device = NULL;
    for (int j = 0; j < job_mgr->num_of_nodes; j++) {
      if (job_mgr->device_list[j]->flux_rank == local_rank) {
        current_device = job_mgr->device_list[j];
        log_message("device found breaking %d", local_rank);
        break;
      }
      power_offset += job_mgr->device_list[j]->num_of_gpus;
    }
    if (!current_device) {
      log_error("Device Not found for the node %d", local_rank);
      continue;
    }
    json_t *data =
        node_device_info_to_json(current_device, &power_data[power_offset]);
    if (!data) {
      log_error("device parsing to json failed");
    }
    log_message("sending new power data for node %s and local rank %d",
                job_mgr->node_hostname_list[i], local_rank);

    if (local_rank == 0) {
      for (int k = 0; k < current_device->num_of_gpus; k++)
        node_manager_set_powerlimit(job_mgr->jobId, power_data[power_offset],
                                    current_device->device_id_gpus[k]);
    } else {
      log_message("send powerlimit RPC");
      if (flux_rpc_pack(h, "pwr_mgr.nm-set_pl", local_rank, FLUX_RPC_NORESPONSE,
                        "{s:I s:O}","jobId",job_mgr->jobId, "data", data) < 0) {
        log_error(
            "RPC_ERROR:Unable to send new powelimit to node index: %d and name",
            i, job_mgr->node_hostname_list[i]);
        return -1;
      }
    }

    for (int k = 0; k < current_device->num_of_gpus; k++)
      job_mgr->device_pwr_stats[k + power_offset].powerlimit =
          power_data[k + power_offset];
    json_decref(data);
  }
  return 0;
}

int send_new_job_rpc(flux_t *h, job_mgr_t *job_mgr) {
  // Get the per node power data;
  int errnos;
  errnos = 0;
  if (!h) {
    errnos = -1;
    log_error("NULL FLUX HANDLE");
  }
  double *power_data = calloc(job_mgr->num_of_gpus, sizeof(double));
  if (!power_data) {
    errnos = -1;
    goto cleanup;
  }
  log_message("Number of nodes %ld", job_mgr->num_of_nodes);
  job_mgr->job_pwr_policy->get_power_dist(
      job_mgr->num_of_gpus, job_mgr->device_pwr_stats, job_mgr->powerlimit,
      job_mgr->powerlimit, power_data);
  for (int i = 0; i < job_mgr->num_of_nodes; i++) {
    int local_rank = job_mgr->hostname_rank_mapping[i];
    int power_offset = 0;
    node_device_info_t *current_device = NULL;
    for (int j = 0; j < job_mgr->num_of_nodes; j++) {
      if (job_mgr->device_list[j]->flux_rank == local_rank) {
        current_device = job_mgr->device_list[j];
        break;
      }
      power_offset += job_mgr->device_list[j]->num_of_gpus;
    }
    if (!current_device) {
      log_error("Device Not found for the node %d", local_rank);
      continue;
    }
    log_message("local rank %d", local_rank);
    if (local_rank == 0) {

      node_manager_new_job(job_mgr->jobId, job_mgr->cwd, job_mgr->job_name,
                           current_device);
      for (int k = power_offset; k < power_offset + current_device->num_of_gpus;
           k++) {
        if (node_manager_set_powerlimit(
                job_mgr->jobId, power_data[i],
                current_device->device_id_gpus[k - power_offset]) < 0)
          log_error("ERROR in setting rank 0 node power settings");
      }
      log_message("SET power limit for rank 0");

    } else {

      log_message("checking  sending RPC to rank %d job_mgr "
                  "jobId %ld cwd %s name %s power %f",
                  local_rank, job_mgr->jobId, job_mgr->cwd, job_mgr->job_name,
                  power_data[i]);
      json_t *current_device_json =
          node_device_info_to_json(current_device, &power_data[power_offset]);
      if (!current_device_json) {
        log_error("Creation of node_device_info_json failed");
        json_decref(current_device_json);
        continue;
      }
      log_message("data JSON %s\n", json_dumps(current_device_json, 0));
      flux_future_t *f = flux_rpc_pack(
          h, "pwr_mgr.nm-new_job", local_rank, FLUX_RPC_NORESPONSE,
          "{s:I s:s s:s s:O}", "jobid", job_mgr->jobId, "cwd", job_mgr->cwd,
          "name", job_mgr->job_name, "data", current_device_json);
      if (!f) {
        json_decref(current_device_json);
        log_error("RPC_ERROR:new job notification failure");
        continue;
      }
      json_decref(current_device_json);
    }
  }
cleanup:
  if (power_data)
    free(power_data);
  return errnos;
}

int send_end_job_rpc(flux_t *h, job_mgr_t *job_mgr) {
  for (int i = 0; i < job_mgr->num_of_nodes; i++) {
    log_message("sending end job to each node, current node rank :%d", i);
    int local_rank = job_mgr->hostname_rank_mapping[i];
    if (local_rank == 0) {
      node_manager_finish_job(job_mgr->jobId);
    } else {
      log_message("Sending end job RPC");
      if (flux_rpc_pack(h, "pwr_mgr.nm-end_job",
                        job_mgr->hostname_rank_mapping[i], FLUX_RPC_NORESPONSE,
                        "{s:I}", "jobid", job_mgr->jobId) < 0) {
        log_error("RPC_ERROR:job end rpc failure");
        return -1;
      }
    }
  }
  return 0;
}
job_mgr_t *job_mgr_new(uint64_t jobId, char **nodelist, int num_of_nodes,
                       char *cwd, char *job_name, POWER_POLICY_TYPE pwr_policy,
                       double powerlimit, node_device_info_t **device_data,
                       int *node_indices_para, flux_t *h) {
  bool error = false;
  char *error_string;
  if (!nodelist || num_of_nodes == 0 || !cwd)
    return NULL;
  job_mgr_t *job_mgr = calloc(1, sizeof(job_mgr_t));
  if (!job_mgr) {
    error = true;
    error_string = "job_mgr calloc failed";
    goto cleanup;
  }
  job_mgr->power_history = retro_queue_buffer_new(100, free);
  if (!job_mgr->power_history) {
    error = true;
    error_string = "job_mgr power history failed";
    goto cleanup;
  }
  job_mgr->node_hostname_list = calloc(num_of_nodes, sizeof(char *));
  for (int i = 0; i < num_of_nodes; i++) {
    job_mgr->node_hostname_list[i] = strdup(nodelist[i]);
    if (!job_mgr->node_hostname_list[i]) {
      error_string = "job_mgr node_hostname strdup failed";
      error = true;
      goto cleanup;
    }
  }
  job_mgr->num_of_nodes = num_of_nodes;
  job_mgr->hostname_rank_mapping = calloc(num_of_nodes, sizeof(int));
  if (!job_mgr->hostname_rank_mapping) {
    error = true;
    error_string = "Memory allocation for hostname_rank_mapping failed";
    goto cleanup;
  }

  for (int i = 0; i < num_of_nodes; i++)
    job_mgr->hostname_rank_mapping[i] = node_indices_para[i];

  job_mgr->job_pwr_policy = malloc(sizeof(pwr_policy_t));
  if (!job_mgr->job_pwr_policy) {
    error = true;
    error_string = "Memory allocation for job_pwr_mgr failed";
    goto cleanup;
  }
  if (pwr_policy == UNIFORM) {
    uniform_pwr_policy_init(job_mgr->job_pwr_policy);
  }
  job_mgr->jobId = jobId;
  job_mgr->powerlimit = powerlimit;
  job_mgr->cwd = strdup(cwd);
  job_mgr->job_name = strdup(job_name);
  job_mgr->device_list = device_data;
  for (int i = 0; i < num_of_nodes; i++) {
    total_number_gpus += device_data[i]->num_of_gpus;
    total_number_cpus += device_data[i]->num_of_cores;
    job_mgr->num_of_cpus += device_data[i]->num_of_cores;
    job_mgr->num_of_gpus += device_data[i]->num_of_gpus;
  }
  log_message("number of devices %d", job_mgr->num_of_gpus);

  job_mgr->device_pwr_stats = calloc(job_mgr->num_of_gpus, sizeof(pwr_stats_t));
  if (!job_mgr->device_pwr_stats) {
    error = true;
    error_string = "job mgr device pwr stats is null";
    goto cleanup;
  }
  job_self_ref = job_mgr;
  // Get device info for each node

  send_new_job_rpc(h, job_mgr);
cleanup:

  if (error && job_mgr) {
    log_error("Job creating failed");
    log_error("Error message %s", error_string);
    if (job_mgr->power_history)
      retro_queue_buffer_destroy(job_mgr->power_history);

    if (job_mgr->node_hostname_list) {
      for (int i = 0; i < num_of_nodes; i++) {
        free(job_mgr->node_hostname_list[i]);
      }
      free(job_mgr->node_hostname_list);
    }

    free(job_mgr->hostname_rank_mapping);
    free(job_mgr->job_pwr_policy);
    free(job_mgr->cwd);
    free(job_mgr->job_name);
    free(job_mgr);
  }
  return job_mgr;
}
int job_mgr_update_powerlimit(job_mgr_t *job_mgr, flux_t *h,
                              double new_powerlimit) {

  int errnos = 0;
  if (!job_mgr || new_powerlimit == 0)
    errnos = -1;
  double current_powerlimit = 0;
  job_mgr->powerlimit = new_powerlimit;
  for (int i = 0; i < job_mgr->num_of_gpus; i++)
    current_powerlimit += job_mgr->device_pwr_stats[i].powerlimit;
  double *power_data = calloc(job_mgr->num_of_gpus, sizeof(double));

  if (!power_data) {
    errnos = -1;
    goto cleanup;
  }

  job_mgr->job_pwr_policy->get_power_dist(
      job_mgr->num_of_gpus, job_mgr->device_pwr_stats, new_powerlimit,
      current_powerlimit, power_data);
  if (update_node_powerlimit_rpc(job_mgr, h, power_data) < 0) {
    log_error("Unable to update powerlimit");
    errnos = -1;
    goto cleanup;
  }
cleanup:
  if (power_data)
    free(power_data);
  power_data = NULL;
  return errnos;
}

void job_mgr_destroy(flux_t *h, job_mgr_t **job) {
  if (!job || !*job)
    return;

  job_mgr_t *proper_job = *job;

  send_end_job_rpc(h, proper_job);

  if (proper_job->node_hostname_list) {
    for (int i = 0; i < proper_job->num_of_nodes; i++) {
      free(proper_job->node_hostname_list[i]);
    }
    free(proper_job->node_hostname_list);
  }
  if (proper_job->device_list)
    for (int i = 0; i < proper_job->num_of_nodes; i++) {
      free(proper_job->device_list[i]->hostname);
      free(proper_job->device_list[i]);
    }
  free(proper_job->device_list);
  retro_queue_buffer_destroy(proper_job->power_history);
  free(proper_job->device_pwr_stats);
  free(proper_job->hostname_rank_mapping);
  free(proper_job->job_name);
  free(proper_job->cwd);
  free(proper_job);
  *job = NULL; // Set the pointer to NULL to avoid dangling references.
  job_self_ref = NULL;
}
