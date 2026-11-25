#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <stdint.h>

#define SHM_SIZE 8192

typedef struct {
    uint32_t length;
    char data[SHM_SIZE - 4];
} shared_data_t;

void my_itoa(int num, char *str) {
    int i = 0;
    if (num == 0) { str[i++] = '0'; str[i] = '\0'; return; }
    while (num != 0) {
        str[i++] = (num % 10) + '0';
        num /= 10;
    }
    str[i] = '\0';
    for (int start = 0, end = i - 1; start < end; start++, end--) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        write(STDERR_FILENO, "usage: server <filename>\n", 25);
        exit(EXIT_FAILURE);
    }

    char shm_name[256] = "/lab3_shm_";
    char sem_empty_name[256] = "/lab3_sem_empty_";
    char sem_full_name[256] = "/lab3_sem_full_";
    char pid_str[16];
    
    my_itoa(getpid(), pid_str);
    strcat(shm_name, pid_str);
    strcat(sem_empty_name, pid_str);
    strcat(sem_full_name, pid_str);

    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0600);
    if (shm_fd == -1) {
        write(STDERR_FILENO, "error: shm_open failed\n", 23);
        exit(EXIT_FAILURE);
    }
    
    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        write(STDERR_FILENO, "error: ftruncate failed\n", 24);
        exit(EXIT_FAILURE);
    }

    shared_data_t *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        write(STDERR_FILENO, "error: mmap failed\n", 19);
        exit(EXIT_FAILURE);
    }

    sem_t *sem_empty = sem_open(sem_empty_name, O_CREAT, 0600, 1);
    sem_t *sem_full = sem_open(sem_full_name, O_CREAT, 0600, 0);
    if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED) {
        write(STDERR_FILENO, "error: sem_open failed\n", 23);
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == 0) {
        execl("./client", "client", shm_name, sem_empty_name, sem_full_name, argv[1], NULL);
        write(STDERR_FILENO, "error: execl failed\n", 21);
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        write(STDERR_FILENO, "error: fork failed\n", 19);
        exit(EXIT_FAILURE);
    }

    char msg[128];
    int len = 0;
    
    strcpy(msg, "Parent (Server) PID: ");
    len = strlen(msg);
    my_itoa(getpid(), msg + len);
    len = strlen(msg);
    strcat(msg + len, ", Child (Client) PID: ");
    len = strlen(msg);
    my_itoa(pid, msg + len);
    strcat(msg, "\n");
    write(STDOUT_FILENO, msg, strlen(msg));
    
    write(STDOUT_FILENO, "Enter text (press Enter on empty line to exit):\n", 48);

    char buf[SHM_SIZE];
    while (1) {
        ssize_t bytes_read = read(STDIN_FILENO, buf, sizeof(buf) - 1);
        if (bytes_read <= 0) break;
        if (bytes_read == 1 && buf[0] == '\n') break;

        sem_wait(sem_empty);
        shm_ptr->length = bytes_read;
        memcpy(shm_ptr->data, buf, bytes_read);
        sem_post(sem_full);
    }

    sem_wait(sem_empty);
    shm_ptr->length = 0; 
    sem_post(sem_full);

    waitpid(pid, NULL, 0);

    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
    shm_unlink(shm_name);

    sem_close(sem_empty);
    sem_close(sem_full);
    sem_unlink(sem_empty_name);
    sem_unlink(sem_full_name);

    return 0;
}