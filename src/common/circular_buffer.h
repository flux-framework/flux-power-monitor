#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H
#include <czmq.h>
#include <pthread.h>

typedef void (*destructor_fn)(void *);
typedef void (*circular_buffer_iterator_fn)(void *, void *);

typedef struct {
  zlist_t *list;
  size_t max_size;
  size_t current_size;
  pthread_mutex_t mutex;

  destructor_fn destructor;
} circular_buffer_t;

circular_buffer_t *circular_buffer_new(size_t max_size,
                                       destructor_fn destructor);
void ciruclar_buffer_free(circular_buffer_t *buffer);
void circular_buffer_push(circular_buffer_t *buffer, void *data);
void *circular_buffer_pop(circular_buffer_t *buffer);
size_t circular_buffer_get_max_size(circular_buffer_t *buffer);
size_t circular_buffer_get_current_size(circular_buffer_t *buffer);
void circular_buffer_destroy(circular_buffer_t *buffer);
void circular_buffer_iterate_partial(circular_buffer_t *buffer,
                                     circular_buffer_iterator_fn fn,
                                     void *user_data, size_t start_index,
                                     size_t end_index);
#endif
