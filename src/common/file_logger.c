#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "file_logger.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

void reset_buffer(Logger *log) {
    if (log == NULL || log->buffer == NULL)
        return;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        memset(log->buffer[i], 0, DATA_SIZE);
    }
    log->buffer_location = 0;
}

void init_buffer(Logger *log) {
    if (log == NULL)
        return;
    log->buffer = (char **)malloc(sizeof(char *) * BUFFER_SIZE);
    if (log->buffer == NULL) {
        printf("Memory error in buffer allocation\n");
        return;
    }
    for (int i = 0; i < BUFFER_SIZE; i++) {
        log->buffer[i] = (char *)malloc(sizeof(char) * DATA_SIZE);
        if (log->buffer[i] == NULL) {
            printf("Memory error in buffer row allocation\n");
            for (int j = 0; j < i; j++) {
                free(log->buffer[j]);
            }
            free(log->buffer);
            log->buffer = NULL;
            return;
        }
    }
    reset_buffer(log);
}

void destroy_buffer(Logger *log) {
    if (log == NULL || log->buffer == NULL)
        return;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        free(log->buffer[i]);
    }
    free(log->buffer);
    log->buffer = NULL;
}

void write_file(Logger *log) {
    if (log == NULL || log->buffer == NULL)
        return;
    FILE *file = fopen(log->filename, "a");
    if (file == NULL) {
        printf("Failed to open file\n");
        return;
    }
    for (int i = 0; i < log->buffer_location; i++) {
        fprintf(file, "%s", log->buffer[i]);
    }
    fclose(file);
    reset_buffer(log);
}

Logger *file_logger_new(char *filename, int filename_len) {
    if (filename == NULL || filename_len == 0 || filename_len > MAX_FILENAME_LENGTH) {
        printf("Invalid filename\n");
        return NULL;
    }
    Logger *log = (Logger *)malloc(sizeof(Logger));
    if (!log) {
        printf("Memory error in logger allocation\n");
        return NULL;
    }
    log->filename = strdup(filename);
    init_buffer(log);
    if (log->buffer == NULL) { // Check if buffer was successfully initialized
        free(log->filename);
        free(log);
        return NULL;
    }
    return log;
}

void file_logger_destroy(Logger **log) {
    if (log != NULL && *log != NULL) {
        if ((*log)->buffer != NULL) {
            write_file(*log);
            destroy_buffer(*log);
        }
        free((*log)->filename);
        free(*log);
        *log = NULL;
    }
}

int file_logger_add_data_to_buffer(Logger *log, char *key, int key_len, char *data, int data_len) {
    if (log == NULL || log->buffer == NULL)
        return -1;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t timestamp = (tv.tv_sec * (uint64_t)1000) + (tv.tv_usec / 1000);
    int required_size = key_len + data_len + 37; // Adjusted buffer size requirement
    if (required_size > DATA_SIZE) {
        printf("Data too big to write\n");
        return -1;
    }
    if (log->buffer_location >= BUFFER_SIZE) {
        write_file(log);
    }
    snprintf(log->buffer[log->buffer_location], DATA_SIZE, "{\"%s\":\"%s\",time:%lu}", key, data, timestamp);
    log->buffer_location++;
    return 0;
}

