#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <stdatomic.h>
#include "master.h"
#include "uds.h"
#include "logger.h"
#include "stats.h"
#include "cache.h"

volatile _Atomic int keep_running = 1;

typedef struct
{
    shared_data_t *data;
    semaphores_t *semaphores;
    int delay;
} stats_thread_args;

// Thread that periodically displays server statistics to the console
void *stats_display_thread(void *arg)
{
    stats_thread_args *args = (stats_thread_args *)arg;

    while (keep_running)
    {
        sleep(30);
        if (!keep_running)
            break;

        printf("\n");
        printf("=====================================\n");
        printf("    SERVER STATISTICS\n");
        printf("=====================================\n");

        sem_wait(args->semaphores->stats_mutex);
        send_terminal_statics(&args->data->stats);
        sem_post(args->semaphores->stats_mutex);

        printf("=====================================\n\n");
    }

    return NULL;
}

// Handles SIGINT (Ctrl+C) to gracefully shut down the server
void signal_handler(int signum)
{
    (void)signum;

    // Only handle once
    if (keep_running == 0)
    {
        return;
    }

    keep_running = 0;
    printf("\nThe server is closing...\n");
}

// Creates and binds a TCP socket to listen for incoming connections
int create_server_socket(int port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return -1;
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(sockfd);
        return -1;
    }
    if (listen(sockfd, 128) < 0)
    {
        close(sockfd);
        return -1;
    }
    return sockfd;
}

// Main master process that accepts connections and distributes them to workers
void run_master(server_config_t *conf, shared_data_t *data, semaphores_t *semaphores)
{
    struct sigaction sigact;
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = signal_handler;
    sigact.sa_flags = 0;
    sigemptyset(&sigact.sa_mask);

    if (sigaction(SIGINT, &sigact, NULL) < 0)
    {
        perror("Error registering SIGINT");
        exit(1);
    }

    signal(SIGPIPE, SIG_IGN);
    int server_sock = create_server_socket(conf->port);
    int n_workers = conf->num_workers;
    pid_t worker_pids[n_workers];
    int worker_sockets[n_workers];

    master_log(&data->log_file, conf->log_file, semaphores, 0);

    pthread_t stats_thread;
    stats_thread_args stats_args = {
        .data = data,
        .semaphores = semaphores};
    stats_args.delay = conf->timeout_seconds;

    pthread_create(&stats_thread, NULL, stats_display_thread, &stats_args);

    for (int i = 0; i < n_workers; i++)
    {
        int sv[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
        {
            perror("Socketpair failed");
            exit(1);
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("Fork failed!");
            exit(1);
        }

        if (pid == 0)
        {
            close(server_sock);
            close(sv[0]);

            SSL_library_init();
            OpenSSL_add_all_algorithms();
            SSL_load_error_strings();

            SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_server_method());
            if (!ssl_ctx)
            {
                fprintf(stderr, "Worker %d: Failed to create SSL context\n", i);
                ERR_print_errors_fp(stderr);
                exit(1);
            }

            if (SSL_CTX_use_certificate_file(ssl_ctx, "server.crt", SSL_FILETYPE_PEM) <= 0)
            {
                fprintf(stderr, "Worker %d: Failed to load certificate\n", i);
                ERR_print_errors_fp(stderr);
                exit(1);
            }

            if (SSL_CTX_use_PrivateKey_file(ssl_ctx, "server.key", SSL_FILETYPE_PEM) <= 0)
            {
                fprintf(stderr, "Worker %d: Failed to load private key\n", i);
                ERR_print_errors_fp(stderr);
                exit(1);
            }

            worker_args worker_arg;
            worker_arg.data = data;
            worker_arg.semaphores = semaphores;
            worker_arg.configs = *conf;
            worker_arg.worker_id = i;
            worker_arg.channel_fd = sv[1];
            worker_arg.ssl_ctx = ssl_ctx;

            start_worker(worker_arg);

            SSL_CTX_free(ssl_ctx);
            close(sv[1]);

            if (data->cache)
            {
                cache_destroy(data->cache);
            }

            if (data->log_file)
            {
                fclose(data->log_file);
            }

            free(conf);

            if (semaphores)
            {
                sem_close(semaphores->empty_slots);
                sem_close(semaphores->filled_slots);
                sem_close(semaphores->queue_mutex);
                sem_close(semaphores->stats_mutex);
                sem_close(semaphores->log_mutex);
                sem_close(semaphores->cache_mutex);
                free(semaphores);
            }

            EVP_cleanup();
            CRYPTO_cleanup_all_ex_data();
            ERR_free_strings();

            exit(0);
        }

        worker_pids[i] = pid;
        close(sv[1]);
        worker_sockets[i] = sv[0];
    }

    int rr_worker = 0;

    while (keep_running)
    {

        struct sockaddr_in client_addr;
        socklen_t cli_len = sizeof(client_addr);

        int client_fd = accept(server_sock, (struct sockaddr *)&client_addr, &cli_len);

        int saved_errno = errno;

        if (!keep_running)
        {
            if (client_fd >= 0)
                close(client_fd);
            break;
        }

        if (client_fd < 0)
        {
            if (saved_errno == EINTR)
            {
                continue;
            }

            if (saved_errno == EBADF)
            {
                break;
            }

            errno = saved_errno;
            perror("Accept error");
            continue;
        }

        if (send_fd(worker_sockets[rr_worker], client_fd) < 0)
        {
            perror("Error in passing the FD to the worker!");
        }

        close(client_fd);

        rr_worker = (rr_worker + 1) % n_workers; // Similar to circular arrays.
    }

    close(server_sock);

    pthread_cancel(stats_thread);
    pthread_join(stats_thread, NULL);

    for (int i = 0; i < n_workers; i++)
    {
        kill(worker_pids[i], SIGTERM);
    }

    for (int i = 0; i < n_workers; i++)
    {
        close(worker_sockets[i]);
    }

    for (int i = 0; i < n_workers; i++)
    {
        waitpid(worker_pids[i], NULL, 0);
    }

    printf("Server successfully closed!");
    master_log(&data->log_file, conf->log_file, semaphores, 1);
}