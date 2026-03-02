#include "shared_mem.h" // for shared_data_t struct
#include "semaphores.h" // for semaphores_t struct
#include "config.h"
#include "worker.h"

void run_master(server_config_t* conf, shared_data_t* data, semaphores_t* semaphores);