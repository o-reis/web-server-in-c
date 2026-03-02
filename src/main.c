#include <stdlib.h>
#include <stdio.h>
#include "master.h"
#include "config.h"
#include "shared_mem.h"
#include "semaphores.h"
#include <signal.h>

#define FILE_LOC "server.conf" // May need changes depending on pc!

// Entry point that initializes server configuration and starts the master process
int main(void)
{
    server_config_t *conf = malloc(sizeof(server_config_t));

    if (load_config(FILE_LOC, conf) < 0)
    {
        fprintf(stderr, "Error in the file reading!\n");
        exit(1);
    }

    shared_data_t *shared_mem = create_shared_memory();
    if (shared_mem == NULL)
    {
        fprintf(stderr, "Error in the shared memory allocation!");
        exit(1);
    }

    semaphores_t *semaphores = malloc(sizeof(semaphores_t));
    if (semaphores == NULL)
    {
        fprintf(stderr, "Error in semaphores allocation!\n");
        exit(1);
    }
    if (init_semaphores(semaphores, conf->max_queue_size) < 0)
    {
        fprintf(stderr, "Error initializing semaphores!\n");
        exit(1);
    }

    shared_mem->log_file = fopen(conf->log_file, "a");
    if (shared_mem->log_file == NULL)
    {
        fprintf(stderr, "Error opening log file!\n");
        exit(1);
    }

    shared_mem->cache = cache_create(conf->cache_size_mb, conf->num_workers);

    printf("Server initialized and working!\n");

    run_master(conf, shared_mem, semaphores);

    free(conf);
    destroy_semaphores(semaphores);
    free(semaphores);

    fclose(shared_mem->log_file);

    cache_destroy(shared_mem->cache);

    destroy_shared_memory(shared_mem);

    return 0;
}