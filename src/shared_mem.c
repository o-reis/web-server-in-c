#include "shared_mem.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#define SHM_NAME "/webserver_shm"
// Creates a shared memory region for inter-process communication
shared_data_t *create_shared_memory()
{
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1)
        return NULL;
    if (ftruncate(shm_fd, sizeof(shared_data_t)) == -1)
    {
        close(shm_fd);
        return NULL;
    }
    shared_data_t *data = mmap(NULL, sizeof(shared_data_t),
                               PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (data == MAP_FAILED)
        return NULL;
    memset(data, 0, sizeof(shared_data_t));
    data->stats.last_second = time(NULL);
    return data;
}
// Unmaps and unlinks the shared memory region
void destroy_shared_memory(shared_data_t *data)
{
    munmap(data, sizeof(shared_data_t));
    shm_unlink(SHM_NAME);
}