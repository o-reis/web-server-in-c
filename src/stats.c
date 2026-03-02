#include "stats.h"
#include <string.h>
#include "semaphores.h"
#include "thread_pool.h"

// Displays formatted server statistics to the terminal with colors
void send_terminal_statics(server_stats_t *stats)
{
    printf("\033[1;31mActive Connections:\033[0m %d (Peak: %d)\n", stats->active_connections, stats->peak_active_connections);
    printf("\033[1;36mRequests/Second:\033[0m %ld\n", stats->requests_per_second);
    printf("====================================\n");
    printf("\033[1;32mStatus 200\033[0m -> %lu\n", stats->status_200);
    printf("\033[1;31mStatus 404\033[0m -> %lu\n", stats->status_404);
    printf("\033[1;31mStatus 500\033[0m -> %lu\n", stats->status_500);
    printf("\033[1;31mStatus 503\033[0m -> %lu\n", stats->status_503);
    printf("====================================\n");
    printf("\033[1;33mBytes transferred\033[0m -> %lu\n", stats->bytes_transferred);
    printf("\033[1;33mTotal requests\033[0m -> %lu\n", stats->total_requests);
}