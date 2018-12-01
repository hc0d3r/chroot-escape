#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>

#include <linux/magic.h>

#include "common.h"
#include "proc.h"

uid_t user_id;

char *xreadlink(const char *file){
    char *buf = NULL;
    ssize_t n, size = 32;

    while(1){
        buf = realloc(buf, size);
        if(!buf)
            err(1, "realloc");

        n = readlink(file, buf, size);
        if(n == -1){
            free(buf);
            return NULL;
        } else if(n < size){
            buf[n] = 0x0;
            break;
        }

        size *= 2;
    }

    return buf;
}

int cant_escape(const char *procdir, pid_t pid){
    char *filepath, *filename = NULL, *linebuf = NULL, *aux;
    struct stat buf;
    size_t len = 0, msize;
    int ret = 1;
    ino_t inode;
    FILE *fh;

    msize = strlen(procdir);
    filepath = malloc(msize+16);

    sprintf(filepath, "%s/%d", procdir, pid);
    if(stat(filepath, &buf) == -1){
        bad("stat failed: %s\n", filepath);
        goto end;
    }

    if(buf.st_uid != user_id)
        goto end;

    strcat(filepath, "/exe");
    filename = xreadlink(filepath);
    if(filename == NULL){
        goto end;
    }

    if(stat(filename, &buf) == -1){
        if((aux = strstr(filename, " (deleted)"))){
            if(aux[10])
                ret = 0;
        } else {
            ret = 0;
        }
        goto end;
    }

    sprintf(filepath, "%s/%d/maps", procdir, pid);

    fh = fopen(filepath, "r");
    if(fh == NULL){
        bad("can't open %s: %s\n", filepath, strerror(errno));
        goto end;
    }

    while(getline(&linebuf, &len, fh) != -1){
        if((aux = strstr(linebuf, filename))){
            if(aux[strlen(filename)] != '\n')
                continue;

            while(*(--aux) == ' ');
            while(*(--aux) != ' ');
            aux++;

            inode = strtol(aux, NULL, 10);
            if(inode != buf.st_ino)
                ret = 0;

            break;
        }
    }

    fclose(fh);

    end:
    free(filepath);
    free(filename);
    return ret;
}

int procfs(const char *procdir){
    struct statfs buf;
    int ret = 1;

    if(statfs(procdir, &buf)){
        bad("statfs failed: %s\n", strerror(errno));
        goto end;
    }

    if(buf.f_type == PROC_SUPER_MAGIC)
        ret = 0;

    end:
    return ret;
}

int listproc(struct pidlist *out, const char *procdir, int any){
    struct dirent *entry;
    DIR *proc;
    int pid;

    out->len = 0;
    out->pids = NULL;

    if((proc = opendir(procdir)) == NULL)
        goto end;

    while((entry = readdir(proc))){
        if(entry->d_name[0] < '0' || entry->d_name[0] > '9')
            continue;

        pid = atoi(entry->d_name);
        if(!any && cant_escape(procdir, pid))
            continue;

        out->pids = realloc(out->pids, (out->len+1)*sizeof(pid_t));
        out->pids[out->len++] = pid;
    }

    closedir(proc);

    end:
    return out->len;
}
