#include "dynamic_job_map.h"

// Initialize a dynamic job map with a given capacity
dynamic_job_map *init_job_map(size_t initial_capacity) {
  dynamic_job_map *job_map = malloc(sizeof(dynamic_job_map));
  job_map->entries = malloc(initial_capacity * sizeof(job_map_entry));
  job_map->size = 0;
  job_map->capacity = initial_capacity;
  job_map->min_capacity = initial_capacity;

  return job_map;
}
// Resize a dynamic job map
void resize_job_map(dynamic_job_map *job_map, size_t new_capacity) {
  job_map->entries =
      realloc(job_map->entries, new_capacity * sizeof(job_map_entry));
  job_map->capacity = new_capacity;
}

// Add a new entry to a dynamic job map
void add_to_job_map(dynamic_job_map *job_map, job_map_entry new_entry) {
  if (job_map->size == job_map->capacity) {
    resize_job_map(job_map, job_map->capacity * 2);
  }
  job_map->entries[job_map->size++] = new_entry;
}
// Remove an entry from a dynamic job map
void remove_from_job_map(dynamic_job_map *job_map, size_t index) {
  free(job_map->entries[index].jobId);
  job_data_destroy(job_map->entries[index].data);

  for (size_t i = index + 1; i < job_map->size; i++) {
    job_map->entries[i - 1] = job_map->entries[i];
  }
  job_map->size--;

  // Shrink the array if it's less than a quarter full, but not smaller than
  // min_capacity
  if (job_map->size > job_map->min_capacity &&
      job_map->size < job_map->capacity / 4) {
    resize_job_map(job_map, job_map->capacity / 2);
  }

  if (job_map->capacity < job_map->min_capacity) {
    resize_job_map(job_map, job_map->min_capacity);
  }
}
