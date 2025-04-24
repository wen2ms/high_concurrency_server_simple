#pragma once

int init_listen_fd(unsigned short port);

int epoll_run(int lfd);

int accept_client(int lfd, int epfd);

int recv_http_request(int cfd, int epfd);