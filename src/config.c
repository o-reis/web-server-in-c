#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// Reads server configuration from a file and populates the config struct
int load_config(const char *filename, server_config_t *config)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
        return -1;
    char line[512], key[128], value[256];
    while (fgets(line, sizeof(line), fp))
    {
        if (line[0] == '#' || line[0] == '\n')
            continue;
        if (sscanf(line, "%[^=]=%s", key, value) == 2)
        {
            if (strcmp(key, "PORT") == 0)
                config->port = atoi(value);

            else if (strcmp(key, "NUM_WORKERS") == 0)
                config->num_workers = atoi(value);

            else if (strcmp(key, "MIN_THREADS") == 0)
                config->min_threads = atoi(value);
            else if (strcmp(key, "MAX_THREADS") == 0)
                config->max_threads = atoi(value);

            else if (strcmp(key, "THREADS_PER_WORKER") == 0)
            {
                config->threads_per_worker = atoi(value);
                if (config->min_threads == 0)
                    config->min_threads = atoi(value);
                if (config->max_threads == 0)
                    config->max_threads = atoi(value);
            }

            else if (strcmp(key, "DOCUMENT_ROOT") == 0)
                strncpy(config->document_root, value, sizeof(config->document_root));
            else if (strcmp(key, "TIMEOUT_SECONDS") == 0)
                config->timeout_seconds = atoi(value);
            else if (strcmp(key, "MAX_QUEUE_SIZE") == 0)
                config->max_queue_size = atoi(value);
            else if (strcmp(key, "CACHE_SIZE_MB") == 0)
                config->cache_size_mb = atoi(value);
            else if (strcmp(key, "LOG_FILE") == 0)
                strncpy(config->log_file, value, sizeof(config->log_file));
        }
    }
    fclose(fp);
    return 0;
}