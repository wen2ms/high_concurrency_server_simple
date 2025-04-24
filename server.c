#include "server.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/epoll.h>

int init_listen_fd(unsigned short port) {
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret == -1) {
        perror("setsocketpot");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(lfd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1) {
        perror("bind");
        return -1;
    }

    ret = listen(lfd, 128);
    if (ret == -1) {
        perror("listen");
        return -1;
    }

    return lfd;
}

int epoll_run(int lfd) {
    int epfd = epoll_create(1);
    if (epfd == -1) {
        perror("epoll_create");
        return -1;
    }

    struct epoll_event event;
    event.data.fd = lfd;
    event.events = EPOLLIN;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &event);
    if (ret == -1) {
        perror("epoll_ctl");
        return -1;
    }

    struct epoll_event events[1024];
    int size = sizeof(events) / sizeof(struct epoll_event);
    while (1) {
        int num = epoll_wait(epfd, events, size, -1);
        for (int i = 0; i < num; ++i) {
            int fd = events[i].data.fd;
            if (fd == lfd) {
                accept_client(lfd, epfd);
            } else {
                
            }
        }
    }

    return 0;
}

int accept_client(int lfd, int epfd) {
    int cfd = accept(lfd, NULL, NULL);
    if (cfd == -1) {
        perror("accept");
        return -1;
    }

    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    struct epoll_event event;
    event.data.fd = cfd;
    event.events = EPOLLIN | EPOLLET;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &event);
    if (ret == -1) {
        perror("epoll_ctl");
        return -1;
    }

    return 0;
}
