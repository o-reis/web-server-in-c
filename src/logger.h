#include "semaphores.h"
#ifndef LOGGER_H
#define LOGGER_H

void exec_log(FILE** log_file, const char* log_path, char* address, char* method, char* route, int status_code, int bytes_sent, semaphores_t* sem);
void master_log(FILE** log_file, const char* log_path, semaphores_t* sem, int action);

#endif