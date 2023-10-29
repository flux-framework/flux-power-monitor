#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "circular_buffer.h"

circular_buffer_t *circular_buffer_new(size_t max_size,
                                       destructor_fn destructor) {
  circular_buffer_t *buffer =
      (circular_buffer_t *)malloc(sizeof(circular_buffer_t));
  buffer->list = zlist_new();
  if (buffer == NULL)
    return NULL;
  buffer->max_size = max_size;
  buffer->current_size = 0;
  buffer->destructor = destructor;
  pthread_mutex_init(&buffer->mutex, NULL); // Initialize the mutex

  return buffer;
}

void circular_buffer_push(circular_buffer_t *buffer, void *data) {
  if (buffer == NULL)
    return;
  pthread_mutex_lock(&buffer->mutex); // Lock the mutex

  if (buffer->current_size >= buffer->max_size) {
    void *old_data = zlist_pop(buffer->list);
    if (buffer->destructor)
      buffer->destructor(old_data);
  } else {
    buffer->current_size++;
  }
  zlist_append(buffer->list, data);
  pthread_mutex_unlock(&buffer->mutex); // Unlock the mutex
}

void *circular_buffer_pop(circular_buffer_t *buffer) {
  if (buffer == NULL)
    return NULL;
  if (buffer->current_size > 0) {

    buffer->current_size--;
  }
  return zlist_pop(buffer->list);
}


size_t circular_buffer_get_current_size(circular_buffer_t *buffer) {
  if (buffer == NULL)
    return -1;
  return buffer->current_size;
}
size_t circular_buffer_get_max_size(circular_buffer_t *buffer) {
  if (buffer == NULL)
    return -1;

  return buffer->max_size;
}

void circular_buffer_destroy(circular_buffer_t *buffer) {
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
void circular_buffer_iterate_partial(circular_buffer_t *buffer,
                                     circular_buffer_iterator_fn fn,
                                     void *user_data, size_t start_index,
                                     size_t end_index) {
  if (buffer == NULL || fn == NULL)
    return;

  pthread_mutex_lock(&buffer->mutex); // Lock the buffer

  // Special case: if both start_index and end_index are 0, iterate over the
  // entire buffer
  if (start_index == 0 && end_index == 0) {
    start_index = 0;
    end_index = buffer->max_size - 1;
  } else {
    // Calculate actual start and end indices based on the circular nature
    start_index = start_index % buffer->max_size;
    end_index = end_index % buffer->max_size;
  }

  size_t current_index = 0;
  void *current_item;

  for (current_item = zlist_first(buffer->list); current_item != NULL;
       current_item = zlist_next(buffer->list)) {
    if ((current_index >= start_index) && (current_index <= end_index)) {
      fn(current_item,
         user_data); // Call the callback function with the generic item
    }
    current_index++;
    if (current_index > end_index && start_index <= end_index) {
      break; // Stop if we've reached the end index and it's not a wrap-around
             // case
    }
    if (current_index >= buffer->max_size) {
      current_index = 0; // Wrap around
    }
  }

  pthread_mutex_unlock(&buffer->mutex); // Unlock the buffer
}
