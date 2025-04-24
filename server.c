#include "server.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/sendfile.h>

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
                recv_http_request(fd, epfd);
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

int recv_http_request(int cfd, int epfd) {
    int len = 0, total = 0;
    char tmp[1024] = {0};
    char buf[4096] = {0};
    while ((len = recv(cfd, tmp, sizeof(tmp), 0) > 0)) {
        if (total + len < sizeof(buf)) {
            memcpy(buf + total, tmp, len);
        }
        total += len;
    }

    if (len == -1 && errno == EAGAIN) {
    } else if (len == 0) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
        close(cfd);
    } else {
        perror("recv");
    }

    return 0;
}

int parse_request_line(const char* line, int cfd) {
    char method[12];
    char path[1024];
    sscanf(line, "%[^ ], %[^ ]", method, path);
    if (strcasecmp(method, "get") != 0) {
        return -1;
    }

    char* file = NULL;
    if (strcmp(path, "/") == 0) {
        file = "./";
    } else {
        file = path + 1;
    }

    struct stat st;
    int ret = stat(file, &stat);
    if (ret == -1) {
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
    } else {
    }

    return 0;
}

int send_file(const char* file_name, int cfd) {
    int fd = open(file_name, O_RDONLY);
    assert(fd > 0);
    // while (1) {
    //     char buff[1024];
    //     int len = read(fd, buff, sizeof(buff));
    //     if (len > 0) {
    //         send(cfd, buff, len, 0);

    //         usleep(10);
    //     } else if (len == 0) {
    //         break;
    //     } else {
    //         perror("read");
    //     }
    // }

    int size = lseek(fd, 0, SEEK_END);
    sendfile(cfd, fd, NULL, size);

    return 0;
}
