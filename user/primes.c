#include "kernel/types.h"
#include "user/user.h"

void primes(int p, int fd_in) {
    printf("prime %d\n", p);

    int fds[2]; // first is write, second is read
    int ret = pipe(fds);
    if(ret < 0){
        fprintf(2, "pipe() failed: %d\n", ret);
        exit(1);
    }

    int pid = fork();
    if(pid < 0){
        fprintf(2, "fork() failed\n");
        exit(1);
    }

    if (pid == 0){
        close(fds[1]); // child reads from fds[1]
        close(fd_in);
        int in;
        if(read(fds[0], &in, sizeof(int)) > 0){
            primes(in, fds[0]);
        }
    } else {
        close(fds[0]); // parent writes to fds[1]
        int in;
        while(read(fd_in, &in, sizeof(int)) > 0){
            if(in % p != 0){
                write(fds[1], &in, sizeof(int));
            }
        }
        close(fd_in);
        close(fds[1]);
        wait(0);
    }
}
int main(int argc, char *argv[]){
    int leastPrime = 2;
    int searchBound = 280;

    printf("prime %d\n", leastPrime);

    int fds[2], pid;
    if(pipe(fds) != 0){
        fprintf(2, "pipe() failed\n");
        exit(1);
    }

    pid = fork();
    if(pid < 0){
        fprintf(2, "fork() failed\n");
        exit(1);
    }

    // concurrent execution
    if (pid == 0){
        close(fds[1]);
        int in;
        if(read(fds[0], &in, sizeof(int)) > 0){
            primes(in, fds[0]);
        }
    } else if (pid > 0){
        close(fds[0]);
        for (int i = leastPrime; i <= searchBound; i++){
            if(i%leastPrime != 0){
                write(fds[1], &i, sizeof(int));
            }
        }
        close(fds[1]);
        wait(0);
    }

    exit(0);
}