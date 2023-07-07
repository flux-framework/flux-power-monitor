#include <jansson.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    double mem_power;
    double cpu_power;
    double node_power;
    double gpu_power;
    uint64_t timestamp;
} power_data;

typedef struct {
    power_data* data;
    char* hostname;
} power_node_map;

power_node_map* power_node_map_new(power_data* power_data,char* hostname);
void power_node_map_destroy(power_node_map* data);
