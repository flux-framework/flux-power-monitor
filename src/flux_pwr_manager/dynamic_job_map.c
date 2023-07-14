#if HAVE_CONFIG_H
#include "config.h"
#endif
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
int add_to_job_map(dynamic_job_map *job_map, job_map_entry new_entry) {
  if (job_map->size == job_map->capacity) {
    size_t new_capacity = job_map->capacity * 2;
    job_map_entry *new_entries =
        realloc(job_map->entries, new_capacity * sizeof(job_map_entry));
    if (!new_entries) {
      return -1; // Failed to allocate memory
    }
    job_map->entries = new_entries;
    job_map->capacity = new_capacity;
  }

  job_map->entries[job_map->size++] = new_entry;
  return 0; // Success
}

void remove_from_job_map(dynamic_job_map *job_map, size_t index) {
  if (index >= job_map->size) {
    return;
  }

  job_data_destroy(job_map->entries[index].data);

  for (size_t i = index + 1; i < job_map->size; i++) {
    job_map->entries[i - 1] = job_map->entries[i];
  }
  job_map->size--;

  if (job_map->size > job_map->min_capacity &&
      job_map->size < job_map->capacity / 4) {
    size_t new_capacity = job_map->capacity / 2;
    job_map_entry *new_entries =
        realloc(job_map->entries, new_capacity * sizeof(job_map_entry));
    if (!new_entries) {
      return; // Failed to allocate memory
    }
    job_map->entries = new_entries;
    job_map->capacity = new_capacity;
  }

  if (job_map->capacity < job_map->min_capacity) {
    size_t new_capacity = job_map->min_capacity;
    job_map_entry *new_entries =
        realloc(job_map->entries, new_capacity * sizeof(job_map_entry));
    if (!new_entries) {
      return; // Failed to allocate memory
    }
    job_map->entries = new_entries;
    job_map->capacity = new_capacity;
  }
}
