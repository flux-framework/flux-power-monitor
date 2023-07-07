#ifndef FLUX_DYNAMIC_JOB_MAP_H
#define FLUX_DYNAMIC_JOB_MAP_H
#include "job_data.h"
typedef struct {
  char *jobId;
  job_data *data;
} job_map_entry;

typedef struct {
  job_map_entry *entries;
  size_t size;         // Number of jobs currently stored
  size_t capacity;     // Allocated size of the array
  size_t min_capacity; // Minimum size of the array
} dynamic_job_map;

dynamic_job_map *init_job_map(size_t initial_capacity);
void resize_job_map(dynamic_job_map *job_map, size_t new_capacity);
void add_to_job_map(dynamic_job_map *job_map, job_map_entry new_entry);
void remove_from_job_map(dynamic_job_map *job_map, size_t index);
#endif
