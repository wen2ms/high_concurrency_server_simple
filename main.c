#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "server.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage:%s port path\n", argv[0]);
        return -1;
    }

    unsigned short port = atoi(argv[1]);

    chdir(argv[2]);

    int lfd = init_listen_fd(port);

    return 0;
}