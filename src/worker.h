#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>
#include <openssl/ssl.h>
#include "shared_mem.h"
#include "semaphores.h"
#include "http.h"
#include "config.h"

typedef struct
{
    shared_data_t *data;
    semaphores_t *semaphores;
    int worker_id;
    int channel_fd;
    server_config_t configs;
    SSL_CTX *ssl_ctx;
} worker_args;

void start_worker(worker_args worker_arg);

#endif