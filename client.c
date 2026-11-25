#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <stdint.h>

#define SHM_SIZE 8192

typedef struct {
    uint32_t length;
    char data[SHM_SIZE - 4];
} shared_data_t;

int main(int argc, char **argv) {
    if (argc != 5) {
        write(STDERR_FILENO, "Usage: client <shm> <sem_empty> <sem_full> <filename>\n", 53);
        exit(EXIT_FAILURE);
    }

    const char *shm_name = argv[1];
    const char *sem_empty_name = argv[2];
    const char *sem_full_name = argv[3];
    const char *filename = argv[4];

    int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (file == -1) {
        write(STDERR_FILENO, "error: open file failed\n", 24);
        exit(EXIT_FAILURE);
    }

    int shm_fd = shm_open(shm_name, O_RDWR, 0600);
    if (shm_fd == -1) {
        write(STDERR_FILENO, "error: shm_open failed\n", 23);
        exit(EXIT_FAILURE);
    }

    shared_data_t *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        write(STDERR_FILENO, "error: mmap failed\n", 19);
        exit(EXIT_FAILURE);
    }

    sem_t *sem_empty = sem_open(sem_empty_name, 0);
    sem_t *sem_full = sem_open(sem_full_name, 0);
    if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED) {
        write(STDERR_FILENO, "error: sem_open failed\n", 23);
        exit(EXIT_FAILURE);
    }

    while (1) {
        sem_wait(sem_full);

        if (shm_ptr->length == 0) {
            sem_post(sem_empty);
            break;
        }

        int is_valid = 0;
        if (shm_ptr->length >= 2) {
            char last_char = shm_ptr->data[shm_ptr->length - 2];
            if (last_char == '.' || last_char == ';') {
                is_valid = 1;
            }
        }

        if (is_valid) {
            write(file, shm_ptr->data, shm_ptr->length);
            write(STDERR_FILENO, "OK\n", 3);
        } else {
            write(STDERR_FILENO, "Error: string must end with '.' or ';'\n", 39);
        }

        sem_post(sem_empty);
    }

    close(file);
    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
    sem_close(sem_empty);
    sem_close(sem_full);

    return 0;
}