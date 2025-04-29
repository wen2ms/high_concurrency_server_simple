#include "server.h"

#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

struct FdInfo {
    int fd;
    int epfd;
    pthread_t tid;
};

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
            struct FdInfo* info = (struct FdInfo*)malloc(sizeof(struct FdInfo));
            info->fd = fd;
            info->epfd = epfd;
            if (fd == lfd) {
                // accept_client(lfd, epfd);
                pthread_create(&info->tid, NULL, accept_client, info);
            } else {
                // recv_http_request(fd, epfd);
                pthread_create(&info->tid, NULL, recv_http_request, info);
            }
        }
    }

    return 0;
}

// int accept_client(int lfd, int epfd)
void* accept_client(void* arg) {
    struct FdInfo* info = (struct FdInfo*)arg;

    int cfd = accept(info->fd, NULL, NULL);
    if (cfd == -1) {
        perror("accept");
        return NULL;
    }

    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    struct epoll_event event;
    event.data.fd = cfd;
    event.events = EPOLLIN | EPOLLET;
    int ret = epoll_ctl(info->epfd, EPOLL_CTL_ADD, cfd, &event);
    if (ret == -1) {
        perror("epoll_ctl");
        return NULL;
    }

    printf("accpet_client thread id: %ld\n", info->tid);
    free(info);
    return NULL;
}

// int recv_http_request(int cfd, int epfd)
void* recv_http_request(void* arg) {
    struct FdInfo* info = (struct FdInfo*)arg;
    printf("Recv http request...\n");
    int len = 0, total = 0;
    char tmp[1024] = {0};
    char buf[4096] = {0};
    while ((len = recv(info->fd, tmp, sizeof(tmp), 0)) > 0) {
        if (total + len < sizeof(buf)) {
            memcpy(buf + total, tmp, len);
        }
        total += len;
    }

    if (len == -1 && errno == EAGAIN) {
        char* req_end = strstr(buf, "\r\n");
        int req_len = req_end - buf;
        buf[req_len] = '\0';
        parse_request_line(buf, info->fd);
    } else if (len == 0) {
        epoll_ctl(info->epfd, EPOLL_CTL_DEL, info->fd, NULL);
        close(info->fd);
    } else {
        perror("recv");
    }

    printf("recv_http_request thread id: %ld\n", info->tid);
    free(info);
    return NULL;
}

int parse_request_line(const char* line, int cfd) {
    char method[12];
    char path[1024];
    sscanf(line, "%[^ ] %[^ ]", method, path);
    printf("method: %s, path: %s\n", method, path);
    if (strcasecmp(method, "get") != 0) {
        return -1;
    }

    decode_msg(path, path);

    char* file = NULL;
    if (strcmp(path, "/") == 0) {
        file = "./";
    } else {
        file = path + 1;
    }

    struct stat st;
    int ret = stat(file, &st);
    if (ret == -1) {
        send_head_msg(cfd, 404, "Not Found", get_content_type(".html"), -1);
        send_file("404.html", cfd);
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        send_head_msg(cfd, 200, "OK", get_content_type(".html"), -1);
        send_dir(file, cfd);
    } else {
        send_head_msg(cfd, 200, "OK", get_content_type(file), st.st_size);
        send_file(file, cfd);
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
    lseek(fd, 0, SEEK_SET);
    off_t offset = 0;
    while (offset < size) {
        int ret = sendfile(cfd, fd, &offset, size - offset);
        printf("ret value: %d\n", ret);
        if (ret == - 1 && errno == EAGAIN) {
            printf("no data...\n");
        } else if (ret == -1) {
            perror("sendfile");
        }
    }
    close(fd);
    return 0;
}

int send_head_msg(int cfd, int status, const char* desc, const char* type, int length) {
    char buff[4096] = {0};
    sprintf(buff, "http/1.1 %d %s\r\n", status, desc);
    sprintf(buff + strlen(buff), "content-type: %s\r\n", type);
    sprintf(buff + strlen(buff), "content-length: %d\r\n\r\n", length);

    send(cfd, buff, strlen(buff), 0);
    return 0;
}

const char* get_content_type(const char* file_name) {
    const char* dot = strrchr(file_name, '.');
    if (dot == NULL) {
        return "text/plain; charset=utf-8";
    } else if (strcasecmp(dot, ".html") == 0 || strcasecmp(dot, ".htm") == 0) {
        return "text/html; charset=utf-8";
    } else if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) {
        return "image/jpeg";
    } else if (strcasecmp(dot, ".gif") == 0) {
        return "image/gif";
    } else if (strcasecmp(dot, ".png") == 0) {
        return "image/png";
    } else if (strcasecmp(dot, ".css") == 0) {
        return "text/css";
    } else if (strcasecmp(dot, ".au") == 0) {
        return "audio/basic";
    } else if (strcasecmp(dot, ".wav") == 0) {
        return "audio/wav";
    } else if (strcasecmp(dot, ".avi") == 0) {
        return "video/x-msvideo";
    } else if (strcasecmp(dot, ".midi") == 0 || strcasecmp(dot, ".mid") == 0) {
        return "audio/midi";
    } else if (strcasecmp(dot, ".mp3") == 0) {
        return "audio/mpeg";
    } else if (strcasecmp(dot, ".mov") == 0 || strcasecmp(dot, ".qt") == 0) {
        return "video/quicktime";
    } else if (strcasecmp(dot, ".mpeg") == 0 || strcasecmp(dot, ".mpe") == 0) {
        return "video/mpeg";
    } else if (strcasecmp(dot, ".vrml") == 0 || strcasecmp(dot, ".vrl") == 0) {
        return "model/vrml";
    } else if (strcasecmp(dot, ".ogg") == 0) {
        return "application/ogg";
    } else if (strcasecmp(dot, ".pac") == 0) {
        return "application/x-ns-proxy-autoconfig";
    } else {
        return "text/plain; charset=utf-8";
    }
}

int send_dir(const char* dir_name, int cfd) {
    char buff[4096] = {0};
    sprintf(buff, "<html><head><title>%s</title></head><body><table>", dir_name);

    struct dirent** namelist;
    int num = scandir(dir_name, &namelist, NULL, alphasort);
    for (int i = 0; i < num; ++i) {
        char* name = namelist[i]->d_name;
        struct stat st;
        char sub_path[1024] = {0};
        sprintf(sub_path, "%s/%s", dir_name, name);
        stat(sub_path, &st);
        if (S_ISDIR(st.st_mode)) {
            sprintf(buff + strlen(buff), "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>", name, name, st.st_size);
        } else {
            sprintf(buff + strlen(buff), "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>", name, name, st.st_size);
        }
        
        send(cfd, buff, strlen(buff), 0);
        memset(buff, 0, sizeof(buff));
        free(namelist[i]);
    }

    sprintf(buff, "</table></body></html>");
    send(cfd, buff, strlen(buff), 0);
    free(namelist);

    return 0;
}

void decode_msg(char* to, char* from) {
    for (; *from != '\0'; ++from, ++to) {
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {
            *to = hex_to_dec(from[1]) * 16 + hex_to_dec(from[2]);
            from += 2;
        } else {
            *to = *from;
        }
    }
    *to = '\0';
}

int hex_to_dec(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return 0;
}
