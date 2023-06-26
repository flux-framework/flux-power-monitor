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
  return buffer;
}

void circular_buffer_push(circular_buffer_t *buffer, void *data) {
  if (buffer == NULL)
    return;
  if (buffer->current_size >= buffer->max_size) {
    void *old_data = zlist_pop(buffer->list);
    if (buffer->destructor)
      buffer->destructor(old_data);
  } else {
    buffer->current_size++;
  }
  zlist_append(buffer->list, data);
}

void *circular_buffer_pop(circular_buffer_t *buffer) {
  if (buffer == NULL)
    return NULL;
  if (buffer->current_size > 0) {

    buffer->current_size--;
  }
  return zlist_pop(buffer->list);
}

void circular_buffer_set_max_size(circular_buffer_t *buffer, size_t max_size) {
  if (buffer == NULL)
    return;
  while (buffer->current_size > max_size) {
    buffer->destructor(zlist_pop(buffer->list));
    buffer->current_size--;
  }
  buffer->max_size = max_size;
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
    free(buffer);
  }
}
