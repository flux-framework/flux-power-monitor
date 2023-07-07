#include "job_power_util.h"
#include <jansson.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


power_hostname_map* parse_power_data(json_t* json_root, size_t* length) {
    json_t *json = json_object_get(json_root, "data");
    if (!json_is_array(json)) {
        fprintf(stderr, "Invalid JSON: 'data' is not an array\n");
        return NULL;
    }

    *length = json_array_size(json);
    power_hostname_map* maps = malloc(*length * sizeof(power_hostname_map));
    if (maps == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }
    
    for (size_t i = 0; i < *length; i++) {
        json_t* entry = json_array_get(json, i);
        if (!json_is_object(entry)) {
            fprintf(stderr, "Invalid JSON: entry not an object\n");
            free(maps);
            return NULL;
        }

        json_t* node_power_data = json_object_get(entry, "node_power_data");
        if (!json_is_object(node_power_data)) {
            fprintf(stderr, "Invalid JSON: no node_power_data object\n");
            free(maps);
            return NULL;
        }

        maps[i].data = malloc(sizeof(power_data));
        if (maps[i].data == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            free(maps);
            return NULL;
        }

        json_t* mem_power = json_object_get(node_power_data, "mem_power");
        json_t* cpu_power = json_object_get(node_power_data, "cpu_power");
        json_t* node_power = json_object_get(node_power_data, "node_power");
        json_t* gpu_power = json_object_get(node_power_data, "gpu_power");
        json_t* result_start_time = json_object_get(node_power_data, "result_start_time");

        if (!json_is_real(mem_power) || !json_is_real(cpu_power) || !json_is_real(node_power) || !json_is_real(gpu_power) || !json_is_integer(result_start_time)) {
            fprintf(stderr, "Invalid JSON: bad or missing data\n");
            free(maps[i].data);
            free(maps);
            return NULL;
        }

        maps[i].data->mem_power = json_real_value(mem_power);
        maps[i].data->cpu_power = json_real_value(cpu_power);
        maps[i].data->node_power = json_real_value(node_power);
        maps[i].data->gpu_power = json_real_value(gpu_power);
        maps[i].data->timestamp = json_integer_value(result_start_time);

        json_t* hostname = json_object_get(entry, "hostname");
        if (!json_is_string(hostname)) {
            fprintf(stderr, "Invalid JSON: hostname not a string\n");
            free(maps[i].data);
            free(maps);
            return NULL;
        }
        
        maps[i].hostname = strdup(json_string_value(hostname));
        if (maps[i].hostname == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            free(maps[i].data);
            free(maps);
            return NULL;
        }
    }
    
    return maps;
}


power_data* parse_power_data(json_t* data){


}
