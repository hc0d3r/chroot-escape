#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <sys/types.h>

int start_listen(int *port);
pid_t wait_shell(int fd);


#endif
