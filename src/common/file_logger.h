#ifndef FLUX_PWR_FILE_LOGGER_H
#define FLUX_PWR_FILE_LOGGER_H
#include <stdio.h>
#define BUFFER_SIZE 100
#define DATA_SIZE 500
#define MAX_FILENAME_LENGTH 20
typedef struct{
  char *filename;
  char **buffer;
  int buffer_location;
}Logger;


Logger* file_logger_new(char *filename,int filename_len);
void file_logger_destroy(Logger **logger);
int file_logger_add_data_to_buffer(Logger *logger,char *key,int key_len,char *data,int data_len);

#endif
