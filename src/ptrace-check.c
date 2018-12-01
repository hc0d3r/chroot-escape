#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include <sys/ptrace.h>
#include <sys/wait.h>

#include "common.h"

int yama_check(const char *path){
    char *buf = NULL;
    int ret = 0;
    size_t len;
    FILE *fh;

    len = strlen(path)+30;
    if((buf = malloc(len)) == NULL)
        goto end;

    sprintf(buf, "%s/sys/kernel/yama/ptrace_scope", path);

    fh = fopen(buf, "r");
    if(fh == NULL){
        bad("can't open %s: %s\n", buf, strerror(errno));
        ret = -1;
        goto end;
    }

    if(fgetc(fh) == '0')
        ret = 1;

    fclose(fh);

    end:
    free(buf);
    return ret;
}

int attach_this(int fd){
    pid_t child;

    child = fork();
    if(child == -1)
        err(1, "fork");

    if(child)
        _exit(0);

    sleep(1);

    child = getpid();
    write(fd, &child, sizeof(pid_t));
    close(fd);

    kill(child, SIGSTOP);
    exit(0);
}


int ptrace_allowed(const char *procdir){
    int fd[2], nbytes, ret = 0, status;
    pid_t child;

    status = yama_check(procdir);
    if(status >= 0)
        return status;

    if(pipe(fd))
        err(1, "pipe");

    child = fork();
    if(child == -1)
        err(1, "fork");

    if(child == 0){
        close(fd[0]);
        attach_this(fd[1]);
    }

    nbytes = read(fd[0], &child, sizeof(pid_t));
    if(nbytes != sizeof(pid_t))
        errx(1, "error...\n");

    if(ptrace(PTRACE_ATTACH, child, 0, 0) == -1)
        goto end;

    ret = 1;

    waitpid(child, &status, 0);
    while(!WIFEXITED(status)){
        ptrace(PTRACE_CONT, child, 0, 0);
        waitpid(child, &status, 0);
    }



    end:
    close(fd[0]);
    close(fd[1]);

    return ret;
}
