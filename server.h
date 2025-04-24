#pragma once

int init_listen_fd(unsigned short port);

int epoll_run(int lfd);

int accept_client(int lfd, int epfd);

int recv_http_request(int cfd, int epfd);

int parse_request_line(const char* line, int cfd);