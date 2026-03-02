#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <pthread.h>
#include <stdio.h>
#include <stdatomic.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "shared_mem.h"
#include "semaphores.h"
#include "config.h"

typedef struct
{
    int *fd_s;
    int max_size;
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} queue_t;

typedef struct thread_pool
{
    pthread_t *threads;
    int num_threads;
    int min_threads;
    int max_threads;
    volatile _Atomic int shutdown;
    queue_t queue;
    shared_data_t *data;
    semaphores_t *semaphore;
    int worker_id;
    int channel_fd;
    server_config_t configs;
    SSL_CTX *ssl_ctx;
} thread_pool_t;

thread_pool_t *create_thread_pool(shared_data_t *data, semaphores_t *semaphore, int worker_id, int channel_fd, server_config_t configs, SSL_CTX *ssl_ctx);
void destroy_thread_pool(thread_pool_t *pool);

#endif