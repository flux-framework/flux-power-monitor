#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H
#include <czmq.h>

typedef void (*destructor_fn)(void *);
typedef struct {
  zlist_t *list;
  size_t max_size;
  size_t current_size;
  destructor_fn destructor;
} circular_buffer_t;

circular_buffer_t *circular_buffer_new(size_t max_size,
                                       destructor_fn destructor);
void ciruclar_buffer_free(circular_buffer_t *buffer);
void circular_buffer_push(circular_buffer_t *buffer,void* data);
void* circular_buffer_pop(circular_buffer_t *buffer);
void circular_buffer_set_max_size(circular_buffer_t *buffer,size_t max_size);
size_t circular_buffer_get_max_size(circular_buffer_t* buffer);
size_t circular_buffer_get_current_size(circular_buffer_t* buffer);
void circular_buffer_destroy(circular_buffer_t* buffer);
#endif
