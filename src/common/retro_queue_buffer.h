#ifndef FLUX_PWR_MANAGER_RETRO_QUEUE_BUFFER_H
#define FLUX_PWR_MANAGER_RETRO_QUEUE_BUFFER_H
#include <czmq.h>

typedef void (*destructor_fn)(void *);
typedef void (*iterator_fn)(void *element, void *user_data);
typedef int (*comparator_fn)(void *element, void *criterion);

typedef struct {
  zlist_t *list;
  size_t max_size;
  size_t current_size;
  pthread_mutex_t mutex;
  destructor_fn destructor;
} retro_queue_buffer_t;

retro_queue_buffer_t *retro_queue_buffer_new(size_t max_size,
                                             destructor_fn destructor);
void ciruclar_buffer_free(retro_queue_buffer_t *buffer);
void retro_queue_buffer_push(retro_queue_buffer_t *buffer, void *data);
void *retro_queue_buffer_pop(retro_queue_buffer_t *buffer);
size_t retro_queue_buffer_get_max_size(retro_queue_buffer_t *buffer);
size_t retro_queue_buffer_get_current_size(retro_queue_buffer_t *buffer);
void retro_queue_buffer_destroy(retro_queue_buffer_t *buffer);
void *retro_queue_buffer_iterate_from(retro_queue_buffer_t *buffer,
                                      comparator_fn comp, void *target,
                                      iterator_fn elem_func, void *user_data,
                                      size_t num_elements);

void *retro_queue_buffer_iterate_until_before_tail(retro_queue_buffer_t *buffer,
                                                   comparator_fn comp,
                                                   void *comp_criterion,
                                                   iterator_fn elem_func,
                                                   void *user_data) ;
#endif
