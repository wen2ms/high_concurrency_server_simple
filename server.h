#pragma once

int init_listen_fd(unsigned short port);

int epoll_run(int lfd);

int accept_client(int lfd, int epfd);

int recv_http_request(int cfd, int epfd);

int parse_request_line(const char* line, int cfd);

int send_file(const char* file_name, int cfd);

int send_head_msg(int cfd, int status, const char* desc, const char* type, int length);

const char* get_content_type(const char* file_name);