#ifndef SHARED_MEM_H
#define SHARED_MEM_H
#include <stdio.h>
#include "cache.h"
#include "stats.h"

typedef struct {
    server_stats_t stats;
    FILE* log_file;
    cache_t* cache;

} shared_data_t;

shared_data_t* create_shared_memory();
void destroy_shared_memory(shared_data_t* data);
#endif