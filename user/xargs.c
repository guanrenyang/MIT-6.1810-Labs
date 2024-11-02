#include "kernel/types.h"
#include "user/user.h"


int readline(char *buf, int n) {
    int i = 0;
    char c;
    while(i < n-1) {
        if(read(0, &c, 1) != 1)
            break;
        if(c == '\n')
            break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

int xargs(char *path, int argc, char *argv[]){
    int n;
    char buf[512];
    if(!argv[argc]){
        argv[argc] = buf;
        argv[argc+1] = 0; // argv[argc] is 0(NULL), while argv[argc+1] is undefined
    }
    while((n=readline(buf, sizeof(buf)))>0){
        int pid = fork();
        if(pid==0){
            exec(path, argv);
            break;
        } else {
            wait(0);
        }
    }
    return 0;
}
int main(int argc, char *argv[]){
    if(argc < 2){
        fprintf(2, "Usage: xargs command [args...]\n");
        exit(1);
    }

    char *cmd = argv[1];
    xargs(cmd, argc-1, &argv[1]);

    return 0;
}
