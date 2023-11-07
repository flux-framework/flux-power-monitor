#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fftw3.h>
#include "retro_queue_buffer.h"

#define NUM_ELEMENTS 100000

int main() {
    // Create a circular buffer with a max size of 100,000
    retro_queue_buffer_t *buffer = retro_queue_buffer_new(NUM_ELEMENTS, fftw_free);

    // Populate the buffer with 100,000 double values
    for (size_t i = 0; i < NUM_ELEMENTS; i++) {
        double *value = (double *)fftw_malloc(sizeof(double));
        *value = (double)i;
        retro_queue_buffer_push(buffer, value);
    }

    // Check if buffer size matches expectation
    if (retro_queue_buffer_get_current_size(buffer) != NUM_ELEMENTS) {
        fprintf(stderr, "Error: Buffer size doesn't match expected number of elements.\n");
        retro_queue_buffer_destroy(buffer);
        return 1; // exit with an error code
    }

    // Extract and copy the data into an fftw_malloc-allocated block while timing the operation
    clock_t start, end;
    double cpu_time_used;
    double *extracted_data = (double *)fftw_malloc(NUM_ELEMENTS * sizeof(double));

    start = clock();

    zlist_first(buffer->list);
    for (size_t i = 0; i < NUM_ELEMENTS; i++) {
        double *value = (double *)zlist_next(buffer->list);
        if (value) {
            extracted_data[i] = *value;
        } else {
            fprintf(stderr, "Error: value at index %zu is NULL\n", i);
            extracted_data[i] = 0;  // or handle this case as you see fit
        }
    }

    end = clock();

    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    printf("Time taken to extract and copy 100,000 elements: %f seconds\n", cpu_time_used);

    fftw_free(extracted_data);
    retro_queue_buffer_destroy(buffer);
    return 0;
}

