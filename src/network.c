#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>

#include <netinet/in.h>

#include "common.h"

int getnumber(int fd){
    struct sockaddr_in addr;
    socklen_t len;

    len = sizeof(addr);
    getsockname(fd, (struct sockaddr *)&addr, &len);

    return htons(addr.sin_port);
}

int start_listen(int *port){
    static struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = 0x0100007f
    };

    static const int enable = 1;

    int sockfd;

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd == -1){
        return -1;
    }

    server_addr.sin_port = htons(*port);

    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
        close(sockfd);
        return -1;
    }

#ifdef SO_REUSEPORT
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0){
        close(sockfd);
        return -1;
    }
#endif


    if(bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        close(sockfd);
        return -1;
    }

    listen(sockfd, SOMAXCONN);

    if(*port == 0)
        *port = getnumber(sockfd);

    return sockfd;
}

pid_t wait_shell(int fd){
    struct pollfd pfds[2];
    char buf[1024];
    int clifd;
    pid_t pid;
    ssize_t n;

    pid = fork();

    if(pid){
        close(fd);
        return pid;
    }

    clifd = accept(fd, NULL, 0);
    if(clifd == -1){
        exit(0);
    }

    close(fd);
    good("new connection, type a command ...\n");

    pfds[0].fd = 0;
    pfds[0].events = POLLIN;
    pfds[1].fd = clifd;
    pfds[1].events = POLLIN;

    while(1){
        if(poll(pfds, 2, -1) == -1)
            break;

        if(pfds[0].revents & POLLIN){
            n = read(0, buf, sizeof(buf));
            if(n <= 0)
                break;

            send(clifd, buf, n, MSG_NOSIGNAL);
        }

        if(pfds[1].revents & POLLIN){
            n = recv(clifd, buf, sizeof(buf), MSG_NOSIGNAL);
            if(n <= 0)
                break;

            write(1, buf, n);
        }
    }



    exit(0);
}
