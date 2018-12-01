#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <grp.h>

#define xcheck(x...) if(x == -1){ \
	perror(#x); \
	exit(1); \
}

int main(void){
	xcheck(setgroups(1, (gid_t []){0xdead}))
	xcheck(setgid(0xdead));
	xcheck(setuid(0xdead));

	execve("/bin/sleep", (char *[]){"sleep", "123456", NULL}, NULL);
}
