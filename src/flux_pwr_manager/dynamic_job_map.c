#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "dynamic_job_map.h"
#include <errno.h>
#include <unistd.h>
#include <wchar.h>

// Initialize a dynamic job map with a given capacity
dynamic_job_map *init_job_map(size_t initial_capacity) {
  dynamic_job_map *job_map = malloc(sizeof(dynamic_job_map));
  if (job_map == NULL) {
    perror("Failed to allocate memory for job map");
    return NULL;
  }

  job_map->entries = malloc(initial_capacity * sizeof(job_map_entry));
  if (job_map->entries == NULL) {
    perror("Failed to allocate memory for job map entries");
    free(job_map);
    return NULL;
  }

  job_map->size = 0;
  job_map->capacity = initial_capacity;
  job_map->min_capacity = initial_capacity;

  return job_map;
}

// Resize a dynamic job map
int resize_job_map(dynamic_job_map *job_map, size_t new_capacity) {
  job_map_entry *new_entries =
      realloc(job_map->entries, new_capacity * sizeof(job_map_entry));
  if (new_entries == NULL) {
    perror("Failed to allocate memory for resizing job map");
    return -1;
  }

  job_map->entries = new_entries;
  job_map->capacity = new_capacity;
  return 0;
}

// Add a new entry to a dynamic job map
int add_to_job_map(dynamic_job_map *job_map, job_map_entry new_entry) {
  if (job_map->size == job_map->capacity) {
    size_t new_capacity = job_map->capacity * 2;
    if (resize_job_map(job_map, new_capacity) == -1) {
      perror("Failed to allocate memory for adding to job map");
      return -1;
    }
  }

  job_map->entries[job_map->size++] = new_entry;
  return 0;
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
    if (resize_job_map(job_map, new_capacity) == -1) {
      perror("Failed to allocate memory for removing from job map");
    }
  }

  if (job_map->capacity < job_map->min_capacity) {
    size_t new_capacity = job_map->min_capacity;
    if (resize_job_map(job_map, new_capacity) == -1) {
      perror("Failed to allocate memory for resizing job map to min capacity");
    }
  }
}

void dynamic_job_map_destroy(dynamic_job_map *job_map) {
  if (job_map == NULL) {
    return;
  }

  // Free the entries array
  if (job_map->entries != NULL) {
    for (size_t i = 0; i < job_map->size; i++) {
      if (job_map->entries[i].data != NULL)
        job_data_destroy(job_map->entries[i].data);
    }
    free(job_map->entries);
    job_map->entries = NULL;
  }

  // Free the job map itself
  free(job_map);
}
