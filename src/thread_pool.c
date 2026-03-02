#include "thread_pool.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <poll.h>
#include "uds.h"
#include "http.h"
#include "logger.h"
#include "cache.h"

void *worker_thread(void *arg);

// Thread that receives client connections and queues them for worker threads
void *dispatcher_thread(void *arg)
{
    thread_pool_t *pool = (thread_pool_t *)arg;
    int scaling_check_counter = 0;

    while (1)
    {
        pthread_mutex_lock(&pool->queue.mutex);
        int should_shutdown = pool->shutdown;
        pthread_mutex_unlock(&pool->queue.mutex);

        if (should_shutdown)
        {
            break;
        }

        int client_fd = recv_fd(pool->channel_fd);
        if (client_fd < 0)
        {
            pthread_mutex_lock(&pool->queue.mutex);
            pool->shutdown = 1;
            pthread_cond_broadcast(&pool->queue.cond);
            pthread_mutex_unlock(&pool->queue.mutex);
            break;
        }

        pthread_mutex_lock(&pool->queue.mutex);

        if (pool->queue.count >= pool->queue.max_size)
        {
            pthread_mutex_unlock(&pool->queue.mutex);

            SSL *ssl = SSL_new(pool->ssl_ctx);
            if (ssl)
            {
                SSL_set_fd(ssl, client_fd);
                int ssl_accept_result = SSL_accept(ssl);
                if (ssl_accept_result > 0)
                {
                    char pathBuffer[256];
                    pathBuffer[0] = '\0';
                    strcat(pathBuffer, pool->configs.document_root);
                    strcat(pathBuffer, "/errors/serviceunavailable.html");

                    size_t file_size = 0;
                    char *body = read_http(pathBuffer, &file_size);
                    if (body)
                    {
                        send_http_response(ssl, 503, "Service Unavailable", "text/html", body, file_size, 0);
                        free(body);
                    }
                }
                SSL_shutdown(ssl);
                SSL_free(ssl);
            }

            sem_wait(pool->semaphore->stats_mutex);
            pool->data->stats.status_503++;
            sem_post(pool->semaphore->stats_mutex);

            close(client_fd);

            pthread_mutex_lock(&pool->queue.mutex);
            if (pool->num_threads < pool->max_threads)
            {
                int thread_idx = pool->num_threads + 1;
                pthread_create(&pool->threads[thread_idx], NULL, worker_thread, pool);
                pool->num_threads++;
            }
            pthread_mutex_unlock(&pool->queue.mutex);
            continue;
        }

        pool->queue.fd_s[pool->queue.rear] = client_fd;
        pool->queue.rear = (pool->queue.rear + 1) % pool->queue.max_size;
        pool->queue.count++;

        scaling_check_counter++;
        if (scaling_check_counter >= 100)
        {
            scaling_check_counter = 0;
            int queue_load = (pool->queue.count * 100) / pool->queue.max_size;

            if (queue_load > 70 && pool->num_threads < pool->max_threads)
            {
                int thread_idx = pool->num_threads + 1;
                pthread_create(&pool->threads[thread_idx], NULL, worker_thread, pool);
                pool->num_threads++;
            }
            else if (queue_load < 20 && pool->num_threads > pool->min_threads)
            {
                if (pool->queue.count < pool->queue.max_size)
                {
                    pool->queue.fd_s[pool->queue.rear] = -1;
                    pool->queue.rear = (pool->queue.rear + 1) % pool->queue.max_size;
                    pool->queue.count++;

                    pool->num_threads--;

                    pthread_cond_signal(&pool->queue.cond);
                }
            }
        }

        pthread_cond_signal(&pool->queue.cond);

        pthread_mutex_unlock(&pool->queue.mutex);
    }

    return NULL;
}

// Worker thread that processes HTTP requests from the queue
void *worker_thread(void *arg)
{
    thread_pool_t *pool = (thread_pool_t *)arg;

    char buffer[4096];
    while (1)
    {
        int client_fd;

        pthread_mutex_lock(&pool->queue.mutex);

        while (pool->queue.count == 0 && !pool->shutdown)
        {
            pthread_cond_wait(&pool->queue.cond, &pool->queue.mutex);
        }

        if (pool->shutdown && pool->queue.count == 0)
        {
            pthread_mutex_unlock(&pool->queue.mutex);
            break;
        }

        client_fd = pool->queue.fd_s[pool->queue.front];
        pool->queue.front = (pool->queue.front + 1) % pool->queue.max_size;
        pool->queue.count--;

        pthread_mutex_unlock(&pool->queue.mutex);

        if (client_fd == -1)
        {
            pthread_detach(pthread_self());
            break;
        }

        struct pollfd pfd;
        pfd.fd = client_fd;
        pfd.events = POLLIN;

        int poll_ret = poll(&pfd, 1, 5000);
        if (poll_ret <= 0)
        {
            close(client_fd);
            continue;
        }

        struct timeval tv;
        tv.tv_sec = pool->configs.timeout_seconds;
        tv.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));

        SSL *ssl = SSL_new(pool->ssl_ctx);
        if (!ssl)
        {
            close(client_fd);
            continue;
        }

        SSL_set_fd(ssl, client_fd);

        int ssl_accept_result = SSL_accept(ssl);
        if (ssl_accept_result <= 0)
        {
            int ssl_error = SSL_get_error(ssl, ssl_accept_result);
            unsigned long err_code = ERR_peek_error();
            int reason = ERR_GET_REASON(err_code);

            if (!(ssl_error == SSL_ERROR_SSL && (reason == 156 || reason == 294)))
            {
                fprintf(stderr, "Worker %d: SSL_accept failed with error %d (reason: %d)\n",
                        pool->worker_id, ssl_error, reason);
                ERR_print_errors_fp(stderr);
            }
            ERR_clear_error();
            SSL_free(ssl);
            close(client_fd);
            continue;
        }

        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        char client_ip[INET_ADDRSTRLEN];

        if (getpeername(client_fd, (struct sockaddr *)&client_addr, &addr_len) == -1)
        {
            perror("getpeername failed");
            strncpy(client_ip, "Unknown", INET_ADDRSTRLEN);
        }
        else
        {
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        }

        int keep_alive = 1;
        while (keep_alive)
        {
            pthread_mutex_lock(&pool->queue.mutex);
            int should_shutdown = pool->shutdown;
            pthread_mutex_unlock(&pool->queue.mutex);

            if (should_shutdown)
                break;

            if (SSL_pending(ssl) == 0)
            {
                pfd.fd = client_fd;
                pfd.events = POLLIN;
                int ret = poll(&pfd, 1, 5000);
                if (ret == 0)
                {
                    break;
                }
                else if (ret < 0)
                {
                    break;
                }
            }

            ssize_t bytes_read = SSL_read(ssl, buffer, sizeof(buffer) - 1);
            if (bytes_read <= 0)
            {
                int err = SSL_get_error(ssl, bytes_read);

                if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                {
                    pthread_mutex_lock(&pool->queue.mutex);
                    should_shutdown = pool->shutdown;
                    pthread_mutex_unlock(&pool->queue.mutex);

                    if (should_shutdown)
                        break;

                    continue;
                }
                break;
            }
            buffer[bytes_read] = '\0';

            http_request_t request;
            if (parse_http_request(buffer, &request) < 0)
            {
                fprintf(stderr, "Worker %d (Thread %lu): Invalid request or bad format.\n",
                        pool->worker_id, pthread_self());
                break;
            }

            if (strcmp(request.version, "HTTP/1.0") == 0)
            {
                keep_alive = 0;
            }
            else
            {
                keep_alive = !request.connection_close;
            }

            if (strcmp(request.path, "/api/stats") == 0)
            {
                sem_wait(pool->semaphore->stats_mutex);

                char json_body[512];
                int json_len = snprintf(json_body, sizeof(json_body),
                                        "{\"active_connections\":%d,\"peak_active_connections\":%d,\"requests_per_second\":%ld,\"total_requests\":%ld,\"status_200\":%ld,\"status_403\":%ld,\"status_404\":%ld,\"status_500\":%ld,\"status_503\":%ld,\"bytes_transferred\":%ld}",
                                        pool->data->stats.active_connections,
                                        pool->data->stats.peak_active_connections,
                                        pool->data->stats.requests_per_second,
                                        pool->data->stats.total_requests,
                                        pool->data->stats.status_200,
                                        pool->data->stats.status_403,
                                        pool->data->stats.status_404,
                                        pool->data->stats.status_500,
                                        pool->data->stats.status_503,
                                        pool->data->stats.bytes_transferred);

                sem_post(pool->semaphore->stats_mutex);

                send_http_response(ssl, 200, "OK", "application/json", json_body, json_len, keep_alive);

                sem_wait(pool->semaphore->stats_mutex);
                pool->data->stats.bytes_transferred += json_len;
                sem_post(pool->semaphore->stats_mutex);

                if (!keep_alive)
                {
                    break;
                }
                continue;
            }

            sem_wait(pool->semaphore->stats_mutex);
            pool->data->stats.active_connections += 1;

            if (pool->data->stats.active_connections > pool->data->stats.peak_active_connections)
            {
                pool->data->stats.peak_active_connections = pool->data->stats.active_connections;
            }
            sem_post(pool->semaphore->stats_mutex);

            char pathBuffer[128];
            pathBuffer[0] = '\0';

            strcat(pathBuffer, pool->configs.document_root);
            strcat(pathBuffer, request.path);

            int status, bodylen;
            char *status_msg;
            char *content_type = "";
            char *body;

            struct stat path_stat;
            if (stat(pathBuffer, &path_stat) == 0 && S_ISDIR(path_stat.st_mode))
            {
                pathBuffer[0] = '\0';
                strcat(pathBuffer, pool->configs.document_root);
                strcat(pathBuffer, "/index.html");
            }

            if (access(pathBuffer, F_OK) != 0)
            {
                content_type = "text/html";

                pathBuffer[0] = '\0';
                strcat(pathBuffer, pool->configs.document_root);
                if (strcmp(request.path, "/admin") == 0)
                {
                    strcat(pathBuffer, "/errors/unauthorized.html");
                    status = 401;
                    status_msg = "Unauthorized";
                }
                else
                {
                    strcat(pathBuffer, "/errors/notfound.html");
                    status = 404;
                    status_msg = "Not Found";
                }

                if (access(pathBuffer, F_OK) != 0)
                {
                    status = 500;
                    status_msg = "Internal Server Error";

                    pathBuffer[0] = '\0';
                    strcat(pathBuffer, pool->configs.document_root);
                    strcat(pathBuffer, "/errors/internalservererror.html");
                }
            }
            else if (access(pathBuffer, R_OK) != 0)
            {
                content_type = "text/html";
                status = 403;
                status_msg = "Forbidden";

                pathBuffer[0] = '\0';
                strcat(pathBuffer, pool->configs.document_root);
                strcat(pathBuffer, "/errors/forbidden.html");

                if (access(pathBuffer, F_OK) != 0)
                {
                    status = 500;
                    status_msg = "Internal Server Error";

                    pathBuffer[0] = '\0';
                    strcat(pathBuffer, pool->configs.document_root);
                    strcat(pathBuffer, "/errors/internalservererror.html");
                }
            }
            else
            {
                if (strlen(pathBuffer) >= strlen(".html") && strcmp(pathBuffer + strlen(pathBuffer) - 5, ".html") == 0)
                {
                    content_type = "text/html";
                }
                else if (strlen(pathBuffer) >= strlen(".css") && strcmp(pathBuffer + strlen(pathBuffer) - 4, ".css") == 0)
                {
                    content_type = "text/css";
                }
                else if (strlen(pathBuffer) >= strlen(".js") && strcmp(pathBuffer + strlen(pathBuffer) - 3, ".js") == 0)
                {
                    content_type = "application/javascript";
                }
                else if (strlen(pathBuffer) >= strlen(".png") && strcmp(pathBuffer + strlen(pathBuffer) - 4, ".png") == 0)
                {
                    content_type = "image/png";
                }
                else if (strlen(pathBuffer) >= strlen(".jpeg") && strcmp(pathBuffer + strlen(pathBuffer) - 5, ".jpeg") == 0)
                {
                    content_type = "image/jpeg";
                }
                else if (strlen(pathBuffer) >= strlen(".pdf") && strcmp(pathBuffer + strlen(pathBuffer) - 4, ".pdf") == 0)
                {
                    content_type = "application/pdf";
                }
                status = 200;
                status_msg = "OK";
            }

            int cache_hit = 0;

            if (strcmp(request.method, "HEAD") == 0)
            {
                struct stat file_stat;
                if (stat(pathBuffer, &file_stat) == 0)
                {
                    bodylen = file_stat.st_size;
                }
                else
                {
                    bodylen = 0;
                }
                body = NULL;
            }
            else if (pool->data->cache)
            {
                size_t cached_size = 0;

                sem_wait(pool->semaphore->cache_mutex);
                char *internal_body = (char *)cache_get(pool->data->cache, pathBuffer, &cached_size);

                if (internal_body)
                {
                    body = malloc(cached_size);
                    if (body)
                    {
                        memcpy(body, internal_body, cached_size);
                        bodylen = cached_size > 0 ? cached_size - 1 : 0;
                        cache_hit = 1;
                    }
                    else
                    {
                        body = NULL;
                    }
                }
                else
                {
                    body = NULL;
                }
                sem_post(pool->semaphore->cache_mutex);

                if (!cache_hit && !body)
                {
                    size_t file_size = 0;
                    body = read_http(pathBuffer, &file_size);
                    if (body)
                    {
                        bodylen = file_size;

                        char *cached_body = malloc(file_size + 1);
                        if (cached_body)
                        {
                            memcpy(cached_body, body, file_size);
                            cached_body[file_size] = '\0';

                            sem_wait(pool->semaphore->cache_mutex);
                            cache_put(pool->data->cache, pathBuffer, cached_body, file_size + 1);
                            sem_post(pool->semaphore->cache_mutex);
                        }
                    }
                    else
                    {
                        bodylen = 0;
                    }
                }
            }
            else
            {
                size_t file_size = 0;
                body = read_http(pathBuffer, &file_size);
                if (body)
                {
                    bodylen = file_size;
                }
                else
                {
                    bodylen = 0;
                }
            }

            send_http_response(ssl, status, status_msg, content_type, body, bodylen, keep_alive);

            exec_log(&pool->data->log_file, pool->configs.log_file, client_ip, request.method, request.path, status, bodylen, pool->semaphore);

            sem_wait(pool->semaphore->stats_mutex);

            pool->data->stats.total_requests++;
            pool->data->stats.bytes_transferred += bodylen > 0 ? bodylen : 0;
            pool->data->stats.active_connections -= 1;

            time_t current_time = time(NULL);
            if (current_time != pool->data->stats.last_second)
            {
                pool->data->stats.requests_per_second = pool->data->stats.requests_this_second;
                pool->data->stats.requests_this_second = 1;
                pool->data->stats.last_second = current_time;
            }
            else
            {
                pool->data->stats.requests_this_second++;
            }

            if (status == 200)
            {
                pool->data->stats.status_200++;
            }
            else if (status == 403)
            {
                pool->data->stats.status_403++;
            }
            else if (status == 404)
            {
                pool->data->stats.status_404++;
            }
            else if (status == 500)
            {
                pool->data->stats.status_500++;
            }
            else if (status == 503)
            {
                pool->data->stats.status_503++;
            }

            sem_post(pool->semaphore->stats_mutex);

            if (body != NULL)
            {
                free(body);
            }

            if (!keep_alive)
            {
                break;
            }
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_fd);
    }

    return NULL;
}

// Creates a thread pool with dispatcher and worker threads for handling requests
thread_pool_t *create_thread_pool(shared_data_t *data, semaphores_t *semaphore, int worker_id, int channel_fd, server_config_t configs, SSL_CTX *ssl_ctx)
{
    thread_pool_t *pool = malloc(sizeof(thread_pool_t));

    int min = (configs.min_threads > 0) ? configs.min_threads : configs.threads_per_worker;
    int max = (configs.max_threads > 0) ? configs.max_threads : configs.threads_per_worker;

    pool->min_threads = min;
    pool->max_threads = max;
    pool->num_threads = min;
    pool->threads = malloc(sizeof(pthread_t) * (max + 1));
    pool->configs = configs;

    pool->data = data;
    pool->semaphore = semaphore;
    pool->worker_id = worker_id;
    pool->channel_fd = channel_fd;
    pool->ssl_ctx = ssl_ctx;

    pool->queue.fd_s = malloc(sizeof(int) * configs.max_queue_size);
    pool->queue.max_size = configs.max_queue_size;
    pool->queue.front = 0;
    pool->queue.rear = 0;
    pool->queue.count = 0;
    pthread_mutex_init(&pool->queue.mutex, NULL);
    pthread_cond_init(&pool->queue.cond, NULL);

    pthread_mutex_lock(&pool->queue.mutex);
    pool->shutdown = 0;
    pthread_mutex_unlock(&pool->queue.mutex);

    pthread_create(&pool->threads[0], NULL, dispatcher_thread, pool);

    for (int i = 1; i <= min; i++)
    {
        pthread_create(&pool->threads[i], NULL, worker_thread, pool);
    }

    return pool;
}

// Shuts down all threads in the pool and frees associated resources
void destroy_thread_pool(thread_pool_t *thread_pool)
{
    if (!thread_pool)
        return;

    pthread_mutex_lock(&thread_pool->queue.mutex);
    int already_shutdown = thread_pool->shutdown;
    int num_threads_snapshot = thread_pool->num_threads;
    if (!already_shutdown)
    {
        thread_pool->shutdown = 1;
    }
    pthread_mutex_unlock(&thread_pool->queue.mutex);

    if (!already_shutdown)
    {
        if (thread_pool->channel_fd >= 0)
        {
            close(thread_pool->channel_fd);
        }

        pthread_cond_broadcast(&thread_pool->queue.cond);

        for (int i = 0; i <= num_threads_snapshot; i++)
        {
            pthread_join(thread_pool->threads[i], NULL);
        }
    }

    pthread_mutex_destroy(&thread_pool->queue.mutex);
    pthread_cond_destroy(&thread_pool->queue.cond);
    free(thread_pool->queue.fd_s);
    free(thread_pool->threads);
    free(thread_pool);
}