#pragma once

int init_listen_fd(unsigned short port);

int epoll_run(int lfd);

// int accept_client(int lfd, int epfd);
void* accept_client(void* arg);

// int recv_http_request(int cfd, int epfd);
void* recv_http_request(void* arg);

int parse_request_line(const char* line, int cfd);

int send_file(const char* file_name, int cfd);

int send_head_msg(int cfd, int status, const char* desc, const char* type, int length);

const char* get_content_type(const char* file_name);

int send_dir(const char* dir_name, int cfd);

void decode_msg(char* to, char* from);

int hex_to_dec(char c);
