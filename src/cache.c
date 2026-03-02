#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cache.h"
#include "dll.h"
#include "hash.h"

// Creates a new LRU cache with the specified size limit
cache_t *cache_create(size_t max_size_mb, int num_workers)
{
    cache_t *cache = (cache_t *)malloc(sizeof(cache_t));
    if (!cache)
        return NULL;

    cache->max_size = (size_t)num_workers * max_size_mb * 1048576;
    cache->current_size = 0;
    cache->head = NULL;
    cache->tail = NULL;

    cache->hash_table = ht_create(512);
    if (!cache->hash_table)
    {
        free(cache);
        return NULL;
    }

    return cache;
}

// Retrieves a file from cache and moves it to the front (most recently used)
void *cache_get(cache_t *cache, char *key, size_t *out_size)
{
    if (!cache || !key)
        return NULL;

    Node *node = (Node *)ht_get(cache->hash_table, key);
    if (!node)
    {
        return NULL;
    }

    if (node != cache->head)
    {
        if (node == cache->tail)
        {
            cache->tail = node->left;
            if (cache->tail)
                cache->tail->right = NULL;
        }
        move_to_front(&cache->head, node);
    }

    if (out_size)
    {
        *out_size = node->size;
    }
    return node->data;
}

// Adds a new file to cache, evicting old ones if necessary to make space
int cache_put(cache_t *cache, char *key, void *data, size_t size)
{
    if (!cache || !key || !data)
        return -1;

    Node *existing = (Node *)ht_get(cache->hash_table, key);
    if (existing)
    {
        cache->current_size -= existing->size;
        cache->current_size += size;

        existing->data = data;
        existing->size = size;

        move_to_front(&cache->head, existing);
        return 0;
    }

    while (cache->current_size + size > cache->max_size && cache->tail)
    {
        cache_evict_lru(cache);
    }

    if (size > cache->max_size)
    {
        return -1;
    }

    Node *new_node = create_node((char *)key, data, size);
    if (!new_node)
        return -1;

    if (ht_put(cache->hash_table, key, new_node) != 0)
    {
        free(new_node->key);
        free(new_node);
        return -1;
    }

    if (cache->head == NULL)
    {
        cache->head = new_node;
        cache->tail = new_node;
    }
    else
    {
        new_node->right = cache->head;
        cache->head->left = new_node;
        cache->head = new_node;
    }

    cache->current_size += size;
    return 0;
}

// Removes the least recently used item from cache to free up space
void cache_evict_lru(cache_t *cache)
{
    if (!cache || !cache->tail)
        return;

    Node *lru = cache->tail;

    ht_remove(cache->hash_table, lru->key);

    cache->tail = lru->left;
    if (cache->tail)
    {
        cache->tail->right = NULL;
    }
    else
    {
        cache->head = NULL;
    }

    cache->current_size -= lru->size;

    if (lru->data)
        free(lru->data);

    free(lru->key);
    free(lru);
}

// Frees all memory used by the cache and its contents
void cache_destroy(cache_t *cache)
{
    if (!cache)
        return;

    Node *current = cache->head;
    while (current)
    {
        Node *next = current->right;
        if (current->data)
        {
            free(current->data);
        }
        free(current->key);
        free(current);
        current = next;
    }

    ht_destroy(cache->hash_table, 0);
    free(cache);
}