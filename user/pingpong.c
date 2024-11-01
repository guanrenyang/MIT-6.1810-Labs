#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[]){
    int fds_ping[2], fds_pong[2], pid; // first file is read, second is write

    if(pipe(fds_ping) != 0 || pipe(fds_pong) != 0){
        fprintf(2, "pipe() failed\n");
        exit(1);
    }

    pid = fork();
    if(pid == 0){ // child
        // child reads from fds_ping[0]
        close(fds_ping[1]);
        // child writes to fds_pong[1]
        close(fds_pong[0]);

        char buf[1];
        if(read(fds_ping[0], buf, 1) != 1){
            fprintf(2, "read() failed\n");
            exit(1);
        }
        printf("%d: received ping\n", getpid());

        if(write(fds_pong[1], buf, 1) != 1){
            fprintf(2, "write() failed\n");
            exit(1);
        }
    } else if(pid > 0){ // parent
        // parent writes to fds_ping[1]
        close(fds_ping[0]);
        // parent reads from fds_pong[1]
        close(fds_pong[1]);

        if(write(fds_ping[1], "x", 1) != 1){
            fprintf(2, "write() failed\n");
            exit(1);
        }

        char buf[1];
        if(read(fds_pong[0], buf, 1) != 1){
            fprintf(2, "read() failed\n");
            exit(1);
        }
        printf("%d: received pong\n", getpid());
    }
    exit(0);
}
