#ifndef __LISTPROC_H__
#define __LISTPROC_H__

#include <sys/types.h>

extern uid_t user_id;

struct pidlist {
    pid_t *pids;
    int len;
};

int procfs(const char *procdir);
int listproc(struct pidlist *out, const char *procdir, int any);

#endif
