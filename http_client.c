#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#define BUFSIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_name> <server_port> <server_path>\n", argv[0]);
        exit(1);
    }
    char *server_name = argv[1], *server_port = argv[2], *server_path = argv[3];
    struct addrinfo hints, *res, *p;
    int sockfd;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(server_name, server_port, &hints, &res) != 0) {
        perror("getaddrinfo");
        exit(1);
    }
    for (p = res; p; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) { close(sockfd); continue; }
        break;
    }
    if (!p) { fprintf(stderr, "Could not connect\n"); exit(1); }
    freeaddrinfo(res);

    char request[1024];
    snprintf(request, sizeof(request), "GET %s HTTP/1.0\r\n\r\n", server_path);
    if (send(sockfd, request, strlen(request), 0) == -1) {
        perror("send");
        exit(1);
    }

    int header_parsed = 0, success = 0;
    char buffer[BUFSIZE];
    char header_buf[BUFSIZE * 2];
    int header_buf_len = 0;
    char *header_end;
    int bytes;
    while ((bytes = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        if (!header_parsed) {
            if (header_buf_len + bytes < (int)sizeof(header_buf)) {
                memcpy(header_buf + header_buf_len, buffer, bytes);
                header_buf_len += bytes;
                header_buf[header_buf_len] = '\0';
                header_end = strstr(header_buf, "\r\n\r\n");
                if (header_end) {
                    header_parsed = 1;
                    char *line_end = strstr(header_buf, "\r\n");
                    if (line_end) {
                        *line_end = '\0';
                        int code;
                        if (sscanf(header_buf, "HTTP/%*s %d", &code) == 1)
                            success = (code == 200);
                    }
                    int header_size = header_end - header_buf + 4;
                    int body_bytes = header_buf_len - header_size;
                    if (success && body_bytes > 0)
                        fwrite(header_buf + header_size, 1, body_bytes, stdout);
                    else if (!success)
                        fwrite(header_buf, 1, header_buf_len, stderr);
                }
            } else {
                fprintf(stderr, "Header too large\n");
                close(sockfd);
                exit(1);
            }
        } else {
            if (success)
                fwrite(buffer, 1, bytes, stdout);
            else
                fwrite(buffer, 1, bytes, stderr);
        }
    }
    if (bytes < 0)
        perror("recv");
    close(sockfd);
    return success ? 0 : -1;
}
