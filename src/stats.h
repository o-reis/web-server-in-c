#ifndef STATS_H
#define STATS_H
#include <stdlib.h>
#include <stdio.h>

typedef struct thread_pool thread_pool_t;

typedef struct
{
    long total_requests;
    long bytes_transferred;
    long status_200;
    long status_403;
    long status_404;
    long status_500;
    long status_503;
    int active_connections;
    int peak_active_connections;
    long requests_per_second;
    long requests_this_second;
    time_t last_second;
} server_stats_t;

void send_terminal_statics(server_stats_t *stats);

#endif