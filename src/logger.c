#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include "logger.h"

#define LOG_CHECK_INTERVAL 100

static int counter = 0;

// Generates a logrotate configuration file for automatic log rotation
static void generate_logrotate_conf(const char *log_path)
{
    FILE *conf = fopen("logrotate.conf", "w");
    if (!conf)
    {
        fprintf(stderr, "Failed to create logrotate!");
        return;
    }

    char full_path[1024];
    if (log_path[0] == '/')
    {
        snprintf(full_path, sizeof(full_path), "%s", log_path);
    }
    else
    {
        char cwd[512];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            size_t cwd_len = strnlen(cwd, sizeof(cwd));
            size_t log_len = strnlen(log_path, sizeof(full_path) - cwd_len - 2);
            if (cwd_len + 1 + log_len + 1 <= sizeof(full_path))
            {
                snprintf(full_path, sizeof(full_path), "%s/%s", cwd, log_path);
            }
            else
            {
                snprintf(full_path, sizeof(full_path), "%s", log_path);
            }
        }
        else
        {
            snprintf(full_path, sizeof(full_path), "%s", log_path);
        }
    }

    fprintf(conf,
            "%s {\n"
            "   size 10M\n"
            "   rotate 5\n"
            "   compress\n"
            "   delaycompress\n"
            "   missingok\n"
            "   notifempty\n"
            "   create 0644\n"
            "   olddir old_logs\n"
            "}\n",
            full_path);

    fclose(conf);
}

// Checks if log file needs rotation and performs it if necessary
static FILE *check_logrotate(FILE *fp, const char *log_path)
{
    if (fp == NULL)
    {
        fprintf(stderr, "Error: log file pointer is NULL!\n");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);

    if (size <= 10000000)
        return fp;

    if (system("command -v logrotate > /dev/null 2>&1") != 0)
    {
        fprintf(stderr, "Warning: logrotate not found! Please install it for automatic log rotation.\n");
        return fp;
    }

    generate_logrotate_conf(log_path);

    fclose(fp);

    int ret = system("logrotate -f -s ./logrotate.status logrotate.conf 2>&1");
    if (ret != 0)
    {
        fprintf(stderr, "Error: logrotate failed with exit code %d\n", ret);
    }
    else
    {
        printf("Log rotation completed successfully!\n");
    }

    FILE *new_fp = fopen(log_path, "a");
    if (new_fp == NULL)
    {
        fprintf(stderr, "Fatal: Failed to reopen log file at %s!\n", log_path);
    }

    return new_fp;
}

// Formats the current date and time for log entries
static void get_date(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm tm_info;

    localtime_r(&now, &tm_info);

    strftime(buffer, size, "%d/%b/%Y:%H:%M:%S %z", &tm_info);
}

// Logs an HTTP request to the log file in Apache combined format
void exec_log(FILE **log_file, const char *log_path, char *address, char *method,
              char *route, int status_code, int bytes_sent, semaphores_t *sem)
{
    if (!*log_file)
        return;

    sem_wait(sem->log_mutex);

    if (++counter % LOG_CHECK_INTERVAL == 0)
    {
        *log_file = check_logrotate(*log_file, log_path);
    }

    char buffer[64];
    get_date(buffer, sizeof(buffer));

    fprintf(*log_file, "%s - - [%s] \"%s %s HTTP/1.1\" %d %d\n",
            address, buffer, method, route, status_code, bytes_sent);

    fflush(*log_file);

    sem_post(sem->log_mutex);
}

// Logs server start or shutdown events to the log file
void master_log(FILE **log_file, const char *log_path, semaphores_t *sem, int action)
{
    if (!*log_file)
        return;

    sem_wait(sem->log_mutex);

    if (action == 0)
    {
        *log_file = check_logrotate(*log_file, log_path);
    }

    char buffer[64];
    get_date(buffer, sizeof(buffer));

    if (action == 0)
    {
        fprintf(*log_file, "[%s] Server starting!\n", buffer);
    }
    else if (action == 1)
    {
        fprintf(*log_file, "[%s] Server closing!\n", buffer);
    }

    fflush(*log_file);
    sem_post(sem->log_mutex);
}