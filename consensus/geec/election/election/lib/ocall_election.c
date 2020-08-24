//
// Created by jianyu on 5/19/18.
//

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ocall_election.h"

size_t ocall_sendto(int sockfd, const char * buf, int len,
                    int flags, const char* dest_buf, int addrlen) {
    const struct sockaddr* dest = (struct sockaddr*)dest_buf;
    ssize_t ret = sendto(sockfd, buf, len, flags, dest, addrlen);
    if (ret == -1) {
        printf("port %d\n", ((struct sockaddr_in*)dest)->sin_port);
        perror("Failed to send resp");
    }
    return ret;
}

int ocall_print_string(const char* s)
{
    fprintf(stdout, "%s", s);
    return 0;
}