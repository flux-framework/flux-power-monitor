#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "retro_queue_buffer.h"
#include <pthread.h>
retro_queue_buffer_t *retro_queue_buffer_new(size_t max_size,
                                             destructor_fn destructor) {
  retro_queue_buffer_t *buffer =
      (retro_queue_buffer_t *)malloc(sizeof(retro_queue_buffer_t));
  buffer->list = zlist_new();
  if (buffer == NULL)
    return NULL;
  buffer->max_size = max_size;
  buffer->current_size = 0;
  buffer->destructor = destructor;
  pthread_mutex_init(&buffer->mutex, NULL); // Initialize the mutex

  return buffer;
}

void retro_queue_buffer_push(retro_queue_buffer_t *buffer, void *data) {
  if (buffer == NULL)
    return;

  pthread_mutex_lock(&buffer->mutex); // Ensure thread safety
  if (buffer->current_size >= buffer->max_size) {
    void *old_data = zlist_pop(buffer->list);
    if (buffer->destructor)
      buffer->destructor(old_data);
  } else {
    buffer->current_size++;
  }
  zlist_append(buffer->list, data);

  pthread_mutex_unlock(&buffer->mutex); // Unlock before returning
}

void *retro_queue_buffer_pop(retro_queue_buffer_t *buffer) {
  if (buffer == NULL)
    return NULL;
  if (buffer->current_size > 0) {

    buffer->current_size--;
  }
  return zlist_pop(buffer->list);
}

size_t retro_queue_buffer_get_current_size(retro_queue_buffer_t *buffer) {
  if (buffer == NULL)
    return -1;
  return buffer->current_size;
}
size_t retro_queue_buffer_get_max_size(retro_queue_buffer_t *buffer) {
  if (buffer == NULL)
    return -1;

  return buffer->max_size;
}

void retro_queue_buffer_destroy(retro_queue_buffer_t *buffer) {
  if (buffer == NULL)
    return;
  if (buffer != NULL) {
    if (buffer->list) {
      while (zlist_size(buffer->list) > 0) {
        void *item = zlist_pop(buffer->list);
        if (buffer->destructor)
          buffer->destructor(item);
      }

      zlist_destroy(&buffer->list);
    }
    buffer->list = NULL;
    pthread_mutex_destroy(&buffer->mutex);
    free(buffer);
  }
}
void *retro_queue_buffer_iterate_from(retro_queue_buffer_t *buffer,
                                      comparator_fn comp, void *target,
                                      iterator_fn elem_func, void *user_data,
                                      size_t num_elements) {
  if (buffer == NULL || buffer->current_size == 0)
    return NULL;

  pthread_mutex_lock(&buffer->mutex);

  int counter = 0;
  void *last_element = NULL;
  void *element = zlist_first(buffer->list);
  size_t processed_elements = 0;
  bool start_found = false;

  // Attempt to find the starting element
  while (element) {
    if (comp && comp(element, target)) {
      start_found = true;
      break;
    }
    counter++;

    element = zlist_next(buffer->list);
  }

  if (!start_found) {
    counter = 0;

    // Reset to start from the beginning if no element was found
    element = zlist_first(buffer->list);
  }

  // Iterate from the found element or from the start, up to num_elements
  while (element && processed_elements < num_elements) {
    last_element = element;
    if (elem_func != NULL) {
      elem_func(last_element, user_data);
    }
    element = zlist_next(buffer->list);
    processed_elements++;
  }

  pthread_mutex_unlock(&buffer->mutex);

  return last_element;
}
void *retro_queue_buffer_iterate_until_before_tail(retro_queue_buffer_t *buffer,
                                                   comparator_fn comp,
                                                   void *comp_criterion,
                                                   iterator_fn elem_func,
                                                   void *user_data) {
  if (buffer == NULL ||
      buffer->current_size <=
          1) // Ensure there's at least a tail and one other element
    return NULL;
  pthread_mutex_lock(&buffer->mutex);
  void *last_element = NULL;
  void *next_element = NULL;
  void *element = zlist_first(buffer->list);
  bool start_found = false;
  // Attempt to find the starting element
  while (element) {
    if (comp && comp(element, comp_criterion)) {
      start_found = true;
      break;
    }
    element = zlist_next(buffer->list);
  }
  if (!start_found) {
    // Reset to start from the beginning if no element was found
    element = zlist_first(buffer->list);
  }

  // Iterate from the found element to the one before the tail
  while (element) {
    next_element = zlist_next(buffer->list);
    if (!next_element) { // If next is NULL, current is the tail; stop
                         // processing and prepare to return it
      break;
    }

    last_element = element;
    if (elem_func != NULL) {
      elem_func(last_element, user_data);
    }

    element = next_element;
  }

  pthread_mutex_unlock(&buffer->mutex);

  return element; // Return the tail element
}
