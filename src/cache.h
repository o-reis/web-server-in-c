#ifndef CACHE_H
#define CACHE_H

#include "dll.h"
#include "hash.h"

typedef struct cache {
    hashtable_t* hash_table;
    Node* head;                
    Node* tail;                
    size_t current_size; 
    size_t max_size;    
} cache_t;


cache_t* cache_create(size_t max_size_mb, int num_workers);
void* cache_get(cache_t* cache, char* key, size_t* out_size);
int cache_put(cache_t* cache, char* key, void* data, size_t size);
void cache_destroy(cache_t* cache);
void cache_evict_lru(cache_t* cache);

#endif