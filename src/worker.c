#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <signal.h>
#include "worker.h"
#include "thread_pool.h"

static thread_pool_t *global_thread_pool = NULL;

// Handles SIGTERM to gracefully shut down worker threads
void worker_signal_handler(int signum)
{
    (void)signum;

    if (global_thread_pool)
    {
        global_thread_pool->shutdown = 1;
    }
}

// Starts a worker process with its thread pool to handle client requests
void start_worker(worker_args worker_arg)
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sa.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = worker_signal_handler;
    sigaction(SIGTERM, &sa, NULL);

    thread_pool_t *thread_pool = create_thread_pool(worker_arg.data, worker_arg.semaphores, worker_arg.worker_id, worker_arg.channel_fd, worker_arg.configs, worker_arg.ssl_ctx);

    global_thread_pool = thread_pool;

    for (int i = 0; i <= thread_pool->num_threads; i++)
    {
        pthread_join(thread_pool->threads[i], NULL);
    }

    destroy_thread_pool(thread_pool);
}